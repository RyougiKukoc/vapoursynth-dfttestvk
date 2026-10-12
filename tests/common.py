"""Shared fixtures and explicitly selected backend construction."""
from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path
import numpy as np
import vapoursynth as vs

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_DLL = ROOT / "build" / ("dfttestvk.dll" if os.name == "nt" else "dfttestvk.so")
CUDA_REFERENCE = ROOT / "reference" / "backend-artifacts" / "dfttest2-cu129-win64" / "dfttest2"
_DLL_DIRECTORIES = []
_DEVICE_SELECTED = False
FORMATS = {name.lower(): getattr(vs, name) for name in
           ("GRAY8", "GRAY9", "GRAY10", "GRAY12", "GRAY14", "GRAY16", "GRAYH", "GRAYS",
            "YUV420P8", "YUV420P10", "YUV420P16", "YUV444PH", "YUV444PS", "RGB24", "RGB48", "RGBH", "RGBS")}


def select_device(core):
    """Select an explicitly requested Vulkan device before any GPU operation."""
    global _DEVICE_SELECTED
    if _DEVICE_SELECTED:
        return
    expected = os.environ.get("DFTTESTVK_DEVICE_UUID")
    index = os.environ.get("DFTTESTVK_DEVICE")
    if expected:
        expected = expected.lower().removeprefix("gpu-").replace("-", "")
        choices = [d for d in core.vulkan_devices if d["uuid"].lower().replace("-", "") == expected]
        if len(choices) != 1:
            raise RuntimeError("Requested Vulkan UUID is missing or ambiguous: " + expected)
        if not choices[0]["usable"]:
            raise RuntimeError("Requested Vulkan device is unusable: " + choices[0].get("reason", ""))
        core.set_vulkan_device(choices[0]["index"])
    elif index is not None:
        core.set_vulkan_device(int(index))
    _DEVICE_SELECTED = True


def sha256(path):
    with Path(path).open("rb") as file:
        return hashlib.file_digest(file, "sha256").hexdigest()


def environment(core):
    plugins = []
    for plugin in core.plugins():
        if plugin.namespace.startswith("dfttest") or plugin.namespace == "std":
            path = Path(plugin.plugin_path) if plugin.plugin_path else None
            plugins.append({"namespace": plugin.namespace, "path": str(path) if path else None,
                            "sha256": sha256(path) if path and path.is_file() else None,
                            "functions": {f.name: f.signature for f in plugin.functions()}})
    return {"plugins": plugins, "core": str(core), "numpy": np.__version__,
            "diagnostic_environment": {k: v for k, v in sorted(os.environ.items()) if k.startswith("DFTTESTVK_")},
            "validation_environment": {k: v for k, v in sorted(os.environ.items())
                                       if k.startswith("VK_LAYER_") or k == "VS_VULKAN_VALIDATION"}}


def load_native(core, dll=DEFAULT_DLL):
    select_device(core)
    dll = Path(dll).resolve()
    if not dll.is_file():
        raise FileNotFoundError(f"Build the plugin first: {dll}")
    loaded = next((p for p in core.plugins() if p.namespace == "dfttestvk"), None)
    if loaded is None:
        core.std.LoadPlugin(path=str(dll))
    elif Path(loaded.plugin_path).resolve() != dll:
        raise RuntimeError(f"Unexpected autoloaded DLL: {loaded.plugin_path}; wanted {dll}")
    return dll


def make_filter(core, source, backend, params=None, dll=DEFAULT_DLL, options=None):
    params, options = dict(params or {}), dict(options or {})
    if backend == "source":
        return source
    if backend in ("vk", "vk-resident"):
        load_native(core, dll)
        if backend == "vk-resident":
            gpu = core.std.GPUUpload(source)
            return core.std.GPUDownload(core.dfttestvk.DFTTestGPU(gpu, **params, **options))
        return core.dfttestvk.DFTTest(source, **params, **options)
    if backend == "original":
        return core.dfttest.DFTTest(source, **params, **options)
    import dfttest2
    selected = {"cpu": dfttest2.Backend.CPU,
                "cuda": dfttest2.Backend.cuFFT,
                "nvrtc": dfttest2.Backend.NVRTC}
    if backend not in selected:
        raise ValueError(f"Unknown backend {backend}")
    if backend in ("cuda", "nvrtc"):
        namespace = "dfttest2_" + backend
        if not any(p.namespace == namespace for p in core.plugins()):
            if not CUDA_REFERENCE.is_dir():
                raise FileNotFoundError(f"Staged CUDA baseline missing: {CUDA_REFERENCE}")
            if os.name == "nt" and not _DLL_DIRECTORIES:
                _DLL_DIRECTORIES.extend([os.add_dll_directory(str(CUDA_REFERENCE)),
                                         os.add_dll_directory(str(CUDA_REFERENCE / "vsmlrt-cuda"))])
            core.std.LoadPlugin(path=str(CUDA_REFERENCE / (namespace + ".dll")))
    return dfttest2.DFTTest(source, **params, backend=selected[backend](**options))


def fixture(core, fmt="grays", width=48, height=40, length=5,
            pattern="noise", seed=20261012):
    """Nonconstant planes/frames with deterministic source properties."""
    format_id = FORMATS[fmt.lower()]
    blank = core.std.BlankClip(width=width, height=height, length=length,
                               format=format_id, fpsnum=24, fpsden=1)
    arrays = []
    rng = np.random.default_rng(seed)
    with blank.get_frame(0) as f:
        for n in range(length):
            frame = []
            for p in range(f.format.num_planes):
                h, w = np.asarray(f[p]).shape
                y, x = np.mgrid[:h, :w]
                if pattern == "constant":
                    a = np.full((h, w), 0.25 + 0.08 * p + 0.03 * n)
                elif pattern == "ramp":
                    a = 0.1 + 0.6 * (x / max(w - 1, 1)) + 0.08 * np.sin((y + n) / 4)
                elif pattern == "impulse":
                    a = np.zeros((h, w)) + 0.2
                    a[(n * 7 + p) % h, (n * 11 + p) % w] = 0.9
                    # Exercise reflected borders as well as interior impulses.
                    a[0, 0] = 0.8
                    a[-1, -1] = 0.05
                elif pattern == "noise":
                    a = 0.2 + 0.45 * x / max(w - 1, 1) + 0.1 * np.sin((y + n) / 3)
                    a += rng.normal(0, 0.05, (h, w))
                else:
                    raise ValueError(pattern)
                a = np.clip(a, 0.0, 1.0)
                if f.format.sample_type == vs.FLOAT:
                    # Native float chroma has [-0.5,0.5] storage; denoiser acts
                    # on those samples directly, without range recentering.
                    if f.format.color_family == vs.YUV and p:
                        a -= 0.5
                    a = a.astype(np.asarray(f[p]).dtype)
                else:
                    a = np.floor(a * ((1 << f.format.bits_per_sample) - 1) + 0.5).astype(np.asarray(f[p]).dtype)
                frame.append(a)
            arrays.append(frame)

    def supply(n, f):
        out = f.copy()
        for p, array in enumerate(arrays[n]):
            np.copyto(np.asarray(out[p]), array)
        out.props["DFTTestFixtureFrame"] = n
        out.props["DFTTestFixtureSeed"] = seed
        out.props["DFTTestFixturePattern"] = pattern.encode()
        out.props["_DurationNum"] = 1
        out.props["_DurationDen"] = 24
        return out
    return core.std.ModifyFrame(blank, clips=blank, selector=supply), arrays


def metrics(actual, expected, bits, floating):
    delta = actual.astype(np.float64) - expected.astype(np.float64)
    if not np.isfinite(actual).all() or not np.isfinite(expected).all():
        raise ValueError("Nonfinite output")
    scale = 255.0 if floating else 1.0 / (1 << (bits - 8))
    absolute = np.abs(delta)
    result = {"rms": float(np.sqrt(np.mean(delta * delta))),
            "max": float(absolute.max(initial=0)), "p99": float(np.quantile(absolute, .99)),
            "different_fraction": float(np.mean(delta != 0)),
            "max_8bit": float(absolute.max(initial=0) * scale)}
    if floating and bits == 16:
        def ordered(array):
            values = array.astype(np.float16).view(np.uint16).astype(np.int32)
            # Collapse +0/-0 to the same point in the ordered finite lattice.
            return np.where(values & 0x8000, 0x10000 - values, values + 0x8000)
        result["max_half_ulps"] = int(np.max(np.abs(ordered(actual) - ordered(expected)), initial=0))
    return result


def write_json(path, result):
    path = Path(path)
    if path.exists():
        raise FileExistsError(f"Use a new result filename: {path}")
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(result, indent=2, ensure_ascii=False), encoding="utf-8")
