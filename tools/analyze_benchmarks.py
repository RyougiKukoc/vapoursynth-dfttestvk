"""Inspect completed benchmark JSON/NPZ/telemetry and write a reviewable table.

Read-only with respect to benchmark artifacts. Three stored samples corroborate
quality for those frames; they do not replace the all-frame numerical suites.
The output reports provenance/protocol gaps without converting an exploratory
run into a formal result or treating a sampled device VRAM maximum as a peak.
"""
from __future__ import annotations

import argparse
from collections import defaultdict
import hashlib
import json
from pathlib import Path
import re
import statistics
import sys
import numpy as np
from summarize_telemetry import summarize as telemetry_summary

ROOT = Path(__file__).resolve().parents[1]
SOURCE_SHA = "9bd42cbecbd611156f6633757d83a2c83e53c18adc7b0a3a452588bd947e798c"
DEFAULT_PARAMS = {"ftype": 0, "sigma": 8.0, "sigma2": 8.0, "pmin": 0.0, "pmax": 500.0,
                  "sbsize": 16, "smode": 1, "sosize": 12, "tbsize": 3, "tmode": 0, "tosize": 0,
                  "swin": 0, "twin": 7, "sbeta": 2.5, "tbeta": 2.5, "zmean": True,
                  "f0beta": 1.0, "ssystem": 0, "planes": None}


def file_sha(path):
    with Path(path).open("rb") as file:
        return hashlib.file_digest(file, "sha256").hexdigest()


def mathematical_params(config):
    params = {**DEFAULT_PARAMS, **config.get("params", {})}
    for name in ("cache", "cache_frames", "cache_mb"):
        params.pop(name, None)
    return params


def workload(run):
    data = run["data"]
    return {"output": data["output"], "params": mathematical_params(data["config"]),
            "source_sha256": data.get("source_info", {}).get("sha256"),
            "source_frame_indices": data.get("source_info", {}).get("source_frame_indices"),
            "held_frames": data.get("source_info", {}).get("held_frames"),
            "sample_frames": data.get("sample_frames"),
            "resources": {name: data.get(name) for name in
                          ("threads", "prefetch", "frames", "warmup", "access", "seed")}}


def collect(directory, expected_vk_sha, expected_source_sha, min_seconds):
    runs = []
    errors = []
    for path in sorted(directory.glob("*.json")):
        if path.name.endswith(".config.json") or path.name == "summary.json":
            continue
        try:
            data = json.loads(path.read_text(encoding="utf-8"))
        except Exception as exc:
            errors.append({"file": str(path), "error": str(exc)})
            continue
        if not all(name in data for name in ("fps", "seconds", "frames", "config", "output")):
            continue
        match = re.fullmatch(r"(.+)-r(\d+)", path.stem)
        name, repeat = (match[1], int(match[2])) if match else (path.stem, None)
        backend = data["config"].get("backend")
        namespaces = {"vk": "dfttestvk", "vk-resident": "dfttestvk", "cpu": "dfttest2_cpu",
                      "original": "dfttest", "nvrtc": "dfttest2_nvrtc", "cuda": "dfttest2_cuda"}
        native = [plugin for plugin in data.get("environment", {}).get("plugins", [])
                  if plugin.get("namespace") == namespaces.get(backend)]
        native_hash = native[0].get("sha256") if len(native) == 1 else None
        source_hash = data.get("source_info", {}).get("sha256")
        diagnostic = data.get("source_info", {}).get("diagnostic_environment")
        issues = []
        if backend in ("vk", "vk-resident") and native_hash != expected_vk_sha:
            issues.append("Vulkan DLL hash differs from selected latest binary")
        if backend != "source" and native_hash is None:
            issues.append("Native backend path/hash is missing or ambiguous")
        if native:
            native_path = Path(native[0].get("path", ""))
            if not native_path.is_file() or file_sha(native_path) != native_hash:
                issues.append("Recorded native backend differs from its current file")
        if source_hash != expected_source_sha:
            issues.append("Input source hash differs from required source")
        if not data.get("finite"):
            issues.append("No positive finite-output assertion")
        if backend != "source" and data["seconds"] < min_seconds:
            issues.append(f"Timed filter duration is shorter than {min_seconds:g} seconds")
        if diagnostic is None:
            issues.append("Diagnostic environment was not recorded")
        elif diagnostic:
            issues.append("Diagnostic mapping is enabled; classify as ablation")
        factory = Path(data.get("factory", ""))
        if not factory.is_file() or file_sha(factory) != data.get("factory_sha256"):
            issues.append("Recorded factory differs from current file")
        for helper, expected in data.get("source_info", {}).get("project_helpers_sha256", {}).items():
            helper_path = Path(helper)
            if not helper_path.is_file() or file_sha(helper_path) != expected:
                issues.append("Recorded project helper differs from current file: " + helper)
        if not path.with_suffix(".npz").is_file():
            issues.append("Sample NPZ is missing")
        telemetry_path = path.with_suffix(".telemetry.csv")
        telemetry = telemetry_summary(telemetry_path) if telemetry_path.is_file() else {
            "samples": 0, "status": "telemetry file missing"}
        if not telemetry.get("valid_memory_samples"):
            issues.append("No sampled GPU memory evidence")
        runs.append({"file": str(path.resolve()), "sha256": file_sha(path), "name": name,
                     "repeat": repeat, "backend": backend, "native_sha256": native_hash,
                     "native_path": native[0].get("path") if native else None,
                     "source_sha256": source_hash, "seconds": data["seconds"], "fps": data["fps"],
                     "config": data["config"], "diagnostic_environment": diagnostic,
                     "telemetry": telemetry, "issues": issues, "data": data})
    return runs, errors


def compare_samples(reference, candidate, int_atol, float_atol):
    if workload(reference) != workload(candidate):
        return {"reference": reference["file"], "candidate": candidate["file"],
                "passed": False, "error": "Source, mathematical parameters, output, resources or sample indices differ"}
    ref_path = Path(reference["file"]).with_suffix(".npz")
    test_path = Path(candidate["file"]).with_suffix(".npz")
    measured = {}
    with np.load(ref_path) as ref, np.load(test_path) as test:
        if set(ref.files) != set(test.files):
            raise ValueError("Sample frame/plane keys differ")
        for key in sorted(ref.files):
            a, b = ref[key], test[key]
            if a.shape != b.shape or a.dtype != b.dtype:
                raise ValueError(f"{key}: sample shape/dtype mismatch")
            if not np.isfinite(a).all() or not np.isfinite(b).all():
                raise ValueError(f"{key}: nonfinite sample")
            floating = np.issubdtype(a.dtype, np.floating)
            format_name = candidate["data"]["output"]["format"]
            # The measured native scale is retained; an explicit 8-bit scale
            # uses nominal depth instead of assuming uint16 means 16 bits.
            if floating:
                scale = 255.0
            else:
                depths = re.findall(r"(?:Gray|P)(\d+)", format_name, flags=re.I)
                bits = int(depths[-1]) if depths else 16 if a.dtype.itemsize == 2 else 8
                scale = 1.0 / (1 << (bits - 8))
            delta = b.astype(np.float64) - a.astype(np.float64)
            absolute = np.abs(delta)
            maximum = float(absolute.max(initial=0))
            budget = float_atol if floating else int_atol
            measured[key] = {"shape": list(a.shape), "dtype": str(a.dtype),
                             "rms": float(np.sqrt(np.mean(delta * delta))), "max": maximum,
                             "p99": float(np.quantile(absolute, .99)),
                             "different_fraction": float(np.mean(delta != 0)),
                             "max_8bit": maximum * scale, "budget": budget, "passed": maximum <= budget}
    return {"reference": reference["file"], "candidate": candidate["file"],
            "reference_backend": reference["backend"], "repeat": candidate["repeat"],
            "planes": measured, "passed": all(row["passed"] for row in measured.values())}


def render_markdown(report):
    lines = ["# Completed benchmark evidence", "",
             "Selected Vulkan DLL SHA256: `" + report["selected_vk_sha256"] + "`.", "",
             "| Result group | Backend | Repeats | Median FPS | Min–max FPS | Seconds min–max | Sampled VRAM max MiB | Checks |",
             "| --- | --- | ---: | ---: | ---: | ---: | ---: | --- |"]
    for row in report["groups"]:
        rates, durations = row["fps"], row["seconds"]
        memory = row["sampled_max_vram_mib"]
        checks = "pass" if row["protocol_passed"] else "; ".join(row["issues"])
        lines.append(f"| {row['directory']}/{row['name']} | {row['backend']} | {len(rates)} | {statistics.median(rates):.2f} | {min(rates):.2f}–{max(rates):.2f} | {min(durations):.3f}–{max(durations):.3f} | {memory if memory is not None else 'unavailable'} | {checks} |")
    lines += ["", "The source-only row is exempt from the minimum filter duration. These are complete CPU-output timings; graph/preload and warmup remain separate in raw JSON.", "",
              "Device-wide sampled VRAM includes other processes and can miss peaks. It is not plugin-only allocation or a worst-case memory bound.", "",
              "| Sample pair | Max native difference | Maximum plane RMS | Result |",
              "| --- | ---: | ---: | --- |"]
    for row in report["sample_comparisons"]:
        measured = row.get("planes", {})
        name = Path(row["candidate"]).stem + " vs " + Path(row["reference"]).stem
        maximum = max((x["max"] for x in measured.values()), default=None)
        rms = max((x["rms"] for x in measured.values()), default=None)
        lines.append(f"| {name} | {maximum if maximum is not None else 'unavailable'} | {rms if rms is not None else 'unavailable'} | {'pass' if row['passed'] else row.get('error', 'budget exceeded')} |")
    lines += ["", "Three benchmark samples establish numerical agreement only for their stored frames. The full numerical, boundary, cache and concurrent suites provide broader validation.", "",
              "Raw configuration, effective mathematical parameters, source/native hashes and resource settings are retained in the JSON companion."]
    return "\n".join(lines) + "\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--results", type=Path, nargs="+", required=True)
    parser.add_argument("--binary", type=Path, default=ROOT / "build/dfttestvk.dll")
    parser.add_argument("--source-sha", default=SOURCE_SHA)
    parser.add_argument("--min-repeats", type=int, default=3)
    parser.add_argument("--min-seconds", type=float, default=3)
    parser.add_argument("--int-atol", type=float, default=1)
    parser.add_argument("--float-atol", type=float, default=2e-5)
    parser.add_argument("--references", default="auto",
                        help="auto checks CPU/NVRTC backends present per directory; or specify cpu,nvrtc/nvrtc")
    parser.add_argument("--allow-partial", action="store_true", help="produce an interim report without treating missing repeats as terminal")
    parser.add_argument("--output", type=Path, required=True, help="new JSON; Markdown uses same stem")
    args = parser.parse_args()
    if args.references != "auto" and any(name not in ("cpu", "nvrtc") for name in args.references.split(",")):
        parser.error("--references must be auto or a comma-separated subset of cpu,nvrtc")
    if args.output.exists() or args.output.with_suffix(".md").exists():
        parser.error("Use a new analysis output filename")
    expected_vk_sha = file_sha(args.binary)
    runs, collection_errors = [], []
    for directory in args.results:
        collected, errors = collect(directory, expected_vk_sha, args.source_sha, args.min_seconds)
        for run in collected:
            run["directory"] = directory.name
        runs += collected
        collection_errors += errors
    if not runs:
        parser.error("No completed benchmark JSON was found")
    grouped = defaultdict(list)
    for run in runs:
        grouped[(run["directory"], run["name"])].append(run)
    summaries, comparisons = [], []
    for (directory, name), rows in sorted(grouped.items()):
        issues = sorted({issue for row in rows for issue in row["issues"]})
        repeats = [row["repeat"] for row in rows]
        if len(rows) < args.min_repeats:
            issues.append(f"Only {len(rows)} of {args.min_repeats} required repeats are present")
        if None in repeats or len(set(repeats)) != len(repeats):
            issues.append("Repeat identifiers are missing or duplicated")
        if any(workload(row) != workload(rows[0]) or row["config"] != rows[0]["config"] for row in rows[1:]):
            issues.append("Configuration/workload changed between repeats")
        if any(row["native_sha256"] != rows[0]["native_sha256"] for row in rows[1:]):
            issues.append("Native binary changed between repeats")
        memory = [row["telemetry"].get("sampled_max_vram_mib") for row in rows]
        summaries.append({"directory": directory, "name": name, "backend": rows[0]["backend"],
                          "config": rows[0]["config"], "effective_workload": workload(rows[0]),
                          "fps": [row["fps"] for row in rows], "seconds": [row["seconds"] for row in rows],
                          "native_sha256": sorted({row["native_sha256"] for row in rows if row["native_sha256"]}),
                          "source_sha256": rows[0]["source_sha256"],
                          "sampled_max_vram_mib": max((x for x in memory if x is not None), default=None),
                          "issues": issues, "protocol_passed": not issues})
    for candidate in [run for run in runs if run["backend"] in ("vk", "vk-resident")]:
        requested_refs = (sorted({run["backend"] for run in runs if run["directory"] == candidate["directory"]
                                  and run["backend"] in ("cpu", "nvrtc")}) if args.references == "auto"
                          else args.references.split(","))
        if not requested_refs:
            comparisons.append({"candidate": candidate["file"], "reference": "CPU/NVRTC",
                                "passed": False, "error": "No selected reference backend is present"})
        for reference_backend in requested_refs:
            references = [run for run in runs if run["directory"] == candidate["directory"] and
                          run["backend"] == reference_backend and run["repeat"] == candidate["repeat"] and
                          workload(run) == workload(candidate)]
            if len(references) != 1:
                comparisons.append({"candidate": candidate["file"], "reference": reference_backend,
                                    "passed": False, "error": "Matching reference repeat is missing or ambiguous"})
                continue
            try:
                comparisons.append(compare_samples(references[0], candidate, args.int_atol, args.float_atol))
            except Exception as exc:
                comparisons.append({"candidate": candidate["file"], "reference": references[0]["file"],
                                    "passed": False, "error": str(exc)})
    for run in runs:
        run.pop("data")
    report = {"selected_binary": str(args.binary.resolve()), "selected_vk_sha256": expected_vk_sha,
              "required_source_sha256": args.source_sha, "minimum_repeats": args.min_repeats,
              "minimum_filter_seconds": args.min_seconds, "allow_partial": args.allow_partial,
              "selected_references": args.references,
              "groups": summaries, "runs": runs, "collection_errors": collection_errors,
              "sample_comparisons": comparisons,
              "complete_and_passed": not collection_errors and all(row["protocol_passed"] for row in summaries)
                                     and all(row["passed"] for row in comparisons)}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2), encoding="utf-8")
    args.output.with_suffix(".md").write_text(render_markdown(report), encoding="utf-8")
    print(json.dumps({"groups": len(summaries), "runs": len(runs), "sample_pairs": len(comparisons),
                      "complete_and_passed": report["complete_and_passed"], "report": str(args.output)}, indent=2))
    return 0 if report["complete_and_passed"] or args.allow_partial else 1


if __name__ == "__main__":
    sys.exit(main())
