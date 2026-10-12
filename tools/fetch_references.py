"""Fetch pinned algorithm sources and optional isolated Windows CUDA references.

This does not install packages or load any plugin. Existing sources/artifacts
must match their pins; changed directories are never reset or overwritten.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import shutil
import tempfile
import urllib.request
import zipfile

from fetch_dependencies import fetch

ROOT = Path(__file__).resolve().parents[1]


def digest(path: Path) -> str:
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def verify(path: Path, expected: str) -> None:
    if not path.is_file() or digest(path) != expected:
        raise RuntimeError(f"{path}: missing or differs from pinned SHA256")


def fetch_gpu(destination: Path, release: dict, verify_only: bool) -> None:
    destination.mkdir(parents=True, exist_ok=True)
    archive = destination / release["artifact"]
    expected = release["github_sha256"]
    if not archive.exists() and not verify_only:
        with tempfile.NamedTemporaryFile(prefix=archive.name + ".", suffix=".partial",
                                         dir=destination, delete=False) as temporary:
            temporary_path = Path(temporary.name)
            try:
                request = urllib.request.Request(release["url"], headers={"User-Agent": "dfttestvk-reference-fetch"})
                with urllib.request.urlopen(request, timeout=30) as response:
                    shutil.copyfileobj(response, temporary, length=1024 * 1024)
            except BaseException:
                temporary.close()
                temporary_path.unlink()
                raise
        try:
            verify(temporary_path, expected)
            if temporary_path.stat().st_size != release["bytes"]:
                raise RuntimeError("Reference archive size differs from lock")
            temporary_path.rename(archive)
        finally:
            temporary_path.unlink(missing_ok=True)
    verify(archive, expected)
    target = destination / archive.stem
    if not verify_only:
        target.mkdir(parents=True, exist_ok=True)
        with zipfile.ZipFile(archive) as package:
            for entry in package.infolist():
                path = target / entry.filename
                if not path.resolve().is_relative_to(target.resolve()):
                    raise RuntimeError(f"Archive member escapes destination: {entry.filename}")
                if entry.is_dir():
                    path.mkdir(parents=True, exist_ok=True)
                    continue
                path.parent.mkdir(parents=True, exist_ok=True)
                if path.exists():
                    with package.open(entry) as member:
                        expected_member = hashlib.file_digest(member, "sha256").hexdigest()
                    verify(path, expected_member)
                    continue
                with package.open(entry) as member, path.open("xb") as output:
                    shutil.copyfileobj(member, output, length=1024 * 1024)
    for name, expected_plugin in release["plugins_sha256"].items():
        verify(target / "dfttest2" / name, expected_plugin)
    print(f"Verified {release['tag']} in {target}", flush=True)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--destination", type=Path, default=ROOT / "dependencies")
    parser.add_argument("--gpu-reference", action="store_true",
                        help="Also stage the locked Windows cu129 archive with NVRTC/cuFFT DLLs")
    parser.add_argument("--artifact-destination", type=Path,
                        default=ROOT / "reference" / "backend-artifacts")
    parser.add_argument("--skip-sources", action="store_true")
    parser.add_argument("--verify-only", action="store_true", help="Verify existing files without downloading or extracting")
    args = parser.parse_args()
    lock = json.loads((ROOT / "reference" / "source-lock.json").read_text("utf-8"))
    if not args.skip_sources:
        for name, source in zip(("dfttest-original", "dfttest2-api4"), lock["sources"]):
            path = args.destination / name
            if args.verify_only and not path.exists():
                raise RuntimeError(f"{path}: source tree missing")
            fetch(path, source["repository"] + ".git", source["commit"])
    if args.gpu_reference:
        fetch_gpu(args.artifact_destination, lock["reference_release"], args.verify_only)


if __name__ == "__main__":
    main()
