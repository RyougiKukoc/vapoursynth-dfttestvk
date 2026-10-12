"""Export complete native Vulkan fixture outputs for a platform/device comparison.

NPZ arrays retain their actual sample dtype and bytes. This is a small numerical
comparison fixture, not a benchmark or a general bit-exactness claim.
"""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import platform
import sys
import traceback

import numpy as np

ROOT = Path(__file__).resolve().parents[2]
SCHEMA = "dfttestvk.fixture.v1"


def array_sha256(array: np.ndarray) -> str:
    descriptor = json.dumps({"dtype": array.dtype.str, "shape": list(array.shape)},
                            sort_keys=True, separators=(",", ":")).encode("utf-8")
    digest = hashlib.sha256(descriptor + b"\n")
    digest.update(np.ascontiguousarray(array).tobytes(order="C"))
    return digest.hexdigest()


def file_sha256(path: Path) -> str:
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def cases() -> list[dict]:
    result = []
    for fmt in ("gray16", "grays"):
        for temporal in (1, 3, 5, 7):
            result.append({"name": f"{fmt}_t{temporal}_twin7", "format": fmt,
                           "params": {"sbsize": 16, "sosize": 12, "tbsize": temporal,
                                      "swin": 0, "twin": 7, "ftype": 0, "zmean": True,
                                      "sigma": 8.0, "f0beta": 1.0}})
        result.append({"name": f"{fmt}_t3_twin0", "format": fmt,
                       "params": {"sbsize": 16, "sosize": 12, "tbsize": 3,
                                  "swin": 0, "twin": 0, "ftype": 0, "zmean": True,
                                  "sigma": 8.0, "f0beta": 1.0}})
    return result


def load_canonical_source(npz: Path, fixture_description: dict) -> tuple[dict, dict]:
    """Verify the complete source arrays of a successful exported artifact."""
    npz = npz.resolve(strict=True)
    metadata = npz.with_suffix(".json")
    record = json.loads(metadata.read_text(encoding="utf-8"))
    if record.get("schema") != SCHEMA or record.get("passed") is not True:
        raise ValueError("Canonical origin is not a successful fixture artifact")
    if record.get("fixture") != fixture_description:
        raise ValueError("Canonical fixture dimensions/length/pattern/seed do not match")
    npz_hash = file_sha256(npz)
    if record.get("output_npz_sha256") != npz_hash:
        raise ValueError("Canonical NPZ hash differs from its metadata")
    expected_keys = {f"SRC_{fmt}_f{n}_p0" for fmt in ("gray16", "grays")
                     for n in range(fixture_description["length"])}
    descriptions = record.get("arrays", {})
    source_keys = {key for key, value in descriptions.items() if value.get("kind") == "source"}
    if source_keys != expected_keys:
        raise ValueError("Canonical metadata must contain exactly the 10 required source arrays")
    result = {}
    with np.load(npz, allow_pickle=False) as archive:
        if set(archive.files) != set(descriptions) or len(archive.files) != 60 or len(record.get("cases", [])) != 10:
            raise ValueError("Canonical origin does not contain the complete 10-case fixture artifact")
        if {key for key in archive.files if key.startswith("SRC_")} != expected_keys:
            raise ValueError("Canonical NPZ source keys are missing or unexpected")
        for fmt, dtype in (("gray16", np.dtype(np.uint16)), ("grays", np.dtype(np.float32))):
            frames = []
            for n in range(fixture_description["length"]):
                key = f"SRC_{fmt}_f{n}_p0"
                array = archive[key]
                description = descriptions[key]
                shape = (fixture_description["height"], fixture_description["width"])
                if array.dtype.str != dtype.str or array.shape != shape:
                    raise ValueError("Canonical source dtype/shape does not match: " + key)
                if description.get("dtype") != array.dtype.str or description.get("shape") != list(shape):
                    raise ValueError("Canonical metadata dtype/shape does not match: " + key)
                if (description.get("format"), description.get("frame"), description.get("plane")) != (fmt, n, 0):
                    raise ValueError("Canonical source semantics do not match: " + key)
                if description.get("sha256") != array_sha256(array):
                    raise ValueError("Canonical source bytes differ from metadata: " + key)
                if not np.isfinite(array).all():
                    raise ValueError("Canonical source contains nonfinite samples: " + key)
                frames.append([array.copy(order="C")])
            result[fmt] = frames
    origin = {"npz": str(npz), "npz_sha256": npz_hash,
              "metadata": str(metadata), "metadata_sha256": file_sha256(metadata)}
    return result, origin


def canonical_fixture(core, fmt, arrays, fixture_description, format_id):
    """Construct native frames from canonical pixels without generating a local source."""
    blank = core.std.BlankClip(width=fixture_description["width"], height=fixture_description["height"],
                               length=fixture_description["length"], format=format_id,
                               fpsnum=24, fpsden=1)

    def supply(n, f):
        output = f.copy()
        for plane, array in enumerate(arrays[n]):
            destination = np.asarray(output[plane])
            if destination.dtype != array.dtype or destination.shape != array.shape:
                raise RuntimeError(f"Canonical frame storage differs: {fmt} frame {n} plane {plane}")
            np.copyto(destination, array, casting="no")
        output.props["DFTTestFixtureFrame"] = n
        output.props["DFTTestFixtureSeed"] = fixture_description["seed"]
        output.props["DFTTestFixturePattern"] = fixture_description["pattern"].encode()
        output.props["_DurationNum"] = 1
        output.props["_DurationDen"] = 24
        return output

    return core.std.ModifyFrame(blank, clips=blank, selector=supply)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dll", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True, help="new NPZ filename")
    parser.add_argument("--metadata", type=Path, help="default: output filename with .json suffix")
    parser.add_argument("--device-uuid", help="select via tests/common.py before device initialization")
    parser.add_argument("--source-npz", type=Path,
                        help="reuse verified SRC arrays from another platform's NPZ plus same-name JSON")
    args = parser.parse_args()
    if args.output.suffix.lower() != ".npz":
        parser.error("--output must have the .npz suffix")
    output = args.output.resolve()
    metadata = (args.metadata or args.output.with_suffix(".json")).resolve()
    if output == metadata or output.exists() or metadata.exists():
        parser.error("Use new and distinct NPZ/metadata filenames")
    if args.device_uuid:
        os.environ["DFTTESTVK_DEVICE_UUID"] = args.device_uuid
        os.environ.pop("DFTTESTVK_DEVICE", None)
    report = {"schema": SCHEMA, "created_utc": datetime.now(timezone.utc).isoformat(),
              "scope": "10 deterministic Gray16/GrayS cases, all 5 frames and samples",
              "performance_verified": False, "python": sys.version,
              "platform": platform.platform(), "libc": platform.libc_ver(),
              "numpy": np.__version__, "output_npz": str(output),
              "fixture": {"width": 80, "height": 64, "length": 5,
                          "pattern": "noise", "seed": 20261012},
              "source_mode": "canonical_npz" if args.source_npz else "generated",
              "array_hash_encoding": "SHA256(canonical JSON dtype.str/shape + LF + C-order native bytes)",
              "bit_exact_definition": "matching dtype, shape and all original sample bytes",
              "numeric_equality_definition": "numpy.array_equal; +0 and -0 compare equal",
              "arrays": {}, "cases": [], "passed": False}
    arrays = {}
    try:
        # Reject invalid canonical artifacts before importing VapourSynth or
        # creating any Vulkan device.
        canonical_arrays = None
        if args.source_npz:
            canonical_arrays, report["source_origin"] = load_canonical_source(args.source_npz, report["fixture"])
        sys.path.insert(0, str(ROOT / "tests"))
        import vapoursynth as vs
        from common import FORMATS, environment, fixture, load_native, make_filter
        core = vs.core
        core.num_threads = 8
        loaded = load_native(core, args.dll)
        report["plugin"] = {"path": str(loaded), "sha256": file_sha256(loaded)}
        report["vapoursynth_version"] = str(vs.__version__)
        report["vulkan_device_info"] = core.vulkan_device_info
        report["implementation_source_sha256"] = {
            str(path.relative_to(ROOT)): file_sha256(path)
            for path in sorted((ROOT / "src").glob("*")) if path.is_file()}
        report["fixture_helpers_sha256"] = {
            "tests/common.py": file_sha256(ROOT / "tests/common.py"),
            "tools/remote/export_fixture.py": file_sha256(Path(__file__).resolve())}
        sources, source_keys = {}, {}

        def save_array(key, array, **description):
            if key in arrays:
                raise RuntimeError("Duplicate fixture array key: " + key)
            array = np.asarray(array).copy(order="C")
            if not np.isfinite(array).all():
                raise RuntimeError("Nonfinite fixture samples: " + key)
            arrays[key] = array
            report["arrays"][key] = {"dtype": array.dtype.str, "shape": list(array.shape),
                                     "sha256": array_sha256(array), **description}

        for fmt in ("gray16", "grays"):
            if canonical_arrays is None:
                source, expected = fixture(core, fmt=fmt, **report["fixture"])
            else:
                expected = canonical_arrays[fmt]
                source = canonical_fixture(core, fmt, expected, report["fixture"], FORMATS[fmt])
            sources[fmt] = source
            source_keys[fmt] = []
            for n in range(source.num_frames):
                with source.get_frame(n) as frame:
                    for plane in range(frame.format.num_planes):
                        actual = np.asarray(frame[plane])
                        if actual.dtype != expected[n][plane].dtype or actual.tobytes() != expected[n][plane].tobytes():
                            raise RuntimeError(f"Fixture storage changed source bytes: {fmt} frame {n} plane {plane}")
                        key = f"SRC_{fmt}_f{n}_p{plane}"
                        save_array(key, actual, kind="source", format=fmt, frame=n, plane=plane)
                        source_keys[fmt].append(key)
        for case in cases():
            source = sources[case["format"]]
            node = make_filter(core, source, "vk", case["params"], loaded)
            core.std.SetVideoCache(node, mode=0)
            row = {**case, "source_keys": source_keys[case["format"]], "output_keys": [],
                   "properties_preserved": True}
            for n in range(node.num_frames):
                with node.get_frame(n) as frame, source.get_frame(n) as source_frame:
                    if dict(frame.props) != dict(source_frame.props):
                        raise RuntimeError(f"Output properties changed: {case['name']} frame {n}")
                    for plane in range(frame.format.num_planes):
                        key = f"OUT_{case['name']}_f{n}_p{plane}"
                        save_array(key, np.asarray(frame[plane]), kind="output", format=case["format"],
                                   case=case["name"], frame=n, plane=plane)
                        row["output_keys"].append(key)
            report["cases"].append(row)
            print("EXPORTED " + case["name"], flush=True)
        if len(report["cases"]) != 10 or len(arrays) != 60:
            raise RuntimeError("Fixture coverage is incomplete")
        output.parent.mkdir(parents=True, exist_ok=True)
        with output.open("xb") as stream:
            np.savez(stream, **arrays)
        report["output_npz_sha256"] = file_sha256(output)
        report["environment"] = environment(core)
        report["passed"] = True
    except Exception as error:
        report.update(error=f"{type(error).__name__}: {error}", traceback=traceback.format_exc())
    metadata.parent.mkdir(parents=True, exist_ok=True)
    with metadata.open("x", encoding="utf-8") as stream:
        json.dump(report, stream, ensure_ascii=False, indent=2)
        stream.write("\n")
    print(json.dumps({"passed": report["passed"], "cases": len(report["cases"]),
                      "npz": str(output), "metadata": str(metadata), "error": report.get("error")}), flush=True)
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
