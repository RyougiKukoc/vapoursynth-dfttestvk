"""Check the actual generated GLSL arithmetic against NumPy's FFT.

This CPU-only check compiles the array codelets as C++ and checks their signs,
frequency ordering, inverse real ordering, and unnormalized transform gain.
It complements full Vulkan output validation; it does not execute a shader.
"""
from __future__ import annotations

import argparse
import ctypes
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

import numpy as np


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", default=shutil.which("g++") or "g++")
    parser.add_argument("--fft-fma",action="store_true",help="exercise explicit fused codelet arithmetic")
    args = parser.parse_args()
    args.compiler = shutil.which(args.compiler) or args.compiler
    header = (Path(__file__).resolve().parents[1] / "src" / "shaders.h").read_text("utf-8")
    code = header[header.index("#define DK"):header.index("#if WARP_TILE\nlayout(local_size_x=")]
    code = code.replace("inout ", "")
    prefix = "#include <cmath>\nstruct vec2 { float x, y; };\n"
    suffix = '''
extern "C" {
__declspec(dllexport) void forward_real(float *p) { rdft16(p); }
__declspec(dllexport) void inverse_real(float *p) { irdft16(p); }
__declspec(dllexport) void forward_complex(float *p) { dft16(reinterpret_cast<vec2*>(p)); }
__declspec(dllexport) void inverse_complex(float *p) { idft16(reinterpret_cast<vec2*>(p)); }
__declspec(dllexport) void forward_temporal(float *p) { dftTemporal(reinterpret_cast<vec2*>(p)); }
}
'''
    if os.name != "nt":
        suffix = suffix.replace("__declspec(dllexport) ", "")
    rng = np.random.default_rng(73)
    max_errors = {}
    with tempfile.TemporaryDirectory(prefix="dfttestvk-codelets-") as folder:
        folder = Path(folder)
        cpp = folder / "verify.cpp"
        cpp.write_text(prefix + code + suffix, "utf-8")
        dll_dir = os.add_dll_directory(str(Path(args.compiler).resolve().parent)) if os.name == "nt" else None
        for t in (1, 3, 5, 7):
            library_path = folder / f"codelets-{t}.dll"
            subprocess.run([args.compiler, "-shared", "-O2", f"-DT={t}", f"-DFFT_FMA={int(args.fft_fma)}", str(cpp), "-o", str(library_path)], check=True)
            library = ctypes.CDLL(str(library_path))

            def call(name: str, data: np.ndarray) -> np.ndarray:
                data = np.ascontiguousarray(data, dtype=np.float32)
                function = getattr(library, name)
                function.argtypes = [ctypes.POINTER(ctypes.c_float)]
                function.restype = None
                function(data.ctypes.data_as(ctypes.POINTER(ctypes.c_float)))
                return data

            real = rng.normal(size=16).astype(np.float32)
            rfft = call("forward_real", np.r_[real, np.zeros(2)].astype(np.float32)).view(np.complex64)
            max_errors[f"t{t}_rfft"] = float(np.max(np.abs(rfft - np.fft.rfft(real))))
            inverse = call("inverse_real", rfft.view(np.float32).copy())[:16]
            inverse = inverse[np.r_[0, np.arange(15, 0, -1)]]
            max_errors[f"t{t}_irfft"] = float(np.max(np.abs(inverse - 16 * real)))
            complex_input = (rng.normal(size=16) + 1j * rng.normal(size=16)).astype(np.complex64)
            fft = call("forward_complex", complex_input.view(np.float32).copy()).view(np.complex64)
            max_errors[f"t{t}_fft"] = float(np.max(np.abs(fft - np.fft.fft(complex_input))))
            inverse = call("inverse_complex", fft.view(np.float32).copy()).view(np.complex64)
            max_errors[f"t{t}_ifft"] = float(np.max(np.abs(inverse - 16 * complex_input)))
            temporal = (rng.normal(size=t) + 1j * rng.normal(size=t)).astype(np.complex64)
            fft = call("forward_temporal", temporal.view(np.float32).copy()).view(np.complex64)
            max_errors[f"t{t}_temporal"] = float(np.max(np.abs(fft - np.fft.fft(temporal))))
            phases = np.exp(2j * np.pi * np.arange(t) * (t // 2) / t)
            max_errors[f"t{t}_center_inverse"] = float(abs(np.sum(fft * phases) - t * temporal[t // 2]))
            if os.name == "nt":
                import _ctypes
                _ctypes.FreeLibrary(library._handle)
        if dll_dir is not None:
            dll_dir.close()
    for key, value in max_errors.items():
        print(f"{key}: max_abs={value:.9g}")
    assert max(max_errors.values()) < 2e-5, max_errors


if __name__ == "__main__":
    main()
