"""Dump one exact DFTTestVK configuration, optimize its SPIR-V and record evidence.

Node creation compiles shaders/pipelines but never requests an output frame.
The optimized override is diagnostic and valid only for the recorded DLL,
dimensions, format, public parameters and DFTTESTVK environment mapping.
It is not a reusable shader cache for unrelated configurations.
"""
from __future__ import annotations

import argparse
from collections import Counter
import hashlib
import json
import os
from pathlib import Path
import re
import struct
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
MAPPING = {
    "DFTTESTVK_TILES": "16",
    "DFTTESTVK_STATIC_MAIN": "1",
    "DFTTESTVK_SHORT_REFLECT": "1",
    "DFTTESTVK_DEVICE_CONSTANTS": "1",
    "DFTTESTVK_SCALAR_SIGMA": "1",
}
PARAMS = {
    "ftype": 0, "sigma": 8.0, "sigma2": 8.0, "pmin": 0.0, "pmax": 500.0,
    "sbsize": 16, "smode": 1, "sosize": 12, "tbsize": 3, "tmode": 0, "tosize": 0,
    "swin": 0, "twin": 7, "sbeta": 2.5, "tbeta": 2.5, "zmean": 1,
    "f0beta": 1.0, "ssystem": 0, "planes": [0],
}

BOOTSTRAP = r'''
import ctypes, json, os, pathlib, sys
config = json.loads(pathlib.Path(sys.argv[1]).read_text(encoding="utf-8"))
tools_dir = pathlib.Path(config["spirv_tools"])
directory_handles = [os.add_dll_directory(str(tools_dir))] if os.name == "nt" else []
# Preload these before VS autoload can occupy the same dependency names.
library_handles = [ctypes.WinDLL(str(tools_dir / name), winmode=8) for name in (
    "libgcc_s_seh-1.dll", "libwinpthread-1.dll", "libstdc++-6.dll",
    "libSPIRV-Tools.dll", "libSPIRV-Tools-opt.dll")] if os.name == "nt" else []
import vapoursynth as vs
core = vs.core
core.num_threads = 1
core.std.LoadPlugin(path=config["dll"])
source = core.std.BlankClip(width=config["width"], height=config["height"],
                           format=vs.GRAY16, length=5, fpsnum=24, fpsden=1)
output = core.dfttestvk.DFTTest(source, **config["params"])
result = {"python": sys.version, "core": str(core), "width": output.width,
          "height": output.height, "format": output.format.name,
          "gpu_resident": output.gpu_resident, "requested_output_frames": 0,
          "plugin_path": next(p.plugin_path for p in core.plugins() if p.namespace == "dfttestvk")}
pathlib.Path(config["creation_result"]).write_text(json.dumps(result, indent=2), encoding="utf-8")
print("Created DFTTest node and dumped shaders; no output frame was requested", flush=True)
'''


def sha256(path):
    with Path(path).open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def statistics(path, assembly):
    data = Path(path).read_bytes()
    if len(data) < 20 or len(data) % 4:
        raise ValueError(f"Invalid SPIR-V byte length: {path}")
    magic, version, generator, bound, schema = struct.unpack_from("<5I", data)
    if magic != 0x07230203:
        raise ValueError(f"Invalid SPIR-V magic: {path}")
    opcodes = Counter(re.findall(r"\b(Op\w+)\b", "\n".join(
        line.split(";", 1)[0] for line in assembly.splitlines())))
    arrays = {}
    pointers = {}
    constants = {}
    names = {}
    variables = []
    for line in assembly.splitlines():
        if match := re.match(r'\s*OpName\s+(%\S+)\s+"(.*)"', line):
            names[match[1]] = match[2]
        if match := re.match(r"\s*(%\S+)\s*=\s*(OpTypeArray|OpTypeRuntimeArray)\s+(.*)", line):
            arrays[match[1]] = {"opcode": match[2], "operands": match[3].split()}
        if match := re.match(r"\s*(%\S+)\s*=\s*OpTypePointer\s+Function\s+(%\S+)", line):
            pointers[match[1]] = match[2]
        if match := re.match(r"\s*(%\S+)\s*=\s*OpConstant\s+%\S+\s+(\S+)", line):
            constants[match[1]] = match[2]
        if match := re.match(r"\s*(%\S+)\s*=\s*OpVariable\s+(%\S+)\s+Function\b", line):
            variables.append((match[1], match[2]))
    function_arrays = []
    for identifier, pointer in variables:
        target = pointers.get(pointer)
        if target in arrays:
            definition = arrays[target]
            operands = definition["operands"]
            function_arrays.append({"id": identifier, "name": names.get(identifier),
                                    "pointer_type": pointer, "array_type": target,
                                    "element_type": operands[0],
                                    "length": constants.get(operands[1], operands[1]) if len(operands) > 1 else None})
    return {"path": str(Path(path).resolve()), "sha256": sha256(path),
            "bytes": len(data), "words": len(data) // 4,
            "spirv_version": f"{(version >> 16) & 255}.{(version >> 8) & 255}",
            "generator": generator, "id_bound": bound, "schema": schema,
            "instruction_count": sum(opcodes.values()), "opcode_counts": dict(sorted(opcodes.items())),
            "OpFDiv": opcodes["OpFDiv"], "function_variables": len(variables),
            "typed_function_arrays_count": len(function_arrays), "typed_function_arrays": function_arrays}


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--dll", type=Path, default=ROOT / "build/dfttestvk.dll")
    ap.add_argument("--spirv-tools", type=Path, default=ROOT / "dependencies/validation_layers/mingw64/bin")
    ap.add_argument("--output-dir", type=Path, required=True, help="new evidence directory")
    args = ap.parse_args()
    dll, tools_dir, output_dir = args.dll.resolve(), args.spirv_tools.resolve(), args.output_dir.resolve()
    if output_dir.exists():
        ap.error("Use a new output directory; shader evidence is never overwritten")
    if not dll.is_file():
        ap.error(f"Build the diagnostic DLL first: {dll}")
    tool_paths = {name: tools_dir / (name + (".exe" if os.name == "nt" else ""))
                  for name in ("spirv-opt", "spirv-val", "spirv-dis")}
    if missing := [str(path) for path in tool_paths.values() if not path.is_file()]:
        ap.error("Missing SPIRV-Tools executables: " + ", ".join(missing))
    output_dir.mkdir(parents=True)
    raw_dir, optimized_dir = output_dir / "raw", output_dir / "optimized"
    raw_dir.mkdir()
    optimized_dir.mkdir()
    config = {"dll": str(dll), "dll_sha256": sha256(dll), "width": 1920, "height": 1080,
              "format": "GRAY16", "source_frames": 5, "params": PARAMS,
              "environment_mapping": MAPPING, "spirv_tools": str(tools_dir),
              "creation_result": str(output_dir / "creation.json")}
    config_path = output_dir / "exact-config.json"
    config_path.write_text(json.dumps(config, indent=2), encoding="utf-8")
    env = {key: value for key, value in os.environ.items() if not key.startswith("DFTTESTVK_")}
    # Do not inherit diagnostic validation/profiling settings into a driver
    # compilation experiment. Machine-wide configuration is never changed.
    for key in ("VS_VULKAN_VALIDATION", "VK_LAYER_ENABLES", "VK_LAYER_SETTINGS_PATH", "VK_LOADER_DEBUG", "VK_INSTANCE_LAYERS"):
        env.pop(key, None)
    env.update(MAPPING, DFTTESTVK_DUMP_SPIRV=str(raw_dir))
    env["PATH"] = str(tools_dir) + os.pathsep + env.get("PATH", "")
    commands = []
    def run(command, name):
        started = time.perf_counter()
        completed = subprocess.run(command, env=env, capture_output=True, text=True, encoding="utf-8", errors="replace")
        log = output_dir / (name + ".log")
        log.write_text(completed.stdout + completed.stderr, encoding="utf-8")
        commands.append({"command": command, "seconds": time.perf_counter() - started,
                         "returncode": completed.returncode, "log": str(log)})
        if completed.returncode:
            raise RuntimeError(f"{name} failed ({completed.returncode}); inspect {log}")
        return completed.stdout
    result = {"config": config, "commands": commands, "kernels": {},
              "tools_sha256": {str(path): sha256(path) for path in tool_paths.values()},
              "override_contract": "Use DFTTESTVK_LOAD_SPIRV only with this exact DLL, GRAY16 1920x1080, recorded parameters and mapping. No runtime safety check enforces that contract.",
              "requested_output_frames": 0, "passed": False}
    try:
        run([sys.executable, "-c", BOOTSTRAP, str(config_path)], "dump-node-creation")
        for kernel in ("fused", "gather"):
            raw, optimized = raw_dir / (kernel + ".spv"), optimized_dir / (kernel + ".spv")
            if not raw.is_file():
                raise FileNotFoundError(f"Diagnostic DLL did not dump {raw}")
            run([str(tool_paths["spirv-val"]), "--target-env", "vulkan1.4", str(raw)], kernel + "-raw-val")
            run([str(tool_paths["spirv-opt"]), "-O", "--target-env=vulkan1.4", str(raw), "-o", str(optimized)], kernel + "-opt")
            run([str(tool_paths["spirv-val"]), "--target-env", "vulkan1.4", str(optimized)], kernel + "-optimized-val")
            row = {}
            for label, path in (("raw", raw), ("optimized", optimized)):
                assembly = run([str(tool_paths["spirv-dis"]), "--raw-id", str(path)], kernel + "-" + label + "-dis")
                path.with_suffix(".spvasm").write_text(assembly, encoding="utf-8")
                row[label] = statistics(path, assembly)
            row["words_change"] = row["optimized"]["words"] - row["raw"]["words"]
            result["kernels"][kernel] = row
        result["creation"] = json.loads(Path(config["creation_result"]).read_text(encoding="utf-8"))
        result["override_environment"] = {**MAPPING, "DFTTESTVK_LOAD_SPIRV": str(optimized_dir)}
        result["passed"] = True
    except Exception as exc:
        result["error"] = str(exc)
    finally:
        (output_dir / "summary.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
    print(json.dumps({"passed": result["passed"], "summary": str(output_dir / "summary.json"),
                      "error": result.get("error"), "kernels": {name: {label: {key: data[key] for key in
                      ("words", "OpFDiv", "typed_function_arrays_count")} for label, data in row.items()
                      if label in ("raw", "optimized")} for name, row in result["kernels"].items()}}, indent=2))
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    sys.exit(main())
