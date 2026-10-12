"""Record a remote Linux runtime without claiming filter validation/performance.

Select --device before inspecting the live device. Every invocation writes a
new JSON report; a failed import or SDK mismatch is preserved as failed evidence.
"""
from __future__ import annotations

import argparse
import hashlib
import importlib.metadata
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys


def sha256(path: Path) -> str:
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def command(argv: list[str]) -> dict:
    executable = shutil.which(argv[0])
    if not executable:
        return {"command": argv, "available": False}
    try:
        result = subprocess.run([executable, *argv[1:]], text=True,
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                timeout=20, check=False)
        return {"command": [executable, *argv[1:]], "available": True,
                "returncode": result.returncode, "output": result.stdout.strip()}
    except Exception as error:
        return {"command": argv, "available": True, "error": str(error)}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--plugin", type=Path, required=True)
    parser.add_argument("--device", type=int, default=-1,
                        help="VapourSynth Vulkan enumeration index; -1 uses core selection")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists():
        parser.error("Use a new output filename")
    report = {"python": sys.version, "python_executable": sys.executable,
              "platform": platform.platform(), "libc": platform.libc_ver(),
              "conda_prefix": os.environ.get("CONDA_PREFIX"),
              "requested_vulkan_device": args.device,
              "scope": "SDK, native plugin loading and Vulkan device initialization only",
              "filter_output_verified": False,
              "hardware_gpu_performance_verified": False,
              "commands": {}}
    try:
        try:
            import vapoursynth as vs
        except ImportError as error:
            raise RuntimeError("VapourSynth is unavailable in this Python environment; "
                               "install the matching R81 runtime before GPU initialization") from error
        report["vapoursynth_module"] = vs.__file__
        report["vapoursynth_version"] = str(vs.__version__)
        for name in ("VapourSynth", "numpy", "dfttest2"):
            try:
                report.setdefault("packages", {})[name] = importlib.metadata.version(name)
            except importlib.metadata.PackageNotFoundError:
                report.setdefault("packages", {})[name] = None
        installed_sdk = Path(vs.get_include())
        pinned_sdk = args.root / "dependencies/vapoursynth/include"
        headers = {}
        for name in ("VapourSynth4.h", "VSVulkan4.h"):
            installed, pinned = installed_sdk / name, pinned_sdk / name
            headers[name] = {"installed_path": str(installed), "pinned_path": str(pinned),
                             "installed_sha256": sha256(installed),
                             "pinned_sha256": sha256(pinned),
                             "matches_pinned_sdk": installed.read_bytes() == pinned.read_bytes()}
        report["headers"] = headers
        if not all(row["matches_pinned_sdk"] for row in headers.values()):
            raise RuntimeError("Installed VapourSynth SDK differs from the pinned build SDK")
        core = vs.core
        report["core"] = str(core)
        # Enumeration does not instantiate a Vulkan device. Reading device_info does.
        report["vulkan_devices"] = core.vulkan_devices
        core.set_vulkan_device(args.device)
        plugin = args.plugin.resolve(strict=True)
        core.std.LoadPlugin(path=str(plugin))
        loaded = next(p for p in core.plugins() if p.namespace == "dfttestvk")
        loaded_path = Path(loaded.plugin_path).resolve(strict=True)
        if loaded_path != plugin:
            raise RuntimeError(f"Unexpected autoloaded plugin: {loaded_path}; requested {plugin}")
        report["plugin"] = {"path": str(loaded_path), "sha256": sha256(loaded_path),
                            "functions": {f.name: f.signature for f in loaded.functions()}}
        report["vulkan_device_info"] = core.vulkan_device_info
        maps = Path("/proc/self/maps")
        if maps.is_file():
            paths = sorted({line.split()[-1] for line in maps.read_text().splitlines()
                            if "/" in line and any(token in line.lower() for token in
                            ("vapoursynth", "libvulkan", "libnvidia", "libstdc++"))})
            report["loaded_runtime_libraries"] = [
                {"path": path, "sha256": sha256(Path(path))}
                for path in paths if Path(path).is_file()]
        report["passed"] = True
    except Exception as error:
        report.update(passed=False, error=f"{type(error).__name__}: {error}")
    # Tool inspection is useful even when the runtime/SDK check failed. None of
    # these observations substitutes for actual filter or output validation.
    for name, argv in {"mamba": ["mamba", "--version"],
                       "gcc": ["g++", "--version"],
                       "cmake": ["cmake", "--version"],
                       "glibc": ["ldd", "--version"],
                       "nvidia_smi": ["nvidia-smi", "--query-gpu=index,name,uuid,driver_version,memory.total",
                                      "--format=csv,noheader"]}.items():
        report["commands"][name] = command(argv)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("x", encoding="utf-8") as stream:
        json.dump(report, stream, ensure_ascii=False, indent=2)
        stream.write("\n")
    print(json.dumps(report, ensure_ascii=False, indent=2))
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
