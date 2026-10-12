"""Serial, fresh-process exploration of execution mappings; not final performance evidence."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
VARIANTS = {
    "base": {"DFTTESTVK_TILES": "4", "DFTTESTVK_SUBGROUP": "0", "DFTTESTVK_UNROLL": "0"},
    "unroll": {"DFTTESTVK_TILES": "4", "DFTTESTVK_SUBGROUP": "0", "DFTTESTVK_UNROLL": "1"},
    "subgroup": {"DFTTESTVK_TILES": "4", "DFTTESTVK_SUBGROUP": "1", "DFTTESTVK_UNROLL": "0"},
    "both": {"DFTTESTVK_TILES": "4", "DFTTESTVK_SUBGROUP": "1", "DFTTESTVK_UNROLL": "1"},
    "tiles2": {"DFTTESTVK_TILES": "2", "DFTTESTVK_SUBGROUP": "1", "DFTTESTVK_UNROLL": "1"},
    "tiles8": {"DFTTESTVK_TILES": "8", "DFTTESTVK_SUBGROUP": "1", "DFTTESTVK_UNROLL": "1"},
    "tiles16": {"DFTTESTVK_TILES": "16", "DFTTESTVK_SUBGROUP": "1", "DFTTESTVK_UNROLL": "1"},
    "static4": {"DFTTESTVK_TILES": "4", "DFTTESTVK_SUBGROUP": "0", "DFTTESTVK_STATIC_MAIN": "1"},
    "static16": {"DFTTESTVK_TILES": "16", "DFTTESTVK_SUBGROUP": "0", "DFTTESTVK_STATIC_MAIN": "1"},
    "static16sub": {"DFTTESTVK_TILES": "16", "DFTTESTVK_SUBGROUP": "1", "DFTTESTVK_STATIC_MAIN": "1"},
    "short": {"DFTTESTVK_TILES": "16", "DFTTESTVK_SUBGROUP": "1", "DFTTESTVK_UNROLL": "1", "DFTTESTVK_SHORT_REFLECT": "1"},
    "staticshort": {"DFTTESTVK_TILES": "16", "DFTTESTVK_SUBGROUP": "1", "DFTTESTVK_STATIC_MAIN": "1", "DFTTESTVK_SHORT_REFLECT": "1"},
    "devicebase": {"DFTTESTVK_TILES": "4", "DFTTESTVK_DEVICE_CONSTANTS": "1"},
    "devicestatic": {"DFTTESTVK_TILES": "16", "DFTTESTVK_SUBGROUP": "0", "DFTTESTVK_STATIC_MAIN": "1", "DFTTESTVK_SHORT_REFLECT": "1", "DFTTESTVK_DEVICE_CONSTANTS": "1"},
    "fma": {"DFTTESTVK_TILES": "16", "DFTTESTVK_STATIC_MAIN": "1", "DFTTESTVK_SHORT_REFLECT": "1", "DFTTESTVK_DEVICE_CONSTANTS": "1", "DFTTESTVK_FFT_FMA": "1"},
    "realshared": {"DFTTESTVK_TILES": "16", "DFTTESTVK_STATIC_MAIN": "1", "DFTTESTVK_SHORT_REFLECT": "1", "DFTTESTVK_DEVICE_CONSTANTS": "1", "DFTTESTVK_REAL_SHARED": "1"},
    "embed4": {"DFTTESTVK_TILES": "4", "DFTTESTVK_STATIC_MAIN": "1", "DFTTESTVK_SHORT_REFLECT": "1", "DFTTESTVK_EMBED_CONSTANTS": "1"},
    "embed16": {"DFTTESTVK_TILES": "16", "DFTTESTVK_STATIC_MAIN": "1", "DFTTESTVK_SHORT_REFLECT": "1", "DFTTESTVK_EMBED_CONSTANTS": "1"},
    "embed16sub": {"DFTTESTVK_TILES": "16", "DFTTESTVK_SUBGROUP": "1", "DFTTESTVK_STATIC_MAIN": "1", "DFTTESTVK_SHORT_REFLECT": "1", "DFTTESTVK_EMBED_CONSTANTS": "1"},
    "pad16": {"DFTTESTVK_TILES": "16", "DFTTESTVK_STATIC_MAIN": "1", "DFTTESTVK_SHORT_REFLECT": "1", "DFTTESTVK_DEVICE_CONSTANTS": "1", "DFTTESTVK_SHARED_PAD": "1"},
    "scalarsigma": {"DFTTESTVK_TILES": "16", "DFTTESTVK_STATIC_MAIN": "1", "DFTTESTVK_SHORT_REFLECT": "1", "DFTTESTVK_DEVICE_CONSTANTS": "1", "DFTTESTVK_SCALAR_SIGMA": "1"},
    "ssbostatic": {"DFTTESTVK_TILES": "16", "DFTTESTVK_STATIC_MAIN": "1", "DFTTESTVK_SHORT_REFLECT": "1", "DFTTESTVK_DEVICE_CONSTANTS": "1", "DFTTESTVK_SCALAR_SIGMA": "0"},
    "rows4": {"DFTTESTVK_TILES": "4", "DFTTESTVK_STATIC_MAIN": "1", "DFTTESTVK_SHORT_REFLECT": "1", "DFTTESTVK_ROW_CONSTANTS": "1"},
    "rows16": {"DFTTESTVK_TILES": "16", "DFTTESTVK_STATIC_MAIN": "1", "DFTTESTVK_SHORT_REFLECT": "1", "DFTTESTVK_ROW_CONSTANTS": "1"},
    "warp2": {"DFTTESTVK_TILES": "2", "DFTTESTVK_SUBGROUP": "1", "DFTTESTVK_STATIC_MAIN": "1", "DFTTESTVK_SHORT_REFLECT": "1", "DFTTESTVK_DEVICE_CONSTANTS": "1", "DFTTESTVK_WARP_TILE": "1"},
    "warp4": {"DFTTESTVK_TILES": "4", "DFTTESTVK_SUBGROUP": "1", "DFTTESTVK_STATIC_MAIN": "1", "DFTTESTVK_SHORT_REFLECT": "1", "DFTTESTVK_DEVICE_CONSTANTS": "1", "DFTTESTVK_WARP_TILE": "1"},
    "warp8": {"DFTTESTVK_TILES": "8", "DFTTESTVK_SUBGROUP": "1", "DFTTESTVK_STATIC_MAIN": "1", "DFTTESTVK_SHORT_REFLECT": "1", "DFTTESTVK_DEVICE_CONSTANTS": "1", "DFTTESTVK_WARP_TILE": "1"},
    "warp16": {"DFTTESTVK_TILES": "16", "DFTTESTVK_SUBGROUP": "1", "DFTTESTVK_STATIC_MAIN": "1", "DFTTESTVK_SHORT_REFLECT": "1", "DFTTESTVK_DEVICE_CONSTANTS": "1", "DFTTESTVK_WARP_TILE": "1"},
    "cache4": {"DFTTESTVK_TILES": "4", "DFTTESTVK_STATIC_MAIN": "1", "DFTTESTVK_SHORT_REFLECT": "1", "DFTTESTVK_DEVICE_CONSTANTS": "1", "DFTTESTVK_TEMPORAL_CACHE": "1"},
    "cache8": {"DFTTESTVK_TILES": "8", "DFTTESTVK_STATIC_MAIN": "1", "DFTTESTVK_SHORT_REFLECT": "1", "DFTTESTVK_DEVICE_CONSTANTS": "1", "DFTTESTVK_TEMPORAL_CACHE": "1"},
    "cache16": {"DFTTESTVK_TILES": "16", "DFTTESTVK_STATIC_MAIN": "1", "DFTTESTVK_SHORT_REFLECT": "1", "DFTTESTVK_DEVICE_CONSTANTS": "1", "DFTTESTVK_TEMPORAL_CACHE": "1"},
    "cachecopy": {"DFTTESTVK_TILES": "16", "DFTTESTVK_STATIC_MAIN": "1", "DFTTESTVK_SHORT_REFLECT": "1", "DFTTESTVK_DEVICE_CONSTANTS": "1", "DFTTESTVK_TEMPORAL_CACHE": "1", "DFTTESTVK_CACHE_DEVICE_COPY": "1"},
    "cache32": {"DFTTESTVK_TILES": "16", "DFTTESTVK_STATIC_MAIN": "1", "DFTTESTVK_SHORT_REFLECT": "1", "DFTTESTVK_DEVICE_CONSTANTS": "1", "DFTTESTVK_TEMPORAL_CACHE": "1", "DFTTESTVK_CACHE_FRAMES": "32"},
    "cache64": {"DFTTESTVK_TILES": "16", "DFTTESTVK_STATIC_MAIN": "1", "DFTTESTVK_SHORT_REFLECT": "1", "DFTTESTVK_DEVICE_CONSTANTS": "1", "DFTTESTVK_TEMPORAL_CACHE": "1", "DFTTESTVK_CACHE_FRAMES": "64"},
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--results", type=Path, required=True)
    parser.add_argument("--variants", default=",".join(VARIANTS))
    parser.add_argument("--frames", type=int, default=256)
    parser.add_argument("--tbsize", type=int, default=3)
    parser.add_argument("--profile", action="store_true")
    parser.add_argument("--skill", type=Path, default=Path("C:/Users/Kukoc/.codex/skills/holywu-vulkan"))
    args = parser.parse_args()
    args.results.mkdir(parents=True, exist_ok=False)
    rows = []
    for name in args.variants.split(","):
        env = os.environ.copy()
        for key in ("DFTTESTVK_TILES", "DFTTESTVK_SUBGROUP", "DFTTESTVK_UNROLL", "DFTTESTVK_STATIC_MAIN", "DFTTESTVK_SHORT_REFLECT", "DFTTESTVK_DEVICE_CONSTANTS", "DFTTESTVK_FFT_FMA", "DFTTESTVK_REAL_SHARED", "DFTTESTVK_EMBED_CONSTANTS", "DFTTESTVK_SHARED_PAD", "DFTTESTVK_SCALAR_SIGMA", "DFTTESTVK_ROW_CONSTANTS", "DFTTESTVK_WARP_TILE", "DFTTESTVK_TEMPORAL_CACHE", "DFTTESTVK_CACHE_FRAMES", "DFTTESTVK_CACHE_DEVICE_COPY"):
            env.pop(key, None)
        env.update(VARIANTS[name])
        env["DFTTESTVK_PROFILE"] = "1" if args.profile else "0"
        config = {"backend": "vk", "format": "gray16", "raw_yuv420p16": str(ROOT.parent / "sample-1080p.yuv"),
                  "params": {"sbsize": 16, "sosize": 12, "tbsize": args.tbsize, "twin": 7},
                  "diagnostic_environment": VARIANTS[name], "profiling": args.profile}
        cfg = args.results / (name + ".config.json")
        cfg.write_text(json.dumps(config, indent=2), "utf-8")
        out = args.results / (name + ".json")
        command = [sys.executable, str(args.skill / "scripts" / "benchmark_vs.py"),
                   "--factory", str(ROOT / "tests" / "bench_factory.py") + ":make",
                   "--config", str(cfg), "--output", str(out), "--frames", str(args.frames),
                   "--warmup", "16", "--threads", "16", "--prefetch", "8"]
        with (args.results / (name + ".log")).open("w", encoding="utf-8") as log:
            result = subprocess.run(command, env=env, stdout=log, stderr=subprocess.STDOUT)
        row = {"name": name, "environment": VARIANTS[name], "returncode": result.returncode}
        if out.exists():
            data = json.loads(out.read_text("utf-8"))
            row["timing"] = data.get("timing", data.get("steady", {}))
        rows.append(row)
        print(name, (args.results / (name + ".log")).read_text("utf-8"), flush=True)
    (args.results / "exploration.json").write_text(json.dumps(rows, indent=2), "utf-8")


if __name__ == "__main__":
    main()
