"""Run Linux GPU correctness checks serially with fresh, preserved evidence.

The selected UUID is passed to tests/common.py. This runner does not benchmark
performance. Optional validation layers run in additional fresh processes.
"""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import sys
import time


def sha256(path: Path) -> str:
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def resolve_under(root: Path, path: Path) -> Path:
    return (path if path.is_absolute() else root / path).resolve()


def failure_details(record: dict) -> list[dict]:
    failed = []
    if record.get("error"):
        failed.append({"error": record["error"]})
    for row in record.get("cases", []):
        if not row.get("passed", False):
            case = row.get("case", {})
            failed.append({"case": case.get("name", case), "error": row.get("error"),
                           "traceback": row.get("traceback"), "failures": row.get("failures"),
                           "metrics": [m for m in row.get("metrics", []) if not m.get("passed", False)]})
    failed.extend(record.get("failures", []))
    return failed


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--plugin", type=Path, required=True)
    parser.add_argument("--device-uuid", required=True,
                        help="NVIDIA UUID, with or without the GPU- prefix")
    parser.add_argument("--results", type=Path, required=True,
                        help="directory; preexisting unrelated metadata is preserved")
    parser.add_argument("--layer-path", type=Path,
                        help="optional Linux Khronos validation manifest directory")
    parser.add_argument("--timeout", type=int, default=600, help="seconds per child process")
    parser.add_argument("--threads", type=int, default=24)
    parser.add_argument("--telemetry", action="store_true",
                        help="sample only the selected NVIDIA UUID; outside performance testing")
    parser.add_argument("--dry-run", action="store_true", help="print commands without running or writing")
    args = parser.parse_args()
    if args.timeout < 1 or args.threads < 1:
        parser.error("Timeout and thread count must be positive")
    root = args.root.resolve(strict=True)
    plugin = resolve_under(root, args.plugin)
    if not plugin.is_file():
        parser.error(f"Plugin does not exist: {plugin}")
    results = resolve_under(root, args.results)
    uuid = args.device_uuid.lower().removeprefix("gpu-")
    if not re.fullmatch(r"[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}", uuid):
        parser.error("Pass a complete NVIDIA GPU UUID")
    env = os.environ.copy()
    env["DFTTESTVK_DEVICE_UUID"] = "GPU-" + uuid
    env.pop("DFTTESTVK_DEVICE", None)
    env["PYTHONUNBUFFERED"] = "1"
    # Validation is an explicit extra stage. The ordinary numerical stages
    # must not accidentally inherit diagnostic validation from the caller.
    for name in ("VS_VULKAN_VALIDATION", "VK_LAYER_ENABLES", "VK_LAYER_SETTINGS_PATH",
                 "VK_INSTANCE_LAYERS", "VK_LOADER_DEBUG"):
        env.pop(name, None)
    plan = []

    def add(name, script, arguments, count=None, validation=False):
        script_path = root / script
        if not script_path.is_file():
            parser.error(f"Required test script is missing: {script_path}")
        output = results / (name + ".json")
        command = [sys.executable, str(script_path), *arguments,
                   "--dll", str(plugin), "--output", str(output)]
        plan.append({"name": name, "command": command, "output": str(output),
                     "log": str(results / (name + ".log")),
                     "expected_case_count": count, "validation": validation})

    threads = ["--threads", str(args.threads)]
    for suite, count in (("full", 98), ("tiny", 4), ("degenerate", 24), ("half", 12)):
        add("vk-" + suite, "tests/correctness.py",
            ["--backend", "vk", "--against", "oracle", "--suite", suite, *threads], count)
    add("vk-generic", "tools/probe_generic.py", ["--backend", "vk", *threads], 67)
    add("vk-cache", "tests/cache.py", ["--backend", "vk", *threads], 15)
    stress = ["--backend", "vk", "--instances", "6", "--frames", "13", "--rounds", "6", *threads]
    for cap in (1, 2):
        add(f"vk-stress-cap{cap}", "tests/stress.py", [*stress, "--cache", "1", "--cache-frames", str(cap)])
        plan[-1]["expected_concurrent_requests"] = 486
    add("vk-resident-smoke", "tests/correctness.py",
        ["--backend", "vk-resident", "--against", "oracle", "--suite", "smoke", *threads], 8)
    add("vk-invalid", "tests/invalid.py", ["--backend", "vk"], 96)
    layer_files = {}
    layer = None
    if args.layer_path:
        layer_path = resolve_under(root, args.layer_path)
        for manifest in sorted(layer_path.glob("*.json")):
            try:
                definition = json.loads(manifest.read_text(encoding="utf-8"))["layer"]
            except (KeyError, ValueError):
                continue
            if definition.get("name") == "VK_LAYER_KHRONOS_validation":
                layer = {"manifest": str(manifest), "name": definition["name"],
                         "api_version": definition.get("api_version"), "path": str(layer_path)}
                layer_files[str(manifest)] = sha256(manifest)
                library = Path(definition["library_path"])
                library = library if library.is_absolute() else manifest.parent / library
                if library.is_file():
                    layer_files[str(library.resolve())] = sha256(library)
                break
        if layer is None:
            parser.error(f"Khronos validation manifest is missing in {layer_path}")
        add("sync-stress-cap1", "tests/stress.py", [*stress, "--cache", "1", "--cache-frames", "1"], validation=True)
        plan[-1]["expected_concurrent_requests"] = 486
        add("sync-cache", "tests/cache.py", ["--backend", "vk", *threads], 15, validation=True)
    summary_path = results / "suite-summary.json"
    telemetry_path = results / "selected-gpu-telemetry.csv"
    settings_dir = results / "validation-settings"
    reserved = [summary_path, *(Path(row[k]) for row in plan for k in ("output", "log"))]
    if args.telemetry:
        reserved.append(telemetry_path)
    if layer:
        reserved.append(settings_dir)
    collisions = [str(path) for path in reserved if path.exists()]
    if collisions:
        parser.error("Refusing to overwrite evidence: " + ", ".join(collisions))
    if args.dry_run:
        print(json.dumps({"device_uuid": "GPU-" + uuid, "plan": plan}, indent=2))
        return 0
    results.mkdir(parents=True, exist_ok=True)
    if layer:
        settings_dir.mkdir(exist_ok=False)
        (settings_dir / "vk_layer_settings.txt").write_text(
            "khronos_validation.report_flags = error,warn\n"
            "khronos_validation.enable_message_limit = false\n"
            "khronos_validation.validate_sync = true\n"
            "khronos_validation.thread_safety = true\n", encoding="utf-8")
    metadata_files = {}
    for directory in {results, results.parent, root / "results"}:
        if directory.is_dir():
            for path in directory.glob("*.json"):
                if "environ" in path.name or "runtime" in path.name:
                    metadata_files[str(path.resolve())] = sha256(path)
    source_paths = {root / "src/dfttestvk.cpp", root / "tests/common.py", root / "tests/oracle.py",
                    root / "tests/correctness.py", Path(__file__).resolve(),
                    *(Path(row["command"][1]) for row in plan)}
    source_hashes = {str(path.relative_to(root)): sha256(path) for path in sorted(source_paths)
                     if path.is_file() and path.is_relative_to(root)}
    git = subprocess.run(["git", "rev-parse", "HEAD"], cwd=root, text=True,
                         stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False) if shutil.which("git") else None
    report = {"started_utc": datetime.now(timezone.utc).isoformat(), "python": sys.version,
              "python_executable": sys.executable, "platform": platform.platform(),
              "libc": platform.libc_ver(), "root": str(root),
              "selected_device_uuid": "GPU-" + uuid,
              "plugin": {"path": str(plugin), "sha256": sha256(plugin)},
              "git_head": git.stdout.strip() if git and git.returncode == 0 else None,
              "source_sha256": source_hashes, "metadata_sha256": metadata_files,
              "scope": "serial hardware numerical/cache/request/residency validation",
              "performance_verified": False, "timeout_seconds_per_test": args.timeout,
              "validation_layer": layer, "validation_layer_files_sha256": layer_files,
              "diagnostic_environment": {k: v for k, v in sorted(env.items()) if k.startswith("DFTTESTVK_")},
              "tests": [], "passed": False}
    telemetry = None
    telemetry_file = None
    started = time.perf_counter()
    try:
        if args.telemetry:
            executable = shutil.which("nvidia-smi")
            if not executable:
                raise RuntimeError("nvidia-smi is unavailable; omit --telemetry or install it")
            command = [executable, "--id=GPU-" + uuid,
                       "--query-gpu=timestamp,uuid,name,utilization.gpu,utilization.memory,memory.used,power.draw,temperature.gpu",
                       "--format=csv", "--loop=1"]
            telemetry_file = telemetry_path.open("x", encoding="utf-8")
            telemetry = subprocess.Popen(command, stdout=telemetry_file, stderr=subprocess.STDOUT)
            report["telemetry"] = {"command": command, "path": str(telemetry_path)}
        for planned in plan:
            row = dict(planned)
            child_env = env.copy()
            if row["validation"]:
                child_env.update(VS_VULKAN_VALIDATION="1", VK_LAYER_PATH=layer["path"],
                                 VK_LOADER_DEBUG="warn,error,layer",
                                 VK_LAYER_SETTINGS_PATH=str(settings_dir),
                                 VK_LAYER_ENABLES="VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT")
            print(f"START {row['name']}", flush=True)
            before = time.perf_counter()
            with Path(row["log"]).open("x", encoding="utf-8") as log:
                try:
                    completed = subprocess.run(row["command"], cwd=root, env=child_env,
                                               stdout=log, stderr=subprocess.STDOUT,
                                               timeout=args.timeout, check=False)
                    row["returncode"] = completed.returncode
                except subprocess.TimeoutExpired:
                    row.update(returncode=None, timed_out=True,
                               error=f"Child exceeded {args.timeout} seconds and was terminated")
                except Exception as error:
                    row.update(returncode=None, error=f"{type(error).__name__}: {error}")
            row["seconds"] = time.perf_counter() - before
            output = Path(row["output"])
            if output.is_file():
                row["output_sha256"] = sha256(output)
                try:
                    record = json.loads(output.read_text(encoding="utf-8"))
                    row["test_report_passed"] = record.get("passed") is True
                    row["actual_case_count"] = len(record.get("cases", []))
                    row["actual_concurrent_requests"] = record.get("requests")
                    row["failures"] = failure_details(record)
                    row["environment"] = record.get("environment")
                    native = [p for p in record.get("environment", {}).get("plugins", [])
                              if p.get("namespace") == "dfttestvk"]
                    row["native_plugin_matches_requested"] = len(native) == 1 and (
                        Path(native[0]["path"]).resolve() == plugin and
                        native[0].get("sha256") == report["plugin"]["sha256"])
                    selected = record.get("environment", {}).get("diagnostic_environment", {}).get("DFTTESTVK_DEVICE_UUID", "")
                    row["requested_uuid_recorded"] = selected.lower().removeprefix("gpu-").replace("-", "") == uuid.replace("-", "")
                except (ValueError, TypeError, KeyError, AttributeError) as error:
                    row.update(test_report_passed=False, error="Invalid test JSON: " + str(error))
            else:
                row.update(test_report_passed=False)
                row.setdefault("error", "Child did not produce its required JSON report")
            row["log_sha256"] = sha256(Path(row["log"]))
            count = row["expected_case_count"]
            row["coverage_count_matches"] = count is None or row.get("actual_case_count") == count
            if "expected_concurrent_requests" in row:
                row["coverage_count_matches"] &= row.get("actual_concurrent_requests") == row["expected_concurrent_requests"]
            row["passed"] = (row.get("returncode") == 0 and row["test_report_passed"] and
                             row["coverage_count_matches"] and row.get("native_plugin_matches_requested", False) and
                             row.get("requested_uuid_recorded", False))
            if row["validation"]:
                text = Path(row["log"]).read_text(encoding="utf-8", errors="replace")
                row["layer_inserted"] = bool(re.search(
                    r"Insert\s+instance\s+layer\s+[\"']?VK_LAYER_KHRONOS_validation\b", text, re.IGNORECASE))
                row["validation_messages"] = [line for line in text.splitlines()
                                              if "VUID-" in line or "SYNC-HAZARD" in line or
                                              "Validation Error" in line or "UNASSIGNED-Threading" in line]
                row["passed"] &= row["layer_inserted"] and not row["validation_messages"]
            report["tests"].append(row)
            print(f"{'PASS' if row['passed'] else 'FAIL'} {row['name']} seconds={row['seconds']:.3f}", flush=True)
        report["passed"] = len(report["tests"]) == len(plan) and all(row["passed"] for row in report["tests"])
    except Exception as error:
        report["error"] = f"{type(error).__name__}: {error}"
    finally:
        if telemetry is not None:
            report["telemetry"]["premature_returncode"] = telemetry.poll()
            if telemetry.poll() is None:
                telemetry.terminate()
                try:
                    telemetry.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    telemetry.kill()
                    telemetry.wait(timeout=5)
            report["telemetry"]["returncode"] = telemetry.returncode
        if telemetry_file is not None:
            telemetry_file.close()
            report["telemetry"]["sha256"] = sha256(telemetry_path)
        report["seconds"] = time.perf_counter() - started
        report["finished_utc"] = datetime.now(timezone.utc).isoformat()
        with summary_path.open("x", encoding="utf-8") as stream:
            json.dump(report, stream, ensure_ascii=False, indent=2)
            stream.write("\n")
    print(json.dumps({"passed": report["passed"], "completed_tests": len(report["tests"]),
                      "summary": str(summary_path)}, ensure_ascii=False), flush=True)
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
