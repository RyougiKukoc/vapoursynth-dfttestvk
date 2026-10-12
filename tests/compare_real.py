"""Full-frame numerical comparison on preloaded real video, outside timing."""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys
import numpy as np
import vapoursynth as vs
from bench_factory import make
from common import DEFAULT_DLL, environment, make_filter, metrics, write_json


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--backend", default="vk", choices=["vk", "vk-resident", "cpu", "original", "nvrtc", "cuda"])
    ap.add_argument("--against", default="cpu,nvrtc", help="comma-separated reference backends")
    ap.add_argument("--raw", type=Path, default=Path("D:/WorkSpace-bm3d/sample-1080p.yuv"))
    ap.add_argument("--width", type=int, default=1920)
    ap.add_argument("--height", type=int, default=1080)
    ap.add_argument("--format", default="gray16")
    ap.add_argument("--frames", default="0,1,12,22,23", help="sample indices in a 24-frame real sequence")
    ap.add_argument("--config", type=Path, help="optional JSON containing params/options")
    ap.add_argument("--dll", type=Path, default=DEFAULT_DLL)
    ap.add_argument("--threads", type=int, default=16)
    ap.add_argument("--int-atol", type=float, default=1)
    ap.add_argument("--float-atol", type=float, default=2e-5)
    ap.add_argument("--output", type=Path, required=True)
    args = ap.parse_args()
    if args.output.exists():
        ap.error("Use a new result filename")
    config = json.loads(args.config.read_text(encoding="utf-8")) if args.config else {}
    config.update(backend="source", raw_yuv420p16=str(args.raw), width=args.width,
                  height=args.height, format=args.format, held_frames=24)
    core = vs.core
    core.num_threads = args.threads
    bundle = make(core, config, 24)
    source = bundle["source"]
    node = make_filter(core, source, args.backend, config.get("params"), args.dll, config.get("options"))
    rows = []
    indices = [int(n) for n in args.frames.split(",")]
    for reference in args.against.split(","):
        baseline = make_filter(core, source, reference, config.get("params"), args.dll)
        for n in indices:
            with node.get_frame(n) as actual, baseline.get_frame(n) as expected, source.get_frame(n) as original:
                if dict(actual.props) != dict(original.props):
                    raise AssertionError(f"Frame {n} properties changed")
                for p in range(actual.format.num_planes):
                    floating = actual.format.sample_type == vs.FLOAT
                    result = metrics(np.asarray(actual[p]), np.asarray(expected[p]), actual.format.bits_per_sample, floating)
                    tolerance = args.float_atol if floating else args.int_atol
                    result.update(frame=n, plane=p, reference=reference, tolerance=tolerance,
                                  passed=result["max"] <= tolerance)
                    rows.append(result)
                    print(f"{reference} f{n} p{p} max={result['max']} max8bit={result['max_8bit']} rms={result['rms']}", flush=True)
    result = {"backend": args.backend, "source_info": bundle["source_info"],
              "params": config.get("params", {}), "format": source.format.name,
              "frame_indices": indices, "environment": environment(core), "metrics": rows,
              "passed": all(r["passed"] for r in rows)}
    write_json(args.output, result)
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    sys.exit(main())
