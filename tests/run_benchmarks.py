"""Run serial rotated fresh-process benchmark repetitions with the skill harness."""
from __future__ import annotations

import argparse
from pathlib import Path
import subprocess
import sys


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--skill", type=Path,
                    default=Path("C:/Users/Kukoc/.codex/skills/holywu-vulkan"))
    ap.add_argument("--matrix", type=Path, required=True)
    ap.add_argument("--results", type=Path, required=True)
    ap.add_argument("--frames", type=int, default=2048)
    ap.add_argument("--warmup", type=int, default=16)
    ap.add_argument("--prefetch", type=int, default=8)
    ap.add_argument("--threads", type=int, default=16)
    ap.add_argument("--repeats", type=int, default=3)
    ap.add_argument("--access", choices=["sequential", "random"], default="sequential")
    ap.add_argument("--telemetry", action="store_true")
    args = ap.parse_args()
    runner = args.skill / "scripts" / "run_matrix.py"
    if not runner.is_file():
        ap.error(f"Skill runner missing: {runner}; pass --skill with installed skill path")
    command = [sys.executable, str(runner), "--factory", str(Path(__file__).with_name("bench_factory.py")) + ":make",
               "--matrix", str(args.matrix), "--results", str(args.results), "--frames", str(args.frames),
               "--warmup", str(args.warmup), "--prefetch", str(args.prefetch), "--threads", str(args.threads),
               "--repeats", str(args.repeats), "--access", args.access]
    if args.telemetry:
        command.append("--telemetry")
    return subprocess.run(command).returncode


if __name__ == "__main__":
    sys.exit(main())
