"""Build and inspect Linux artifacts, then validate generated FFT arithmetic."""
from __future__ import annotations

import argparse
import ctypes
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import sys
import time


def registration(binary):
    """Exercise the exported API4 initializer without creating a Vulkan device."""
    library = ctypes.CDLL(str(binary), mode=os.RTLD_NOW)
    records = {"functions": {}}
    version_type = ctypes.CFUNCTYPE(ctypes.c_int)
    config_type = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_char_p, ctypes.c_char_p,
                                  ctypes.c_char_p, ctypes.c_int, ctypes.c_int,
                                  ctypes.c_int, ctypes.c_void_p)
    register_type = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_char_p, ctypes.c_char_p,
                                    ctypes.c_char_p, ctypes.c_void_p, ctypes.c_void_p,
                                    ctypes.c_void_p)

    @version_type
    def version():
        return (4 << 16) | 3

    @config_type
    def configure(identifier, namespace, name, plugin_version, api_version, flags, plugin):
        records.update(identifier=identifier.decode(), namespace=namespace.decode(),
                       name=name.decode(), plugin_version=plugin_version,
                       api_version=api_version, flags=flags)
        return 1

    @register_type
    def register(name, args, returns, function, data, plugin):
        records["functions"][name.decode()] = {"arguments": args.decode(),
                                               "returns": returns.decode(),
                                               "callable_pointer_present": bool(function)}
        return 1

    class PluginAPI(ctypes.Structure):
        _fields_ = [("getAPIVersion", version_type), ("configPlugin", config_type),
                    ("registerFunction", register_type)]

    api = PluginAPI(version, configure, register)
    entry = library.VapourSynthPluginInit2
    entry.argtypes = [ctypes.c_void_p, ctypes.POINTER(PluginAPI)]
    entry.restype = None
    entry(ctypes.c_void_p(1), ctypes.byref(api))
    assert records["identifier"] == "com.ryougikukoc.dfttestvk"
    assert records["namespace"] == "dfttestvk"
    assert records["api_version"] == (4 << 16) | 3
    assert set(records["functions"]) == {"DFTTest", "DFTTestGPU"}
    assert records["functions"]["DFTTest"]["returns"] == "clip:vnode;"
    assert records["functions"]["DFTTestGPU"]["returns"] == "clip:vnode:gpu;"
    assert all(row["callable_pointer_present"] for row in records["functions"].values())
    records["passed"] = True
    return records


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=Path("/src"))
    parser.add_argument("--output", type=Path, default=Path("/out"))
    parser.add_argument("--compilers", default="g++,clang++")
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    report_path = args.output / "verification.json"
    if report_path.exists():
        parser.error("Use a new output directory")
    started = time.perf_counter()
    report = {"environment": {"python": sys.version, "platform": platform.platform(),
                               "os_release": Path("/etc/os-release").read_text(),
                               "proxy": {k: os.environ.get(k) for k in ("http_proxy", "https_proxy")}},
              "commands": [], "builds": [], "gpu_execution": False,
              "scope": "Linux native compilation, ELF dependency/loading and registration, CPU FFT verification"}

    def run(name, command):
        before = time.perf_counter()
        completed = subprocess.run(command, cwd=args.source, text=True, encoding="utf-8",
                                   stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        path = args.output / (name + ".log")
        path.write_text(completed.stdout, "utf-8")
        report["commands"].append({"name": name, "command": list(map(str, command)),
                                   "returncode": completed.returncode,
                                   "seconds": time.perf_counter() - before, "log": path.name})
        print(f"{name}: {'PASS' if completed.returncode == 0 else 'FAIL'}", flush=True)
        if completed.returncode:
            print(completed.stdout[-4000:], flush=True)
            raise subprocess.CalledProcessError(completed.returncode, command)
        return completed.stdout

    try:
        run("packages", ["dpkg-query", "-W", "g++", "clang", "cmake", "ninja-build", "libc6", "libstdc++6"])
        run("python-packages", [sys.executable, "-m", "pip", "freeze"])
        for compiler in args.compilers.split(","):
            resolved = shutil.which(compiler)
            if not resolved:
                raise RuntimeError("Compiler not installed: " + compiler)
            label = "gcc" if compiler == "g++" else "clang"
            run(label + "-version", [resolved, "--version"])
            build = args.output / ("build-" + label)
            run(label + "-configure", ["cmake", "-S", str(args.source), "-B", str(build),
                                      "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release",
                                      "-DCMAKE_CXX_COMPILER=" + resolved,
                                      "-DVAPOURSYNTH_INCLUDE_DIR=" + str(args.source / "dependencies/vapoursynth/include"),
                                      "-DVULKAN_HEADERS=" + str(args.source / "dependencies/vulkan-headers/include")])
            run(label + "-build", ["cmake", "--build", str(build), "--parallel", "2"])
            binary = build / "dfttestvk.so"
            identification = run(label + "-file", ["file", str(binary)])
            symbols = run(label + "-symbols", ["nm", "-D", "--defined-only", str(binary)])
            assert "VapourSynthPluginInit2" in symbols
            dependencies = run(label + "-dependencies", ["ldd", "-r", str(binary)])
            assert "not found" not in dependencies and "undefined symbol" not in dependencies
            versions = run(label + "-symbol-versions", ["readelf", "--version-info", "--wide", str(binary)])
            checks = registration(binary)
            report["builds"].append({"compiler": compiler, "binary": str(binary),
                                     "sha256": hashlib.sha256(binary.read_bytes()).hexdigest(),
                                     "bytes": binary.stat().st_size, "file": identification.strip(),
                                     "registration": checks,
                                     "version_requirements": sorted(set(re.findall(r"(?:GLIBCXX|GLIBC|CXXABI)_\d+(?:\.\d+)+", versions)))})
            run(label + "-fft-codelets", [sys.executable, str(args.source / "tools/verify_fft_codelets.py"), "--compiler", resolved])
            run(label + "-generic-fft", [sys.executable, str(args.source / "tools/verify_generic_fft.py"), "--compiler", resolved])
        report["passed"] = True
    except Exception as error:
        report.update(passed=False, error=str(error))
    finally:
        report["seconds"] = time.perf_counter() - started
        report_path.write_text(json.dumps(report, indent=2), "utf-8")
    print(json.dumps({"passed": report["passed"], "builds": len(report["builds"]), "report": str(report_path)}))
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    sys.exit(main())
