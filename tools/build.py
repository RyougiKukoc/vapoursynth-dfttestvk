"""Configure and build DFTTestVK with CMake and pinned or explicitly chosen headers."""
from __future__ import annotations

import argparse
from pathlib import Path
import shlex
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, default=ROOT / "build")
    parser.add_argument("--generator", default="Ninja")
    parser.add_argument("--configuration", default="Release")
    parser.add_argument("--cmake", default=shutil.which("cmake") or "cmake")
    parser.add_argument("--compiler", help="C++ compiler path or command, otherwise CMake chooses")
    parser.add_argument("--vs-include", type=Path, help="Matching VapourSynth include directory")
    parser.add_argument("--vulkan-headers", type=Path, default=ROOT / "dependencies" / "vulkan-headers" / "include")
    parser.add_argument("--parallel", type=int)
    parser.add_argument("--cmake-arg", action="append", default=[], help="Extra CMake configure option; use --cmake-arg=-DNAME=VALUE")
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()
    if args.parallel is not None and args.parallel < 1:
        parser.error("--parallel must be positive")
    include = args.vs_include
    pinned_include = ROOT / "dependencies" / "vapoursynth" / "include"
    if include is None and pinned_include.is_dir():
        include = pinned_include
    configure = [args.cmake, "-S", str(ROOT), "-B", str(args.build_dir), "-G", args.generator,
                 f"-DCMAKE_BUILD_TYPE={args.configuration}", f"-DPython3_EXECUTABLE={sys.executable}",
                 f"-DVULKAN_HEADERS={args.vulkan_headers.resolve()}"]
    if include is not None:
        configure.append(f"-DVAPOURSYNTH_INCLUDE_DIR={include.resolve()}")
    if args.compiler:
        compiler = shutil.which(args.compiler) or args.compiler
        configure.append(f"-DCMAKE_CXX_COMPILER={compiler}")
    configure.extend(args.cmake_arg)
    build = [args.cmake, "--build", str(args.build_dir), "--config", args.configuration]
    if args.parallel is not None:
        build.extend(["--parallel", str(args.parallel)])
    for command in (configure, build):
        print(shlex.join(command), flush=True)
        if not args.dry_run:
            subprocess.run(command, check=True)


if __name__ == "__main__":
    main()
