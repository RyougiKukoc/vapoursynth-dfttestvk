"""Summarize sampled NVIDIA telemetry without inventing an unsampled VRAM peak."""
from __future__ import annotations

import argparse
import csv
import json
from pathlib import Path
import re
import sys


def summarize(path):
    lines = path.read_text(encoding="utf-8-sig", errors="replace").splitlines()
    if not lines:
        return {"file": str(path.resolve()), "samples": 0, "status": "empty; no memory or peak evidence"}
    reader = csv.DictReader(lines)
    samples = list(reader)
    columns = reader.fieldnames or []
    def values(fragment):
        keys = [key for key in columns if fragment in key]
        if len(keys) != 1:
            return []
        measured = []
        for row in samples:
            value = row.get(keys[0], "")
            if match := re.fullmatch(r"\s*(-?\d+(?:\.\d+)?)\s*(?:[A-Za-z%]+)?\s*", value):
                measured.append(float(match[1]))
        return measured
    memory = values("memory.used")
    temps, power, clocks = values("temperature.gpu"), values("power.draw"), values("clocks.current.sm") or values("clocks.sm")
    return {"file": str(path.resolve()), "samples": len(samples), "valid_memory_samples": len(memory),
            "status": "sampled" if memory else "no parseable memory samples",
            "sampled_min_vram_mib": min(memory) if memory else None,
            "sampled_max_vram_mib": max(memory) if memory else None,
            "sampled_max_temperature_c": max(temps) if temps else None,
            "sampled_max_power_w": max(power) if power else None,
            "sampled_sm_clock_min_mhz": min(clocks) if clocks else None,
            "sampled_sm_clock_max_mhz": max(clocks) if clocks else None,
            "limitation": "Device-wide sampled memory includes other processes; intervals can miss true peaks. This does not measure plugin-only allocation or guarantee a worst-case peak."}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("files", nargs="+", type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    if args.output.exists():
        parser.error("Use a new output filename")
    rows = [summarize(path) for path in args.files]
    result = {"files": rows, "all_have_memory_samples": all(row.get("valid_memory_samples", 0) > 0 for row in rows)}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2), encoding="utf-8")
    print(json.dumps(result, indent=2))
    return 0 if result["all_have_memory_samples"] else 1


if __name__ == "__main__":
    sys.exit(main())
