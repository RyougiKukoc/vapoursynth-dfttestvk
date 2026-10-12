"""Patch only OpFDiv result decorations with AllowRecip|AllowContract.

The input directory is preserved. A new output directory contains byte-exact
input copies, patched .spv files suitable for the exact diagnostic override,
validation/disassembly logs and a JSON report. No shader or GPU is executed.

FloatControls2 requires explicit defaults instead of mixing decorated flags
with client-API defaults. Defaults are zero: no NotNaN, NotInf, NSZ, reassociation
or transform permissions are introduced. This may make unrelated arithmetic
stricter than its former Vulkan defaults and is recorded in the report.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[1]
MAGIC = 0x07230203
DIV_FLAGS = 0x0008 | 0x10000
FLOAT_CONTROLS2 = 6029
DEFAULT_MODE = 6028
EXTENSION = "SPV_KHR_float_controls2"


def instruction(opcode: int, *operands: int) -> list[int]:
    return [((len(operands) + 1) << 16) | opcode, *operands]


def string_words(value: str) -> list[int]:
    data = value.encode("utf-8") + b"\0"
    data += b"\0" * ((-len(data)) % 4)
    return list(struct.unpack(f"<{len(data)//4}I", data))


def decode_string(words: list[int]) -> str:
    return struct.pack(f"<{len(words)}I", *words).split(b"\0", 1)[0].decode("utf-8")


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def patch(data: bytes, default_only: bool = False) -> tuple[bytes, dict]:
    if len(data) < 20 or len(data) % 4:
        raise ValueError("Invalid SPIR-V byte length")
    words = list(struct.unpack(f"<{len(data)//4}I", data))
    header = words[:5]
    if header[0] != MAGIC or header[1] != 0x00010600:
        raise ValueError("This diagnostic requires little-endian SPIR-V 1.6")
    instructions = []
    offset = 5
    while offset < len(words):
        count = words[offset] >> 16
        if count == 0 or offset + count > len(words):
            raise ValueError(f"Invalid instruction at word {offset}")
        instructions.append(words[offset:offset+count])
        offset += count
    divisions = [(ins[2], ins[1]) for ins in instructions if ins[0] & 0xffff == 136]
    report = {"source_sha256": sha256(data), "source_bytes": len(data),
              "division_result_ids": [identifier for identifier, _ in divisions],
              "flags_added": {} if default_only else {"AllowRecip": "0x0008", "AllowContract": "0x10000"},
              "default_only_control": default_only,
              "default_flags": 0, "decorated_results": [], "new_default_modes": [],
              "added_capability": False, "added_extension": False,
              "not_nan_not_inf_nsz_added": False}
    if not divisions:
        report.update(output_sha256=sha256(data), output_bytes=len(data), unchanged=True)
        return data, report
    float_types = {ins[1] for ins in instructions if ins[0] & 0xffff == 22}
    uint_types = [ins[1] for ins in instructions if ins[0] & 0xffff == 21 and ins[2:] == [32, 0]]
    constants = {ins[2]: (ins[1], ins[3:]) for ins in instructions if ins[0] & 0xffff == 43}
    entries = [ins[2] for ins in instructions if ins[0] & 0xffff == 15]
    existing_modes = {}
    decorations = {}
    for index, ins in enumerate(instructions):
        op = ins[0] & 0xffff
        if op in (16, 331) and ins[2] in (31, 4461):
            raise ValueError("ContractionOff/SignedZeroInfNanPreserve conflicts with explicit FloatControls2 defaults")
        if op == 71:
            if ins[2] == 42:
                raise ValueError(f"NoContraction on %{ins[1]} conflicts with explicit FloatControls2 defaults; it is not removed")
            if ins[2] == 40:
                if ins[1] in decorations:
                    raise ValueError(f"Duplicate existing FPFastMathMode for %{ins[1]}")
                if ins[3] & 0x0010:
                    raise ValueError("Existing Fast bit conflicts with FloatControls2 explicit defaults")
                decorations[ins[1]] = index
        if op in (16, 331) and ins[2] == DEFAULT_MODE:
            if op != 331:
                raise ValueError("FPFastMathDefault requires OpExecutionModeId")
            key = (ins[1], ins[3])
            if key in existing_modes:
                raise ValueError(f"Duplicate existing FPFastMathDefault for {key}")
            constant = constants.get(ins[4])
            if constant is None or constant[1] != [0]:
                raise ValueError("Existing nonzero default flags are preserved by refusing this division-only diagnostic")
            existing_modes[key] = ins[4]
    pending = {}

    def insert_before(index: int, values: list[list[int]]) -> None:
        pending.setdefault(index, []).extend(values)

    cap_end = next((i for i, ins in enumerate(instructions) if ins[0] & 0xffff != 17), len(instructions))
    if not any(ins[0] & 0xffff == 17 and ins[1] == FLOAT_CONTROLS2 for ins in instructions):
        insert_before(cap_end, [instruction(17, FLOAT_CONTROLS2)])
        report["added_capability"] = True
    if not any(ins[0] & 0xffff == 10 and decode_string(ins[1:]) == EXTENSION for ins in instructions):
        extension_end = cap_end
        while extension_end < len(instructions) and instructions[extension_end][0] & 0xffff == 10:
            extension_end += 1
        insert_before(extension_end, [instruction(10, *string_words(EXTENSION))])
        report["added_extension"] = True
    first_type = next(i for i, ins in enumerate(instructions) if ins[0] & 0xffff in range(19, 40))
    first_annotation = next((i for i, ins in enumerate(instructions) if ins[0] & 0xffff in (71, 72, 73, 74, 75, 332)), first_type)
    first_function = next(i for i, ins in enumerate(instructions) if ins[0] & 0xffff == 54)

    def new_id() -> int:
        value = header[3]
        header[3] += 1
        return value

    if uint_types:
        uint_type = uint_types[0]
    else:
        uint_type = new_id()
        insert_before(first_type, [instruction(21, uint_type, 32, 0)])
    zero_id = next((identifier for identifier, value in constants.items()
                    if value == (uint_type, [0])), None)
    if zero_id is None:
        zero_id = new_id()
        insert_before(first_function, [instruction(43, uint_type, zero_id, 0)])
    execution_end = max(i for i, ins in enumerate(instructions) if ins[0] & 0xffff in (15, 16, 331)) + 1
    for entry in entries:
        for float_type in sorted(float_types):
            if (entry, float_type) not in existing_modes:
                insert_before(execution_end, [instruction(331, entry, DEFAULT_MODE, float_type, zero_id)])
                report["new_default_modes"].append({"entry_point_id": entry, "float_type_id": float_type, "flags": 0})
    new_decorations = []
    for identifier, result_type in ([] if default_only else divisions):
        if identifier in decorations:
            index = decorations[identifier]
            old = instructions[index][3]
            instructions[index] = instruction(71, identifier, 40, old | DIV_FLAGS)
        else:
            old = 0
            new_decorations.append(instruction(71, identifier, 40, DIV_FLAGS))
        report["decorated_results"].append({"result_id": identifier, "result_type_id": result_type,
                                             "prior_flags": old, "output_flags": old | DIV_FLAGS,
                                             "new_decoration": identifier not in decorations})
    insert_before(first_annotation, new_decorations)
    output_words = list(header)
    for index, ins in enumerate(instructions):
        for addition in pending.get(index, []):
            output_words.extend(addition)
        output_words.extend(ins)
    output = struct.pack(f"<{len(output_words)}I", *output_words)
    report.update(output_sha256=sha256(output), output_bytes=len(output), unchanged=output == data)
    return output, report


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input-dir", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True, help="Must not exist")
    parser.add_argument("--spirv-tools", type=Path, default=ROOT / "dependencies/validation_layers/mingw64/bin")
    parser.add_argument("--default-only", action="store_true", help="Create a matched control with zero defaults but no added division permissions")
    args = parser.parse_args()
    if args.output_dir.exists():
        parser.error("Use a new output directory; input/previous diagnostics are never overwritten")
    inputs = sorted(args.input_dir.glob("*.spv"))
    if not inputs:
        parser.error("Input directory has no .spv files")
    tool_suffix = ".exe" if os.name == "nt" else ""
    tools = {name: args.spirv_tools / (name + tool_suffix) for name in ("spirv-val", "spirv-dis")}
    if not all(path.is_file() for path in tools.values()):
        parser.error("Missing spirv-val/spirv-dis; choose --spirv-tools")
    args.output_dir.mkdir(parents=True)
    originals = args.output_dir / "input"
    originals.mkdir()
    report = {"input_directory": str(args.input_dir.resolve()), "output_directory": str(args.output_dir.resolve()),
              "gpu_execution": False, "target_env": "vulkan1.4", "modules": {}, "commands": [],
              "specification": "https://github.com/KhronosGroup/SPIRV-Registry/blob/main/extensions/KHR/SPV_KHR_float_controls2.asciidoc",
              "limitation": "FPFastMathDefault=0 may tighten nondivision arithmetic relative to old Vulkan defaults. AllowRecip alone does not require NotNaN/NotInf; neither is added. No accuracy/performance conclusion follows from validation.",
              "override_contract": "Only use these SPIR-V files with the original exact DLL/configuration recorded by the source dump; no runtime matching is enforced.",
              "passed": False}

    def run(command: list[str], log_name: str) -> None:
        completed = subprocess.run(command, capture_output=True, text=True, encoding="utf-8", errors="replace")
        (args.output_dir / log_name).write_text(completed.stdout + completed.stderr, encoding="utf-8")
        report["commands"].append({"command": command, "returncode": completed.returncode, "log": log_name})
        if completed.returncode:
            raise RuntimeError(f"Validation/disassembly failed; inspect {log_name}")

    try:
        for source in inputs:
            data = source.read_bytes()
            (originals / source.name).write_bytes(data)
            output, item = patch(data, args.default_only)
            target = args.output_dir / source.name
            target.write_bytes(output)
            report["modules"][source.name] = item
            run([str(tools["spirv-val"]), "--target-env", "vulkan1.4", str(target)], source.stem + "-validation.log")
            run([str(tools["spirv-dis"]), "--raw-id", str(target), "-o", str(target.with_suffix(".spvasm"))], source.stem + "-disassembly.log")
            print(f"{source.name}: decorated {len(item['decorated_results'])} OpFDiv results; Vulkan 1.4 validation passed", flush=True)
        exact_config = args.input_dir.parent / "exact-config.json"
        if exact_config.is_file():
            shutil.copyfile(exact_config, args.output_dir / "exact-config.source.json")
        report["passed"] = True
    finally:
        (args.output_dir / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
