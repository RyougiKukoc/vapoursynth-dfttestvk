"""Temporary shader instrumentation, restoring the normal build afterward.

Extract one spatial block's real or imaginary forward spectrum through the
normal output frame, and compare the actual Vulkan transform with NumPy.
"""
from pathlib import Path
import argparse
import subprocess
import sys


def main():
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--run", action="store_true")
    parser.add_argument("--imag", action="store_true")
    parser.add_argument("--samples", action="store_true")
    parser.add_argument("--window", action="store_true")
    args = parser.parse_args()
    if args.run:
        sys.path.insert(0, str(root / "tests"))
        import numpy as np
        import vapoursynth as vs
        from common import fixture, make_filter
        import oracle
        source, frames = fixture(vs.core, length=5)
        node = make_filter(vs.core, source, "vk", {"tbsize": 1})
        with node.get_frame(1) as frame:
            actual = np.asarray(frame[0])[:16, :16 if args.samples or args.window else 9].copy() * 255
        window = oracle.constants({"tbsize": 1})[0]
        # W48:H40 -> padded W72:H72, so first block starts at (-12,-16).
        padded = np.pad(frames[1][0].astype(np.float64) * 255, ((16,16),(12,12)), mode="reflect")
        expected = np.fft.rfftn(padded[None,:16,:16]*window)[0]
        expected = expected.imag if args.imag else expected.real
        if args.samples:
            expected = padded[:16,:16] * window[0]
        if args.window:
            expected = window[0]
        print("max", float(np.max(np.abs(actual-expected))))
        print("actual",actual[:4,:4])
        print("expected",expected[:4,:4])
        return
    header = root / "src" / "shaders.h"
    original = header.read_text("utf-8")
    part = "y" if args.imag else "x"
    marker = "#if ZMEAN\n    if (lane == 0) blockMean[tile]"
    value = f"spectrum[y].{part}"
    if args.samples:
        value = "sampleFrame(0,reflectCoordinate(originX+lane,pc.width),reflectCoordinate(originY+y,pc.height))*constants[y*N+lane]"
    if args.window:
        value = "constants[y*N+lane]"
    debug = f"""if (validBlock && lane < {'N' if args.samples or args.window else 'N/2+1'}) {{
        for (int y=0; y<N; ++y) block[blockId*N*N+y*N+lane]={value};
    }}
    return;
"""
    altered = original.replace(marker, debug + marker)
    gather = f"""{{int x=int(gl_GlobalInvocationID.x),y=int(gl_GlobalInvocationID.y);
    if(x>=pc.width||y>=pc.height)return;
    float value=x<{'N' if args.samples or args.window else 'N/2+1'}&&y<N?block[y*N+x]/float(SCALE):0.0;
    dst[y*pc.dstStride+x]=SAMPLE(value);
    return;}}
"""
    # The final main() belongs to gather.
    index = altered.rindex("void main() {") + len("void main() {")
    altered = altered[:index] + gather + altered[index:]
    try:
        header.write_text(altered, "utf-8")
        subprocess.run(["cmake", "--build", str(root / "build"), "--config", "Release"], check=True)
        command = [sys.executable, __file__, "--run"] + (["--imag"] if args.imag else []) + (["--samples"] if args.samples else []) + (["--window"] if args.window else [])
        subprocess.run(command, check=True)
    finally:
        header.write_text(original, "utf-8")
        subprocess.run(["cmake", "--build", str(root / "build"), "--config", "Release"], check=True)


if __name__ == "__main__":
    main()
