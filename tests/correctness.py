"""Validate public semantics, full images, properties and request independence.

Defaults to the installed CPU baseline against the independent NumPy oracle,
so fixture/oracle development does not consume a GPU. Select --backend vk
explicitly after a native build. Acceptance budgets are explicit observations,
not a claim of bit exactness across different FFT implementations.
"""
from __future__ import annotations

import argparse
import random
import re
import sys
import time
import traceback
from pathlib import Path
import numpy as np
import vapoursynth as vs

from common import DEFAULT_DLL, environment, fixture, make_filter, metrics, write_json
import oracle


def cases(suite="full"):
    all_cases = []
    def add(name, **values):
        case = {"name": name, "format": "grays", "width": 48, "height": 40,
                "length": 5, "pattern": "noise", "params": {}}
        case.update(values)
        all_cases.append(case)
    for fmt in ("gray8", "gray9", "gray10", "gray12", "gray14", "gray16", "grays"):
        for pattern in ("constant", "ramp", "noise", "impulse"):
            add(f"{fmt}-{pattern}", format=fmt, pattern=pattern)
    if suite == "smoke":
        return all_cases[::4] + [all_cases[-1]]
    if suite == "half":
        return [{**all_cases[-1], "name": f"{fmt}-{pattern}", "format": fmt,
                 "width": 80, "height": 64, "pattern": pattern}
                for fmt in ("grayh", "yuv444ph", "rgbh")
                for pattern in ("constant", "ramp", "noise", "impulse")]
    if suite == "degenerate":
        rows = []
        for tbsize in (1, 3):
            for system in (0, 1):
                for ftype in (0, 2):
                    for profile in ("slocation", "sst", "ssx"):
                        rows.append({"name": f"b1-t{tbsize}-ss{system}-f{ftype}-{profile}",
                                     "format": "grays", "width": 32, "height": 24, "length": 5,
                                     "pattern": "noise", "params": {"sbsize": 1, "sosize": 0,
                                     "tbsize": tbsize, "ftype": ftype, "zmean": False,
                                     "sigma": .8, "ssystem": system, profile: [0, .5, .4, .8, 1, 1.5]}})
        return rows
    for length in (1, 2, 5):
        for tbsize in (1, 3, 5, 7):
            add(f"boundary-length{length}-tb{tbsize}", length=length,
                pattern="impulse", params={"tbsize": tbsize})
    for fmt in ("yuv420p8", "yuv420p10", "yuv420p16", "yuv444ps", "rgb24", "rgb48", "rgbs"):
        for planes in ([0], [1, 2], [0, 1, 2]):
            add(f"{fmt}-planes{''.join(map(str, planes))}", format=fmt,
                width=80, height=64, params={"planes": planes})
    for ftype in range(5):
        values = {"ftype": ftype, "zmean": False}
        if ftype >= 2:
            values.update(sigma=0.7, sigma2=0.3, pmin=5.0, pmax=30.0)
        add(f"ftype{ftype}-zmean0", params=values)
    for beta in (0.0, 0.5, 1.4, 1.00002, 0.5002):
        add(f"f0beta{beta}", params={"f0beta": beta})
    for win in range(12):
        add(f"window{win}", params={"swin": win, "twin": (win + 4) % 12,
                                    "sbeta": 1.7, "tbeta": 3.2})
    for step in (1, 2, 4, 8, 10, 16):
        add(f"overlap-step{step}", params={"sosize": 16 - step}, width=53, height=45)
    for system in (0, 1):
        for tb in (1, 3):
            add(f"slocation-ssystem{system}-tb{tb}", params={"tbsize": tb, "ssystem": system,
                "slocation": [0, 3, 0.25, 7, 0.7, 15, 1, 4]})
            add(f"axes-ssystem{system}-tb{tb}", params={"tbsize": tb, "ssystem": system,
                "ssx": [0, 4, 1, 10], "ssy": [0, 8, 0.6, 3, 1, 12], "sst": [0, 2, 1, 9]})
    add("none-planes-all", format="yuv420p16", width=80, height=64, params={"planes": None})
    if suite == "tiny":
        # Repeated reflection is mathematically defined by the oracle. Some
        # upstream CPU implementations only support one reflection and these
        # cases should be tested against oracle rather than that baseline.
        return [{**all_cases[3], "name": f"tiny-{w}x{h}", "width": w, "height": h}
                for w, h in ((1, 1), (2, 3), (7, 5), (17, 19))]
    return all_cases


def copy_frame(frame):
    with frame:
        return [np.asarray(frame[p]).copy() for p in range(frame.format.num_planes)], dict(frame.props)


def run_case(core, case, args):
    source, arrays = fixture(core, fmt=case["format"], width=case["width"], height=case["height"],
                             length=case["length"], pattern=case["pattern"], seed=args.seed)
    node_params = dict(case["params"])
    if getattr(args, "cache", None) is not None:
        node_params["cache"] = args.cache
    node = make_filter(core, source, args.backend, node_params, args.dll)
    # Force all request checks through the filter instead of its output cache.
    core.std.SetVideoCache(node, mode=0)
    if (node.width, node.height, node.num_frames, node.format.id) != (
            source.width, source.height, source.num_frames, source.format.id):
        raise AssertionError("Filter changed clip layout or length")
    actual = [copy_frame(node.get_frame(n)) for n in range(case["length"])]
    results = []
    floating = source.format.sample_type == vs.FLOAT
    bits = source.format.bits_per_sample
    tolerance = args.float_atol if floating else args.int_atol
    for reference in args.against.split(","):
        if reference == "oracle":
            selected = case["params"].get("planes", list(range(source.format.num_planes)))
            if selected is None or selected == []:
                selected = list(range(source.format.num_planes))
            if isinstance(selected, int):
                selected = [selected]
            expected = [[oracle.plane([a[p] for a in arrays], n, case["params"], bits, floating)
                         if p in selected else arrays[n][p]
                         for p in range(source.format.num_planes)] for n in range(case["length"])]
        else:
            baseline = make_filter(core, source, reference, case["params"], args.dll)
            expected = [copy_frame(baseline.get_frame(n))[0] for n in range(case["length"])]
        for p in range(source.format.num_planes):
            measured = metrics(np.stack([a[0][p] for a in actual]),
                               np.stack([a[p] for a in expected]), bits, floating)
            passed = (measured["max_half_ulps"] <= args.half_ulps if floating and bits == 16
                      else measured["max"] <= tolerance)
            measured.update(reference=reference, plane=p,
                            tolerance={"half_ulps": args.half_ulps} if floating and bits == 16 else tolerance,
                            passed=passed)
            measured["frames"] = [{"frame": n, **metrics(actual[n][0][p], expected[n][p], bits, floating)}
                                  for n in range(case["length"])]
            results.append(measured)
    selected = case["params"].get("planes", list(range(source.format.num_planes)))
    if selected is None or selected == []:
        selected = list(range(source.format.num_planes))
    if isinstance(selected, int):
        selected = [selected]
    for n, (planes, props) in enumerate(actual):
        with source.get_frame(n) as original:
            if props != dict(original.props):
                raise AssertionError(f"Frame {n}: properties changed")
        for p in range(source.format.num_planes):
            if p not in selected and not np.array_equal(planes[p], arrays[n][p]):
                raise AssertionError(f"Frame {n}: unselected plane {p} changed")
    # Requests include repeated, reverse, endpoints and concurrent random order.
    indexes = [0, case["length"] - 1, 0] + list(reversed(range(case["length"])))
    shuffled = list(range(case["length"])) * 3
    random.Random(args.seed).shuffle(shuffled)
    indexes += shuffled
    for n in indexes:
        planes, props = copy_frame(node.get_frame(n))
        if props != actual[n][1] or any(not np.array_equal(a, b) for a, b in zip(planes, actual[n][0])):
            raise AssertionError(f"Serial request inconsistency at frame {n}")
    futures = [(n, node.get_frame_async(n)) for n in shuffled]
    for n, future in futures:
        planes, props = copy_frame(future.result())
        if props != actual[n][1] or any(not np.array_equal(a, b) for a, b in zip(planes, actual[n][0])):
            raise AssertionError(f"Concurrent request inconsistency at frame {n}")
    return {"case": case, "node_params": node_params, "passed": all(m["passed"] for m in results), "metrics": results,
            "properties_preserved": True, "unselected_planes_exact": True,
            "serial_requests": len(indexes), "concurrent_requests": len(shuffled),
            "request_order_independent": True}


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--backend", choices=["cpu", "original", "vk", "vk-resident", "cuda", "nvrtc"], default="cpu")
    ap.add_argument("--against", default="oracle", help="comma-separated oracle,cpu,original,vk,cuda,nvrtc")
    ap.add_argument("--suite", choices=["smoke", "full", "tiny", "half", "degenerate"], default="full")
    ap.add_argument("--case", default="", help="regex selecting case names")
    ap.add_argument("--list", action="store_true")
    ap.add_argument("--dll", type=Path, default=DEFAULT_DLL)
    ap.add_argument("--float-atol", type=float, default=2e-5)
    ap.add_argument("--int-atol", type=float, default=1.0)
    ap.add_argument("--half-ulps", type=int, default=1, help="per-sample float16 output quantization budget")
    ap.add_argument("--cache", type=int, choices=[-1, 0, 1], help="override Vulkan node cache mode for ablation checks")
    ap.add_argument("--threads", type=int, default=8)
    ap.add_argument("--seed", type=int, default=20261012)
    ap.add_argument("--output", type=Path, required=True)
    args = ap.parse_args()
    if args.cache is not None and args.backend not in ("vk", "vk-resident"):
        ap.error("--cache applies only to Vulkan backends")
    if args.output.exists():
        ap.error("Use a new output filename")
    selected = [c for c in cases(args.suite) if re.search(args.case, c["name"])]
    if not selected:
        ap.error("No selected cases")
    if args.list:
        print("\n".join(c["name"] for c in selected))
        return
    core = vs.core
    core.num_threads = args.threads
    before = time.perf_counter()
    rows = []
    for case in selected:
        try:
            row = run_case(core, case, args)
        except Exception as exc:
            row = {"case": case, "passed": False, "error": str(exc), "traceback": traceback.format_exc()}
        rows.append(row)
        worst = max((m["max_8bit"] for m in row.get("metrics", [])), default=None)
        print(f"{'PASS' if row['passed'] else 'FAIL'} {case['name']} max8bit={worst} {row.get('error', '')}", flush=True)
    result = {"backend": args.backend, "against": args.against, "suite": args.suite,
              "float_atol": args.float_atol, "int_atol": args.int_atol,
              "half_ulps": args.half_ulps,
              "cache": args.cache,
              "seed": args.seed, "seconds": time.perf_counter() - before,
              "environment": environment(core), "cases": rows, "passed": all(r["passed"] for r in rows)}
    write_json(args.output, result)
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    sys.exit(main())
