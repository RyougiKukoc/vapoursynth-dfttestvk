"""Archive the final validation and benchmark evidence with per-file hashes."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import zipfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT / "dist/dfttestvk-0.1.0-evidence.zip")
    args = parser.parse_args()
    if args.output.exists():
        parser.error("Use a new archive filename")
    selected = []
    for name in ("results/final-main", "results/final-gpu"):
        selected.extend(p for p in (ROOT / name).rglob("*") if p.is_file())
    for name in ("results/final-main-analysis.json", "results/final-main-analysis.md",
                 "results/final-analysis.json", "results/final-analysis.md",
                 "reference/source-lock.json", "README.md", "docs/validation.md", "docs/benchmark.md"):
        selected.append(ROOT / name)
    selected.extend(p for p in (ROOT / "tests/results").rglob("final-r3-*") if p.is_file())
    for folder in (ROOT / "tests/results").glob("final-r3-*.settings"):
        selected.extend(p for p in folder.rglob("*") if p.is_file())
    layer_source = ROOT / "dependencies/validation_layers/source.json"
    if layer_source.is_file():
        selected.append(layer_source)
    selected = sorted(set(selected))
    manifest = {"schema": 1,
                "binary_sha256": hashlib.sha256((ROOT / "build/dfttestvk.dll").read_bytes()).hexdigest(),
                "files": {p.relative_to(ROOT).as_posix():
                          {"bytes": p.stat().st_size, "sha256": hashlib.sha256(p.read_bytes()).hexdigest()}
                          for p in selected}}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(args.output, "x", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as package:
        for path in selected:
            entry = zipfile.ZipInfo("dfttestvk-evidence/" + path.relative_to(ROOT).as_posix(), (1980, 1, 1, 0, 0, 0))
            entry.create_system = 3
            entry.external_attr = 0o100644 << 16
            package.writestr(entry, path.read_bytes(), compress_type=zipfile.ZIP_DEFLATED, compresslevel=9)
        package.writestr("dfttestvk-evidence/manifest-sha256.json", json.dumps(manifest, indent=2).encode("utf-8"))
    # Verify the archive actually contains the bytes whose hashes were recorded.
    with zipfile.ZipFile(args.output) as package:
        for path, expected in manifest["files"].items():
            data = package.read("dfttestvk-evidence/" + path)
            if len(data) != expected["bytes"] or hashlib.sha256(data).hexdigest() != expected["sha256"]:
                raise RuntimeError("Evidence archive hash mismatch: " + path)
    digest = hashlib.sha256(args.output.read_bytes()).hexdigest()
    args.output.with_suffix(".zip.sha256").write_text(digest + "  " + args.output.name + "\n", encoding="ascii")
    print(json.dumps({"archive": str(args.output), "files": len(selected), "sha256": digest}))


if __name__ == "__main__":
    main()
