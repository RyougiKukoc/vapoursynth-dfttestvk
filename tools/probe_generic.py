"""Full-frame oracle checks for the general spatial block-size shader.

Run serially after all other GPU measurements. Every case also checks frame
properties, repeated/reverse requests and concurrent randomized requests.
"""
from __future__ import annotations

import argparse
from pathlib import Path
import re
import sys
import time
import traceback

import vapoursynth as vs

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tests"))
from common import DEFAULT_DLL, environment, write_json
from correctness import run_case


def cases():
    result = []
    for n, step in ((1,1),(3,1),(8,2),(12,3),(15,3),(32,8),(63,21),(64,16)):
        for t in (1,3,5,7):
            for fmt in ("grays","gray16"):
                result.append({"name":f"n{n}-t{t}-{fmt}","format":fmt,
                    "width":17,"height":19,"length":3,"pattern":"noise",
                    "params":{"sbsize":n,"sosize":n-step,"tbsize":t}})
    for n in (3,15,63):
        # Asymmetric spatial/temporal windows and nonconstant axis profiles
        # expose mistakes in (-t,-y,-x) conjugate lookup for odd N.
        result.append({"name":f"n{n}-odd-spectrum","format":"grays",
            "width":21,"height":17,"length":3,"pattern":"impulse",
            "params":{"sbsize":n,"sosize":n-(1 if n==3 else n//3),"tbsize":3,"swin":9,"twin":9,
                      "ftype":0,"ssx":[0,2,0.3,8,1,3],"ssy":[0,10,1,2],
                      "sst":[0,3,1,9]}})
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--backend",choices=["vk","vk-resident"],default="vk")
    parser.add_argument("--dll",type=Path,default=DEFAULT_DLL)
    parser.add_argument("--case",default="",help="case-name regex")
    parser.add_argument("--list",action="store_true")
    parser.add_argument("--output",type=Path)
    parser.add_argument("--threads",type=int,default=8)
    parser.add_argument("--seed",type=int,default=20261012)
    parser.add_argument("--float-atol",type=float,default=2e-5)
    parser.add_argument("--int-atol",type=float,default=1)
    args = parser.parse_args()
    args.against = "oracle"
    selected = [c for c in cases() if re.search(args.case,c["name"])]
    if not selected:
        parser.error("No selected cases")
    if args.list:
        print("\n".join(c["name"] for c in selected))
        return
    if args.output is None:
        parser.error("--output is required unless --list")
    if args.output.exists():
        parser.error("Use a new output filename")
    vs.core.num_threads = args.threads
    started = time.perf_counter()
    results = []
    for case in selected:
        try:
            result = run_case(vs.core,case,args)
        except Exception as exc:
            result = {"case":case,"passed":False,"error":str(exc),"traceback":traceback.format_exc()}
        results.append(result)
        maximum = max((m["max_8bit"] for m in result.get("metrics",[])),default=None)
        print(f"{'PASS' if result['passed'] else 'FAIL'} {case['name']} max8bit={maximum} {result.get('error','')}",flush=True)
    write_json(args.output,{"backend":args.backend,"against":"oracle",
        "float_atol":args.float_atol,"int_atol":args.int_atol,"seed":args.seed,
        "seconds":time.perf_counter()-started,"environment":environment(vs.core),
        "cases":results,"passed":all(r["passed"] for r in results)})
    if not all(r["passed"] for r in results):
        raise SystemExit(1)


if __name__ == "__main__":
    main()
