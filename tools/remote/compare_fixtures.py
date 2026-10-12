"""Compare complete exported fixtures, requiring identical sources and semantics.

Both numerical equality and original sample-bit equality are reported. A valid
comparison may find differences; --require-bit-exact makes those an exit failure.
This tool imports neither VapourSynth nor GPU APIs.
"""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import sys
import traceback

import numpy as np

SCHEMA = "dfttestvk.fixture.v1"


def sha256(path: Path) -> str:
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def array_sha256(array: np.ndarray) -> str:
    descriptor = json.dumps({"dtype": array.dtype.str, "shape": list(array.shape)},
                            sort_keys=True, separators=(",", ":")).encode("utf-8")
    digest = hashlib.sha256(descriptor + b"\n")
    digest.update(np.ascontiguousarray(array).tobytes(order="C"))
    return digest.hexdigest()


def load_artifact(npz: Path, metadata: Path) -> tuple[dict, dict[str, np.ndarray]]:
    record = json.loads(metadata.read_text(encoding="utf-8"))
    if record.get("schema") != SCHEMA or record.get("passed") is not True:
        raise ValueError("Invalid or incomplete fixture metadata: " + str(metadata))
    if sha256(npz) != record.get("output_npz_sha256"):
        raise ValueError("NPZ hash differs from metadata: " + str(npz))
    with np.load(npz, allow_pickle=False) as archive:
        arrays = {key: archive[key] for key in archive.files}
    if set(arrays) != set(record.get("arrays", {})):
        raise ValueError("NPZ keys differ from metadata: " + str(npz))
    if len(record.get("cases", [])) != 10 or len(arrays) != 60:
        raise ValueError("Fixture must contain all 10 cases and 60 source/output arrays")
    for key, array in arrays.items():
        description = record["arrays"][key]
        if array.dtype.str != description["dtype"] or list(array.shape) != description["shape"]:
            raise ValueError("Sample dtype/shape differs from metadata: " + key)
        if array_sha256(array) != description["sha256"]:
            raise ValueError("Sample bytes differ from metadata: " + key)
        if not np.isfinite(array).all():
            raise ValueError("Nonfinite samples: " + key)
    return record, arrays


def measure(left: np.ndarray, right: np.ndarray) -> dict:
    left = np.ascontiguousarray(left)
    right = np.ascontiguousarray(right)
    left_bytes = left.view(np.uint8).reshape(left.size, left.dtype.itemsize)
    right_bytes = right.view(np.uint8).reshape(right.size, right.dtype.itemsize)
    xor = np.bitwise_xor(left_bytes, right_bytes)
    raw_samples = int(np.count_nonzero(np.any(xor != 0, axis=1)))
    raw_bits = int(np.unpackbits(xor).sum())
    numeric_samples = int(np.count_nonzero(left != right))
    delta = left.astype(np.float64) - right.astype(np.float64)
    square_sum = float(np.sum(delta * delta, dtype=np.float64))
    maximum = float(np.max(np.abs(delta), initial=0))
    return {"samples": int(left.size), "sample_bits": int(left.dtype.itemsize * 8),
            "numeric_array_equal": bool(np.array_equal(left, right)),
            "bit_exact": raw_samples == 0, "numeric_different_samples": numeric_samples,
            "numeric_different_fraction": numeric_samples / left.size,
            "raw_different_samples": raw_samples, "raw_different_sample_fraction": raw_samples / left.size,
            "raw_different_bits": raw_bits, "raw_bit_different_fraction": raw_bits / (left.nbytes * 8),
            "max_absolute_difference": maximum, "rms": float(np.sqrt(square_sum / left.size)),
            "squared_error_sum": square_sum,
            "difference_unit": "native integer code" if np.issubdtype(left.dtype, np.integer) else "native float sample"}


def aggregate(rows: list[dict]) -> dict:
    count = sum(row["samples"] for row in rows)
    bits = sum(row["samples"] * row["sample_bits"] for row in rows)
    numeric = sum(row["numeric_different_samples"] for row in rows)
    raw = sum(row["raw_different_samples"] for row in rows)
    raw_bits = sum(row["raw_different_bits"] for row in rows)
    return {"arrays": len(rows), "samples": count,
            "numeric_array_equal": numeric == 0, "bit_exact": raw == 0,
            "numeric_different_samples": numeric, "numeric_different_fraction": numeric / count,
            "raw_different_samples": raw, "raw_different_sample_fraction": raw / count,
            "raw_different_bits": raw_bits, "raw_bit_different_fraction": raw_bits / bits,
            "max_absolute_difference": max(row["max_absolute_difference"] for row in rows),
            "rms": float(np.sqrt(sum(row["squared_error_sum"] for row in rows) / count))}


def source_semantics(record: dict) -> dict:
    fields = {}
    for field in ("fixture", "cases", "implementation_source_sha256", "array_hash_encoding"):
        value = record.get(field)
        if field in ("implementation_source_sha256", "fixture_helpers_sha256") and isinstance(value, dict):
            value = {str(key).replace("\\", "/"): item for key, item in value.items()}
        fields[field] = value
    return fields


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--left", type=Path, required=True)
    parser.add_argument("--right", type=Path, required=True)
    parser.add_argument("--left-metadata", type=Path)
    parser.add_argument("--right-metadata", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--require-bit-exact", action="store_true")
    args = parser.parse_args()
    if args.output.exists():
        parser.error("Use a new output report filename")
    left_metadata = args.left_metadata or args.left.with_suffix(".json")
    right_metadata = args.right_metadata or args.right.with_suffix(".json")
    report = {"created_utc": datetime.now(timezone.utc).isoformat(),
              "scope": "the complete 10 exported deterministic cases; no wider bit-exactness guarantee",
              "comparison_valid": False, "performance_verified": False,
              "require_bit_exact": args.require_bit_exact,
              "left_npz": str(args.left.resolve()), "right_npz": str(args.right.resolve())}
    try:
        left_record, left = load_artifact(args.left, left_metadata)
        right_record, right = load_artifact(args.right, right_metadata)
        left_semantics, right_semantics = source_semantics(left_record), source_semantics(right_record)
        if left_semantics != right_semantics:
            for field in left_semantics:
                if left_semantics[field] != right_semantics.get(field):
                    raise ValueError("Fixture semantics/source implementation differ: " + field)
            raise ValueError("Fixture semantics/source implementation differ")
        if set(left) != set(right):
            raise ValueError("Fixture array keys differ")
        sources = []
        outputs = []
        for key in sorted(left):
            description = left_record["arrays"][key]
            other = right_record["arrays"][key]
            semantic = {k: v for k, v in description.items() if k != "sha256"}
            if semantic != {k: v for k, v in other.items() if k != "sha256"}:
                raise ValueError("Array semantics/dtype/shape differ: " + key)
            if left[key].dtype != right[key].dtype or left[key].shape != right[key].shape:
                raise ValueError("Array dtype/shape differ: " + key)
            if description["kind"] == "source":
                if description["sha256"] != other["sha256"]:
                    raise ValueError("Source sample bytes differ; output comparison is invalid: " + key)
                sources.append(key)
            elif description["kind"] == "output":
                outputs.append({"key": key, "case": description["case"], "format": description["format"],
                                "frame": description["frame"], "plane": description["plane"],
                                "dtype": description["dtype"], "shape": description["shape"],
                                **measure(left[key], right[key])})
            else:
                raise ValueError("Unknown array kind: " + key)
        if len(sources) != 10 or len(outputs) != 50:
            raise ValueError("Source/output array coverage differs from the required complete fixture")
        report["artifacts"] = {
            side: {"npz_sha256": sha256(npz), "metadata_sha256": sha256(metadata),
                   "plugin": record.get("plugin"), "core": record.get("environment", {}).get("core"),
                   "platform": record.get("platform"), "numpy": record.get("numpy"),
                   "vulkan_device_info": record.get("vulkan_device_info")}
            for side, npz, metadata, record in (("left", args.left, left_metadata, left_record),
                                                ("right", args.right, right_metadata, right_record))}
        report["sources_identical"] = True
        report["source_array_keys"] = sources
        report["arrays"] = outputs
        report["by_format"] = {fmt: aggregate([row for row in outputs if row["format"] == fmt])
                               for fmt in sorted({row["format"] for row in outputs})}
        report["by_case"] = {case: aggregate([row for row in outputs if row["case"] == case])
                             for case in sorted({row["case"] for row in outputs})}
        report["total"] = aggregate(outputs)
        report["comparison_valid"] = True
        report["bit_exact"] = report["total"]["bit_exact"]
        report["numeric_array_equal"] = report["total"]["numeric_array_equal"]
    except Exception as error:
        report.update(error=f"{type(error).__name__}: {error}", traceback=traceback.format_exc())
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("x", encoding="utf-8") as stream:
        json.dump(report, stream, ensure_ascii=False, indent=2)
        stream.write("\n")
    print(json.dumps({k: report.get(k) for k in
                      ("comparison_valid", "bit_exact", "numeric_array_equal", "by_format", "error")}), flush=True)
    return 0 if report["comparison_valid"] and (not args.require_bit_exact or report["bit_exact"]) else 1


if __name__ == "__main__":
    raise SystemExit(main())
