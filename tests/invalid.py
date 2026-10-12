"""Exercise parameter validation without freezing supported transform sizes.

No assumption is made that currently unsupported but mathematically valid
spatial/temporal sizes must remain rejected as the implementation grows.
"""
from __future__ import annotations

import argparse
import math
from pathlib import Path
import sys
import numpy as np
import vapoursynth as vs
from common import DEFAULT_DLL, environment, fixture, load_native, make_filter, write_json


def invalid_cases():
    rows = []
    def add(name, **params):
        rows.append({"name": name, "params": params})
    for name, values in (("ftype", [-1, 5]), ("sbsize", [0, -1]), ("tbsize", [0, -1, 2]),
                         ("sosize", [-1, 16, 17]), ("swin", [-1, 12]), ("twin", [-1, 12]),
                         ("ssystem", [-1, 2]), ("sigma", [-1.0]), ("sigma2", [-1.0]),
                         ("pmin", [-1.0]), ("pmax", [-1.0]), ("f0beta", [-1.0]),
                         ("sbeta", [-1.0]), ("tbeta", [-1.0])):
        for value in values:
            add(f"{name}-{value}", **{name: value})
    add("overlap-nondivisor", sbsize=16, sosize=13)
    for name in ("sigma", "sigma2", "pmin", "pmax", "f0beta", "sbeta", "tbeta"):
        for value, label in ((float("nan"), "nan"), (float("inf"), "inf"), (-float("inf"), "negative-inf")):
            add(f"{name}-{label}", **{name: value})
    for name in ("slocation", "ssx", "ssy", "sst"):
        for label, values in (("onepair", [0, 1]), ("odd", [0, 1, 1]),
                              ("missing-start", [0.1, 4, 1, 5]), ("missing-end", [0, 4, .9, 5]),
                              ("duplicate", [0, 4, 0, 5, 1, 7]), ("outside", [-.1, 4, 1, 5]),
                              ("negative-sigma", [0, -1, 1, 5]), ("nan-value", [0, 4, 1, float("nan")]),
                              ("inf-frequency", [0, 4, float("inf"), 5])):
            add(f"{name}-{label}", **{name: values})
    for label, planes in (("negative", [-1]), ("out-of-range", [3]), ("duplicate", [0, 0])):
        add("planes-" + label, planes=planes)
    for name, values in (("cache", [-2, 2]), ("cache_frames", [0, -1, 129]), ("cache_mb", [-1])):
        for value in values:
            add(f"{name}-{value}", **{name: value})
    # VS Python bindings reject explicit empty arrays before native creation.
    add("empty-planes-python-binding", planes=[])
    return rows


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--backend", choices=["vk", "vk-resident"], default="vk")
    ap.add_argument("--dll", type=Path, default=DEFAULT_DLL)
    ap.add_argument("--output", type=Path, required=True)
    args = ap.parse_args()
    if args.output.exists():
        ap.error("Use a new result filename")
    core = vs.core
    load_native(core, args.dll)
    source, _ = fixture(core, "yuv420p16", 80, 64)
    rows = []
    for case in invalid_cases():
        try:
            node = make_filter(core, source, args.backend, case["params"], args.dll)
            node.get_frame(0).close()
            passed, message = False, "Invalid input was accepted"
        except (vs.Error, ValueError, TypeError) as exc:
            passed, message = True, str(exc)
            # A shader/device failure is not evidence of input validation.
            if any(token in message.lower() for token in ("syntax error", "compilation", "undeclared identifier",
                                                           "failed to compile", "failed to initialize")):
                passed = False
        def display(value):
            if isinstance(value, float) and not math.isfinite(value):
                return repr(value)
            if isinstance(value, list):
                return [display(v) for v in value]
            return value
        rows.append({"case": {"name": case["name"], "params": {k: display(v) for k, v in case["params"].items()}},
                     "passed": passed, "message": message})
        print(f"{'PASS' if passed else 'FAIL'} {case['name']} {message}", flush=True)
    # R81's Python binding inserts the upload/download required by vnode:gpu
    # signatures before invoking native code. Verify public output residency.
    for name, source_node, fn, expected_gpu in (
            ("GPU-entry-CPU-input-auto-upload", source, core.dfttestvk.DFTTestGPU, True),
            ("CPU-entry-GPU-input-auto-download", core.std.GPUUpload(source), core.dfttestvk.DFTTest, False)):
        converted = fn(source_node)
        rows.append({"case": {"name": name}, "passed": converted.gpu_resident == expected_gpu,
                     "output_gpu_resident": converted.gpu_resident, "binding_automatic_transfer": True})
    # Omission/None selects all planes. Empty native maps also mean all planes,
    # but the Python binding does not let an empty list reach that code path.
    identity = make_filter(core, source, args.backend, {"planes": None}, args.dll)
    all_planes = make_filter(core, source, args.backend, {}, args.dll)
    with identity.get_frame(0) as actual, all_planes.get_frame(0) as expected:
        passed = dict(actual.props) == dict(expected.props) and all(
            np.array_equal(np.asarray(actual[p]), np.asarray(expected[p])) for p in range(actual.format.num_planes))
    rows.append({"case": {"name": "none-planes-all"}, "passed": passed})
    # Explicitly exercise normal CPU/GPU entry points and returned residency.
    cpu_output = core.dfttestvk.DFTTest(source)
    gpu_input = core.std.GPUUpload(source)
    gpu_output = core.dfttestvk.DFTTestGPU(gpu_input)
    rows.append({"case": {"name": "CPU-entry-output-residency"}, "passed": not cpu_output.gpu_resident,
                 "output_gpu_resident": cpu_output.gpu_resident})
    rows.append({"case": {"name": "GPU-entry-output-residency"}, "passed": gpu_output.gpu_resident,
                 "output_gpu_resident": gpu_output.gpu_resident})
    result = {"backend": args.backend, "environment": environment(core), "cases": rows,
              "passed": all(r["passed"] for r in rows)}
    write_json(args.output, result)
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    sys.exit(main())
