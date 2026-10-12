"""Create a deterministic plugin zip with documentation, source locks and hashes.

Identical binary/document inputs yield an identical archive. This does not
publish a release or assert that compiler output is bit-for-bit reproducible.
"""
from __future__ import annotations

import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import re
import zipfile

ROOT = Path(__file__).resolve().parents[1]


def project_version() -> str:
    source = (ROOT / "CMakeLists.txt").read_text("utf-8")
    match = re.search(r"project\s*\(\s*dfttestvk\s+VERSION\s+([0-9]+(?:\.[0-9]+)+)", source, re.I)
    if not match:
        raise RuntimeError("Cannot read project version from CMakeLists.txt")
    return match[1]


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--platform", choices=("windows", "linux", "macos"), required=True)
    parser.add_argument("--arch", default="x86_64")
    parser.add_argument("--version", default=project_version())
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    if not re.fullmatch(r"[A-Za-z0-9._+-]+", args.version) or not re.fullmatch(r"[A-Za-z0-9_-]+", args.arch):
        parser.error("version and arch must be simple filename components")
    suffix = {"windows": ".dll", "linux": ".so", "macos": ".dylib"}[args.platform]
    if not args.binary.is_file() or args.binary.suffix.lower() != suffix:
        parser.error(f"Expected an existing {suffix} plugin for {args.platform}")
    output = args.output or ROOT / "dist" / f"dfttestvk-{args.version}-{args.platform}-{args.arch}.zip"
    files = {"dfttestvk" + suffix: args.binary.read_bytes()}
    for name in ("README.md", "LICENSE", "THIRD_PARTY.md", "docs/algorithm.md", "docs/build.md", "docs/temporal-cache-design.md", "docs/validation.md", "docs/optimization-notes.md", "docs/benchmark.md"):
        files[name] = (ROOT / name).read_bytes()
    for name in ("docs/linux-validation.md", "docs/holywu-techniques.md"):
        if (ROOT / name).is_file():
            files[name] = (ROOT / name).read_bytes()
    source_lock = json.loads((ROOT / "reference" / "source-lock.json").read_text("utf-8"))
    # Local development installation paths do not belong in a distributable package.
    installed = source_lock.get("installed_references", {})
    installed.pop("python", None)
    for value in installed.values():
        if isinstance(value, dict):
            value.pop("path", None)
    files["reference/source-lock.json"] = (json.dumps(source_lock, indent=2, sort_keys=True) + "\n").encode("utf-8")
    for path in sorted((ROOT / "licenses").rglob("*")):
        if path.is_file():
            files[path.relative_to(ROOT).as_posix()] = path.read_bytes()
    files["manifest.vs"] = b"[VapourSynth Manifest V1]\ndfttestvk\n"
    manifest = {"schema": 1, "plugin": "dfttestvk", "version": args.version,
                "platform": args.platform, "architecture": args.arch,
                "files": {name: {"bytes": len(body), "sha256": hashlib.sha256(body).hexdigest()}
                          for name, body in sorted(files.items())}}
    files["manifest-sha256.json"] = (json.dumps(manifest, indent=2, sort_keys=True) + "\n").encode("utf-8")
    epoch = int(os.environ.get("SOURCE_DATE_EPOCH", "315532800"))
    date = datetime.datetime.fromtimestamp(max(315532800, epoch), datetime.timezone.utc)
    if date.year > 2107:
        parser.error("SOURCE_DATE_EPOCH exceeds the ZIP timestamp range")
    stamp = (date.year, date.month, date.day, date.hour, date.minute, date.second // 2 * 2)
    output.parent.mkdir(parents=True, exist_ok=True)
    # Refuse to replace an earlier archive so prior evidence stays reviewable.
    with zipfile.ZipFile(output, "x", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as package:
        for name, body in sorted(files.items()):
            entry = zipfile.ZipInfo("dfttestvk/" + name, date_time=stamp)
            entry.create_system = 3
            entry.external_attr = 0o100644 << 16
            entry.compress_type = zipfile.ZIP_DEFLATED
            package.writestr(entry, body, compress_type=zipfile.ZIP_DEFLATED, compresslevel=9)
    sha = hashlib.sha256(output.read_bytes()).hexdigest()
    output.with_suffix(output.suffix + ".sha256").write_text(f"{sha}  {output.name}\n", encoding="ascii")
    print(f"{output}\nSHA256 {sha}", flush=True)


if __name__ == "__main__":
    main()
