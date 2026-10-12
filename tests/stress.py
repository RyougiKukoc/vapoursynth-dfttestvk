"""Concurrent independent filter instances with distinct inputs and parameters."""
from __future__ import annotations

import argparse
from pathlib import Path
import random
import sys
import numpy as np
import vapoursynth as vs
from common import DEFAULT_DLL, environment, fixture, make_filter, write_json
from correctness import copy_frame


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--backend", choices=["vk", "vk-resident", "cpu"], default="vk")
    ap.add_argument("--dll", type=Path, default=DEFAULT_DLL)
    ap.add_argument("--instances", type=int, default=6)
    ap.add_argument("--frames", type=int, default=13)
    ap.add_argument("--rounds", type=int, default=6)
    ap.add_argument("--threads", type=int, default=16)
    ap.add_argument("--seed", type=int, default=20261012)
    ap.add_argument("--cache", type=int, choices=[-1, 0, 1], help="explicit public cache option on Vulkan")
    ap.add_argument("--cache-frames", type=int, help="public cache frame cap; omit to use final defaults")
    ap.add_argument("--cache-mb", type=int, help="public cache budget; omit to use final defaults")
    ap.add_argument("--output", type=Path, required=True)
    args = ap.parse_args()
    if args.output.exists() or min(args.instances, args.frames, args.rounds, args.threads) < 1:
        ap.error("Use a new output filename and positive counts")
    core = vs.core
    core.num_threads = args.threads
    nodes, expected, configs = [], [], []
    for instance in range(args.instances):
        fmt = ["gray16", "grays", "yuv420p16"][instance % 3]
        params = {"tbsize": [1, 3, 5, 7][instance % 4], "sosize": [8, 12, 14][instance % 3],
                  "sigma": 2.0 + instance * 3, "f0beta": [1.0, .5, 1.4][instance % 3]}
        if fmt.startswith("yuv"):
            params["planes"] = [0, 2]
        if args.backend in ("vk", "vk-resident"):
            for name in ("cache", "cache_frames", "cache_mb"):
                if getattr(args, name) is not None:
                    params[name] = getattr(args, name)
        source, _ = fixture(core, fmt, 80 + 8 * instance, 64 + 4 * instance, args.frames,
                            ["noise", "ramp", "impulse"][instance % 3], args.seed + instance)
        node = make_filter(core, source, args.backend, params, args.dll)
        core.std.SetVideoCache(node, mode=0)
        nodes.append(node)
        expected.append([copy_frame(node.get_frame(n)) for n in range(args.frames)])
        configs.append({"format": fmt, "width": node.width, "height": node.height, "params": params,
                        "seed": args.seed + instance})
    rng = random.Random(args.seed)
    requests = [(i, n) for _ in range(args.rounds) for i in range(args.instances) for n in range(args.frames)]
    rng.shuffle(requests)
    # One additional pair per instance at each boundary covers repeated hits.
    requests += [(i, n) for i in range(args.instances) for n in (0, args.frames - 1, 0)]
    futures = [(i, n, nodes[i].get_frame_async(n)) for i, n in requests]
    failures = []
    for i, n, future in futures:
        planes, props = copy_frame(future.result())
        if props != expected[i][n][1] or any(not np.array_equal(a, b) for a, b in zip(planes, expected[i][n][0])):
            failures.append({"instance": i, "frame": n})
    result = {"backend": args.backend, "instances": configs, "requests": len(requests),
              "rounds": args.rounds, "frames": args.frames, "threads": args.threads,
              "environment": environment(core), "failures": failures, "passed": not failures}
    write_json(args.output, result)
    print(f"{len(requests)} concurrent requests, {len(failures)} inconsistencies", flush=True)
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    sys.exit(main())
