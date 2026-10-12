"""CPU-only numerical check of the generic GLSL FFT/DFT arithmetic."""
from __future__ import annotations

import argparse
import ctypes
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

import numpy as np


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", default=shutil.which("g++") or "g++")
    args = parser.parse_args()
    header = (Path(__file__).resolve().parents[1] / "src" / "generic_shaders.h").read_text("utf-8")
    code = header[header.index("vec2 multiplyComplex"):header.index("#ifndef SIGMA_IS_SCALAR")].replace("inout ", "")
    prefix = r'''
#include <cmath>
const float PI2 = 6.283185307179586476925286766559;
struct vec2 {
    float x,y;
    vec2()=default;
    explicit vec2(float a):x(a),y(a){}
    vec2(float a,float b):x(a),y(b){}
    vec2 operator+(vec2 b)const{return vec2(x+b.x,y+b.y);}
    vec2 operator-(vec2 b)const{return vec2(x-b.x,y-b.y);}
    vec2& operator+=(vec2 b){x+=b.x;y+=b.y;return *this;}
};
'''
    exported = "__declspec(dllexport) " if os.name == "nt" else ""
    suffix = f'''
extern "C" {{
{exported}void forward(float *p){{spatialTransform(reinterpret_cast<vec2*>(p),false);}}
{exported}void inverse(float *p){{spatialTransform(reinterpret_cast<vec2*>(p),true);}}
{exported}void temporal(float *p){{temporalTransform(reinterpret_cast<vec2*>(p));}}
}}
'''
    rng = np.random.default_rng(27)
    with tempfile.TemporaryDirectory(prefix="dfttestvk-generic-") as folder:
        folder = Path(folder)
        source = folder / "verify.cpp"
        source.write_text(prefix+code+suffix, "utf-8")
        dll_dir = os.add_dll_directory(str(Path(args.compiler).resolve().parent)) if os.name == "nt" else None
        for n,t in ((1,1),(3,3),(8,3),(12,5),(15,7),(16,1),(32,3),(63,5),(64,7)):
            library_path = folder / f"n{n}-t{t}.dll"
            subprocess.run([args.compiler,"-shared","-O2",f"-DN={n}",f"-DT={t}",str(source),"-o",str(library_path)],check=True)
            library = ctypes.CDLL(str(library_path))

            def call(name, values):
                values = np.ascontiguousarray(values,dtype=np.complex64)
                f = getattr(library,name)
                f.argtypes = [ctypes.POINTER(ctypes.c_float)]
                f.restype = None
                f(values.ctypes.data_as(ctypes.POINTER(ctypes.c_float)))
                return values

            values = (rng.normal(size=n)+1j*rng.normal(size=n)).astype(np.complex64)
            forward = call("forward",values.copy())
            inverse = call("inverse",forward.copy())
            temporal_values = (rng.normal(size=t)+1j*rng.normal(size=t)).astype(np.complex64)
            temporal = call("temporal",temporal_values.copy())
            errors = [float(np.max(np.abs(forward-np.fft.fft(values)))),
                      float(np.max(np.abs(inverse-n*values))),
                      float(np.max(np.abs(temporal-np.fft.fft(temporal_values))))]
            print(f"N{n} T{t} forward={errors[0]:.9g} inverse={errors[1]:.9g} temporal={errors[2]:.9g}")
            assert errors[0] < n*3e-6 and errors[1] < n*2e-5 and errors[2] < t*3e-6, errors
            if os.name == "nt":
                import _ctypes
                _ctypes.FreeLibrary(library._handle)
        if dll_dir is not None:
            dll_dir.close()


if __name__ == "__main__":
    main()
