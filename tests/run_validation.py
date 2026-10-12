"""Run existing correctness/stress tools with an isolated Vulkan validation layer.

This wrapper changes only the child process environment, not machine registry
or global plugin installation. Validation runs must stay outside benchmarks.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import re

ROOT = Path(__file__).resolve().parents[1]


def sha256(path):
    with Path(path).open("rb") as file:
        return hashlib.file_digest(file, "sha256").hexdigest()


BOOTSTRAP = r'''
import ctypes, os, pathlib, runpy, sys
layers = pathlib.Path(sys.argv[1])
directories = [os.add_dll_directory(str(layers))]
libraries = [ctypes.WinDLL(str(layers / name), winmode=8) for name in (
    "libgcc_s_seh-1.dll", "libwinpthread-1.dll", "libstdc++-6.dll",
    "libSPIRV-Tools.dll", "libSPIRV-Tools-opt.dll", "VkLayer_khronos_validation.dll")]
print("Validation bootstrap: isolated layer and dependencies preloaded before VapourSynth", flush=True)
command = sys.argv[2:]
sys.path.insert(0, str(pathlib.Path(command[0]).resolve().parent))
sys.argv = command
runpy.run_path(command[0], run_name="__main__")
'''


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--layers", type=Path, default=ROOT / "dependencies/validation_layers/mingw64/bin")
    ap.add_argument("--output", type=Path, required=True, help="validation environment/log summary")
    ap.add_argument("--sync", action="store_true", help="enable synchronization validation")
    ap.add_argument("--thread-safety", action="store_true")
    ap.add_argument("command", nargs=argparse.REMAINDER, help="Python script and arguments after --")
    args = ap.parse_args()
    command = args.command[1:] if args.command[:1] == ["--"] else args.command
    if not command:
        ap.error("Pass a Python test script and arguments after --")
    if args.output.exists() or args.output.with_suffix(".log").exists():
        ap.error("Use a new validation result filename")
    layers = args.layers.resolve()
    manifest = layers / "VkLayer_khronos_validation.json"
    if not manifest.is_file():
        ap.error(f"Isolated layer not staged: {manifest}")
    definition = json.loads(manifest.read_text(encoding="utf-8"))["layer"]
    env = os.environ.copy()
    env.update(VK_LAYER_PATH=str(layers), VS_VULKAN_VALIDATION="1", VK_LOADER_DEBUG="error,warn,layer")
    env["PATH"] = str(layers) + os.pathsep + env.get("PATH", "")
    settings_dir = args.output.with_suffix(".settings").resolve()
    settings_dir.mkdir(parents=True, exist_ok=False)
    settings = ["khronos_validation.report_flags = error,warn", "khronos_validation.enable_message_limit = false"]
    if args.sync:
        env["VK_LAYER_ENABLES"] = "VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT"
        settings.append("khronos_validation.validate_sync = true")
    if args.thread_safety:
        settings.append("khronos_validation.thread_safety = true")
    (settings_dir / "vk_layer_settings.txt").write_text("\n".join(settings) + "\n", encoding="utf-8")
    env["VK_LAYER_SETTINGS_PATH"] = str(settings_dir)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    log_path = args.output.with_suffix(".log")
    with log_path.open("w", encoding="utf-8") as log:
        completed = subprocess.run([sys.executable, "-c", BOOTSTRAP, str(layers), *command],
                                   env=env, stdout=log, stderr=subprocess.STDOUT)
    output = log_path.read_text(encoding="utf-8", errors="replace")
    # A lack of errors is meaningful only when loader evidence shows the layer
    # was actually loaded and inserted into the Vulkan instance call chain.
    inserted = bool(re.search(r'Insert instance layer\s+"VK_LAYER_KHRONOS_validation"', output))
    vuid_lines = [line for line in output.splitlines() if "VUID-" in line or "SYNC-HAZARD" in line]
    result = {"command": [sys.executable, *command], "returncode": completed.returncode,
              "layer_name": definition["name"], "layer_api_version": definition["api_version"],
              "layer_files_sha256": {str(p): sha256(p) for p in layers.iterdir() if p.suffix in (".dll", ".json")},
              "environment": {k: env[k] for k in ("VK_LAYER_PATH", "VS_VULKAN_VALIDATION", "VK_LOADER_DEBUG", "VK_LAYER_SETTINGS_PATH")},
              "layer_settings": settings,
              "sync_validation": args.sync, "thread_safety_validation": args.thread_safety,
              "log": str(log_path.resolve()), "layer_inserted": inserted,
              "validation_messages": vuid_lines,
              "passed": completed.returncode == 0 and inserted and not vuid_lines}
    args.output.write_text(json.dumps(result, indent=2, ensure_ascii=False), encoding="utf-8")
    print(json.dumps({k: result[k] for k in ("returncode", "layer_inserted", "sync_validation", "passed")}))
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    sys.exit(main())
