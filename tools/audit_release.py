"""Check package hashes and explicitly supplied evidence against a selected DLL.

This is a mechanical integrity/provenance report, not a completion audit.
It cannot decide whether a test covers the intended requirements or whether
its numerical/performance budgets are suitable.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import sys
import zipfile

ROOT = Path(__file__).resolve().parents[1]


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def audit_archive(path, dll_hash):
    with zipfile.ZipFile(path) as archive:
        names = archive.namelist()
        if len(names) != len(set(names)):
            raise ValueError("Archive contains duplicate entries")
        manifests = [name for name in names if name.endswith("/manifest-sha256.json")]
        if len(manifests) != 1:
            raise ValueError("Archive must contain exactly one manifest-sha256.json")
        manifest_name = manifests[0]
        prefix = manifest_name.removesuffix("manifest-sha256.json")
        manifest = json.loads(archive.read(manifest_name))
        if manifest.get("schema") != 1 or manifest.get("plugin") != "dfttestvk":
            raise ValueError("Unexpected manifest schema/plugin")
        expected_names = {prefix + name for name in manifest["files"]} | {manifest_name}
        if set(names) != expected_names:
            raise ValueError("Archive entries do not match manifest exactly")
        rows = []
        for name, expected in manifest["files"].items():
            data = archive.read(prefix + name)
            measured = {"bytes": len(data), "sha256": sha256(data)}
            rows.append({"file": name, **measured, "passed": measured == expected})
        plugins = [row for row in rows if row["file"].startswith("dfttestvk.") and
                   Path(row["file"]).suffix in (".dll", ".so", ".dylib")]
        if len(plugins) != 1:
            raise ValueError("Archive must contain exactly one native plugin")
        same_dll = plugins[0]["sha256"] == dll_hash
    sidecar = path.with_suffix(path.suffix + ".sha256")
    checksum = sha256(path.read_bytes())
    sidecar_match = sidecar.read_text(encoding="ascii").split()[0] == checksum if sidecar.is_file() else None
    return {"archive": str(path.resolve()), "archive_sha256": checksum,
            "manifest": manifest_name, "manifest_files": rows,
            "matches_selected_binary": same_dll, "sidecar_matches": sidecar_match,
            "passed": all(row["passed"] for row in rows) and same_dll and sidecar_match is not False}


def audit_result(path, dll_hash):
    data = json.loads(path.read_text(encoding="utf-8"))
    explicit = data.get("passed") is True
    failures = [row.get("case", row) for row in data.get("cases", []) if row.get("passed") is not True]
    hashes = [p.get("sha256") for p in data.get("environment", {}).get("plugins", [])
              if p.get("namespace") == "dfttestvk"]
    # A layer-only wrapper has no native environment. Its underlying frame
    # result must be supplied separately to prove native provenance.
    exact = bool(hashes) and all(value == dll_hash for value in hashes)
    metrics = [m for row in data.get("cases", []) for m in row.get("metrics", [])]
    return {"result": str(path.resolve()), "sha256": sha256(path.read_bytes()),
            "explicit_passed": explicit, "case_count": len(data.get("cases", [])),
            "failed_cases": failures, "loaded_vulkan_hashes": hashes,
            "matches_selected_binary": exact if hashes else None,
            "max_8bit": max((m.get("max_8bit", 0) for m in metrics), default=None),
            "provenance_status": "matches" if exact else "different binary" if hashes else "not recorded in this file",
            "passed": explicit and not failures and exact}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, default=ROOT / "build/dfttestvk.dll")
    parser.add_argument("--archive", type=Path, action="append", default=[])
    parser.add_argument("--result", type=Path, action="append", default=[])
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists():
        parser.error("Use a new output filename")
    if not args.binary.is_file():
        parser.error(f"Missing selected latest binary: {args.binary}")
    if not args.archive and not args.result:
        parser.error("Supply at least one --archive or --result")
    binary_hash = sha256(args.binary.read_bytes())
    archives, results = [], []
    for path in args.archive:
        try:
            archives.append(audit_archive(path, binary_hash))
        except Exception as exc:
            archives.append({"archive": str(path), "passed": False, "error": str(exc)})
    for path in args.result:
        try:
            results.append(audit_result(path, binary_hash))
        except Exception as exc:
            results.append({"result": str(path), "passed": False, "error": str(exc)})
    report = {"binary": str(args.binary.resolve()), "binary_sha256": binary_hash,
              "archives": archives, "results": results,
              "passed": all(row["passed"] for row in archives + results),
              "meaning": "Integrity and explicit provenance checks only; not a requirement-coverage or goal-completion audit."}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(json.dumps({"passed": report["passed"], "binary_sha256": binary_hash,
                      "archives": len(archives), "results": len(results), "report": str(args.output)}, indent=2))
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    sys.exit(main())
