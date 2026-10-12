"""Record actual R81 plugin loading, SDK agreement and Linux Vulkan devices."""
import hashlib
import json
from pathlib import Path
import platform
import sys

import vapoursynth as vs

root = Path("/src")
output = Path("/out")
core = vs.core
core.std.LoadPlugin(str(output / "build-gcc/dfttestvk.so"))
header = Path(vs.get_include()) / "VSVulkan4.h"
reference = root / "dependencies/vapoursynth/include/VSVulkan4.h"
assert header.read_text("utf-8") == reference.read_text("utf-8")
plugin = next(p for p in core.plugins() if p.namespace == "dfttestvk")
record = {"python": sys.version, "platform": platform.platform(), "vapoursynth": str(vs.__version__),
          "core": str(core), "header_matches_pinned_sdk": True,
          "header_sha256": hashlib.sha256(header.read_bytes()).hexdigest(),
          "plugin_path": plugin.plugin_path,
          "plugin_sha256": hashlib.sha256(Path(plugin.plugin_path).read_bytes()).hexdigest(),
          "functions": {f.name: f.signature for f in plugin.functions()},
          "vulkan_devices": core.vulkan_devices, "vulkan_device_info": core.vulkan_device_info,
          "hardware_gpu_performance_verified": False}
path = output / "runtime-info.json"
if path.exists():
    raise FileExistsError(path)
path.write_text(json.dumps(record, indent=2), "utf-8")
print(json.dumps(record, indent=2))
