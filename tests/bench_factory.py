"""Project factory for holywu-vulkan/scripts/benchmark_vs.py.

All backends return completed ordinary CPU frames. The resident Vulkan path
includes GPUUpload and GPUDownload explicitly. Inputs are preloaded before
timing; output requests use fresh frame numbers rather than cached outputs.
"""
from __future__ import annotations

from pathlib import Path
import os
import numpy as np
import vapoursynth as vs
from common import DEFAULT_DLL, FORMATS, fixture, make_filter, sha256


def make(core, config, length):
    fmt = config.get("format", "gray16").lower()
    width, height = config.get("width", 1920), config.get("height", 1080)
    count = config.get("held_frames", 24)
    raw_path = config.get("raw_yuv420p16")
    if raw_path:
        raw_path = Path(raw_path).resolve()
        rw, rh = config.get("raw_width", 1920), config.get("raw_height", 1080)
        stride = rw * rh * 3 // 2
        if rw % 2 or rh % 2 or raw_path.stat().st_size % (stride * 2):
            raise ValueError("Raw file must contain complete even-dimension YUV420P16 frames")
        raw = np.memmap(raw_path, dtype="<u2", mode="r")
        total = raw.size // stride
        if count > total or count < 1:
            raise ValueError("held_frames must be between 1 and raw frame count")
        # Consecutive frames preserve temporal neighbors; modulo wrap is
        # disclosed because it introduces one discontinuity per held cycle.
        start = config.get("raw_start", 0)
        if not 0 <= start <= total - count:
            raise ValueError("raw_start + held_frames exceeds available source")
        indices = list(range(start, start + count))
        blank = core.std.BlankClip(width=rw, height=rh, format=vs.YUV420P16, length=count,
                                  fpsnum=24, fpsden=1)
        def supply(n, f):
            out = f.copy()
            offset = indices[n] * stride
            for p in range(3):
                array = np.asarray(out[p])
                np.copyto(array, raw[offset:offset + array.size].reshape(array.shape))
                offset += array.size
            out.props.update(_ColorRange=0, _Matrix=1, _Transfer=1, _Primaries=1,
                             _ChromaLocation=0, _FieldBased=0)
            return out
        raw_source = core.std.ModifyFrame(blank, clips=blank, selector=supply)
        family = core.get_video_format(FORMATS[fmt]).color_family
        if family == vs.GRAY:
            raw_source = core.std.ShufflePlanes(raw_source, planes=0, colorfamily=vs.GRAY)
        if family == vs.RGB:
            prepared = core.resize.Spline36(raw_source, width=width, height=height,
                                            format=FORMATS[fmt], matrix_in_s="709")
        else:
            prepared = core.resize.Spline36(raw_source, width=width, height=height, format=FORMATS[fmt])
        source_info = {"kind": "raw-real-video", "path": str(raw_path),
                       "sha256": sha256(raw_path), "raw_width": rw, "raw_height": rh,
                       "raw_format": "YUV420P16", "available_frames": total,
                       "source_frame_indices": indices, "held_frames": count,
                       "periodic_temporal_wrap": True,
                       "upscaled": width > rw or height > rh,
                       "preload_and_resize_excluded_from_timing": True}
    else:
        prepared, _ = fixture(core, fmt, width, height, count, config.get("pattern", "noise"),
                              config.get("seed", 20261012))
        source_info = {"kind": "deterministic-synthetic", "held_frames": count,
                       "seed": config.get("seed", 20261012), "periodic_temporal_wrap": True,
                       "preload_excluded_from_timing": True}
    held = [prepared.get_frame(n) for n in range(count)]
    source_info["project_helpers_sha256"] = {
        str(p.resolve()): sha256(p) for p in [Path(__file__), Path(__file__).with_name("common.py")]}
    source_info["diagnostic_environment"] = {key: value for key, value in sorted(os.environ.items())
                                              if key.startswith("DFTTESTVK_")}
    source_info["selected_backend_options"] = config.get("options", {})
    if config.get("backend") in ("cuda", "nvrtc"):
        from common import CUDA_REFERENCE
        source_info["cuda_dependencies_sha256"] = {
            str(p.resolve()): sha256(p) for p in (CUDA_REFERENCE / "vsmlrt-cuda").glob("*.dll")}
    template = core.std.BlankClip(prepared, length=length)
    source = core.std.ModifyFrame(template, clips=template, selector=lambda n, f: held[n % count])
    output = make_filter(core, source, config.get("backend", "vk"), config.get("params", {}),
                         config.get("dll", DEFAULT_DLL), config.get("options", {}))
    return {"source": source, "output": output, "held": held, "source_info": source_info}
