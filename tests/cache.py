"""Verify cache caps, eviction, duplicate requests and defined memory fallbacks.

Every node is built from the current DLL and has output caching disabled.
Cache variants compare completed pixels/properties against cache=0, then
repeated serial/concurrent results against their own serial output exactly.
"""
from __future__ import annotations

import argparse
from pathlib import Path
import random
import sys
import numpy as np
import vapoursynth as vs
from common import DEFAULT_DLL, environment, fixture, make_filter, metrics, write_json
from correctness import copy_frame


def cases():
    rows = []
    for cap in (1, 2, 12):
        for temporal in (1, 3, 7):
            rows.append({"name": f"cap{cap}-t{temporal}", "format": "grays", "width": 80, "height": 64,
                         "params": {"tbsize": temporal, "cache": 1, "cache_frames": cap, "cache_mb": 64}})
    for fmt in ("gray16", "yuv420p16", "grays"):
        rows.append({"name": f"budget1-{fmt}", "format": fmt, "width": 128, "height": 96,
                     "params": {"tbsize": 7, "cache": -1, "cache_frames": 12, "cache_mb": 1}})
    rows.append({"name": "larger-than-budget", "format": "grays", "width": 512, "height": 288,
                 "params": {"tbsize": 3, "cache": -1, "cache_frames": 12, "cache_mb": 1}})
    rows.append({"name": "generic-cache-fallback", "format": "grays", "width": 80, "height": 64,
                 "params": {"sbsize": 15, "sosize": 10, "tbsize": 3,
                            "cache": -1, "cache_frames": 2, "cache_mb": 64}})
    rows.append({"name": "tiny-cache", "format": "grays", "width": 2, "height": 3,
                 "params": {"tbsize": 7, "cache": 1, "cache_frames": 1, "cache_mb": 1}})
    return rows


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--backend", choices=["vk", "vk-resident"], default="vk")
    ap.add_argument("--dll", type=Path, default=DEFAULT_DLL)
    ap.add_argument("--frames", type=int, default=17)
    ap.add_argument("--threads", type=int, default=24)
    ap.add_argument("--rounds", type=int, default=3)
    ap.add_argument("--seed", type=int, default=20261012)
    ap.add_argument("--float-atol", type=float, default=2e-5)
    ap.add_argument("--int-atol", type=float, default=1)
    ap.add_argument("--case", default="", help="case-name regex")
    ap.add_argument("--output", type=Path, required=True)
    args = ap.parse_args()
    if args.output.exists() or min(args.frames, args.threads, args.rounds) < 1:
        ap.error("Use a new output filename and positive counts")
    import re
    core = vs.core
    core.num_threads = args.threads
    selected = [row for row in cases() if re.search(args.case, row["name"])]
    if not selected:
        ap.error("No selected cases")
    results = []
    for case in selected:
        source, _ = fixture(core, case["format"], case["width"], case["height"], args.frames,
                            "noise", args.seed)
        params = case["params"]
        reference = make_filter(core, source, args.backend, {**params, "cache": 0}, args.dll)
        node = make_filter(core, source, args.backend, params, args.dll)
        core.std.SetVideoCache(reference, mode=0)
        core.std.SetVideoCache(node, mode=0)
        expected = [copy_frame(reference.get_frame(n)) for n in range(args.frames)]
        serial = [copy_frame(node.get_frame(n)) for n in range(args.frames)]
        rows = []
        floating = source.format.sample_type == vs.FLOAT
        bits = source.format.bits_per_sample
        tolerance = args.float_atol if floating else args.int_atol
        for p in range(source.format.num_planes):
            row = metrics(np.stack([frame[0][p] for frame in serial]),
                          np.stack([frame[0][p] for frame in expected]), bits, floating)
            row.update(plane=p, tolerance=tolerance, passed=row["max"] <= tolerance)
            rows.append(row)
        for n in range(args.frames):
            if serial[n][1] != expected[n][1]:
                raise AssertionError(f"{case['name']}: properties changed at {n}")
        rng = random.Random(args.seed)
        requests = [0, args.frames - 1, 0] + list(reversed(range(args.frames)))
        shuffled = list(range(args.frames)) * args.rounds
        rng.shuffle(shuffled)
        requests += shuffled
        failures = []
        for n in requests:
            planes, props = copy_frame(node.get_frame(n))
            if props != serial[n][1] or any(not np.array_equal(a, b) for a, b in zip(planes, serial[n][0])):
                failures.append({"mode": "serial", "frame": n})
        # Enqueue duplicates of neighboring source windows while they may be
        # pending. Their native spectrum dedupe/retention must remain sound.
        duplicate_indexes = shuffled + [0] * args.threads + [args.frames - 1] * args.threads
        futures = [(n, node.get_frame_async(n)) for n in duplicate_indexes]
        for n, future in futures:
            planes, props = copy_frame(future.result())
            if props != serial[n][1] or any(not np.array_equal(a, b) for a, b in zip(planes, serial[n][0])):
                failures.append({"mode": "concurrent", "frame": n})
        result = {"case": case, "metrics": rows, "properties_preserved": True,
                  "serial_requests": len(requests), "concurrent_requests": len(duplicate_indexes),
                  "failures": failures, "passed": all(row["passed"] for row in rows) and not failures}
        results.append(result)
        print(f"{'PASS' if result['passed'] else 'FAIL'} {case['name']} max8bit={max(r['max_8bit'] for r in rows)}", flush=True)
    result = {"backend": args.backend, "frames": args.frames, "threads": args.threads,
              "environment": environment(core), "cases": results,
              "passed": all(row["passed"] for row in results)}
    write_json(args.output, result)
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    sys.exit(main())
