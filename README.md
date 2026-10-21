# DFTTestVK

## Recommended installation: api4-wheels

Install the Windows or Linux x86-64 package into the Python environment used by
VapourSynth:

```console
python -m pip install --extra-index-url https://aliceteaparty.github.io/vapoursynth-api4-wheels/simple/ vapoursynth-dfttestvk
```

[api4-wheels](https://github.com/AliceTeaParty/vapoursynth-api4-wheels) provides
the native plugin, autoload manifest and licenses. Use Python 3.12 or newer,
VapourSynth R81 and a Vulkan 1.4 device/driver. Linux wheels require glibc 2.28
or newer. After installation, `vs.core.dfttestvk` is available automatically.
The central repository maintains wheel packaging and publication; this repository
maintains the native source and validation tools.

## Usage

DFTTestVK implements DFTTest frequency-domain denoising on VapourSynth's
Vulkan device. It exposes a familiar CPU-clip entry point and a GPU-resident
entry point for graphs that keep frames on the device. CUDA is not required.

```python
import vapoursynth as vs
core = vs.core

# Upload, denoise and download are composed by this entry point.
output = core.dfttestvk.DFTTest(clip, sigma=8, sbsize=16, sosize=12, tbsize=3)

# Keep intermediate frames resident when chaining GPU filters.
gpu_clip = core.std.GPUUpload(clip)
gpu_output = core.dfttestvk.DFTTestGPU(gpu_clip, sigma=8, tbsize=3)
output = core.std.GPUDownload(gpu_output)
```

## Algorithm and parameters

The default configuration uses 16x16 blocks, spatial overlap 12, three
temporal frames, Hann spatial windows, rectangular temporal windows and
mean-preserving Wiener filtering. For temporal 16x16 processing, an internal
cache computes each source frame's spatial FFT once and reuses it for nearby
outputs. The finishing dispatch performs temporal transforms, coefficient
filtering and center-slice inverse transforms. A second dispatch gathers their
overlapping output without floating-point atomics. `cache=0` selects the direct
fused path, which computes all input transforms for each output request.

The public parameters use DFTTest names: `ftype`, `sigma`, `sigma2`, `pmin`,
`pmax`, `sbsize`, `smode`, `sosize`, `tbsize`, `tmode`, `tosize`, `swin`, `twin`,
`sbeta`, `tbeta`, `zmean`, `f0beta`, `slocation`, `ssx`, `ssy`, `sst`, `ssystem`
and `planes`, plus `cache`, `cache_frames` and `cache_mb` for GPU spectrum
reuse. The fast path covers `sbsize=16`, `tbsize=1,3,5,7`, all five
filter types, all 12 spatial/temporal windows and frequency-dependent sigma.
Other `sbsize` values in 1–64 use a generic transform path, with radix-2 FFTs
for power-of-two blocks and direct DFTs otherwise. Device workgroup, shared
memory and storage-buffer limits still apply. The 16-point compute kernels
use FFTW codelets adapted from DFTTest2's CUDA implementation, with explicit
static indexing. The default workgroup starts at 16 tiles and selects
16/8/4/2 tiles according to device limits.
Omitted `planes` or `planes=None` processes every plane. Selected unprocessed planes
share their source pixels and preserve frame properties.

Use `smode=1, tmode=0, tosize=0`; temporal overlap-add and `nlocation/alpha`
noise-spectrum estimation are outside this interface. Spatial overlap must
satisfy `0 <= sosize < sbsize`; overlap greater than 50% requires
`sbsize % (sbsize-sosize) == 0`. Input has constant format/dimensions: gray,
YUV or RGB; 8–16-bit integers and 16/32-bit floats. Float16 storage needs the
device's optional `shaderFloat16` feature; transform arithmetic remains FP32.

| Cache parameter | Behavior |
| --- | --- |
| `cache=-1` (default) | Automatically reuse spatial FFTs for `sbsize=16,tbsize>1`; use the direct path when the creation-time budget cannot hold one spectrum |
| `cache=0` | Compute each request directly |
| `cache=1` | Require spectrum caching; `sbsize` must be 16 and the budget must hold at least one spectrum |
| `cache_frames=12` | Maximum ready entries retained by the LRU; valid range 1–128 |
| `cache_mb=0` | Select the creation-time cache budget from one quarter of the core's VRAM limit; positive values replace that automatic choice with a budget in MiB |

The budget can reduce the requested entry count. Pending work and spectra
still used by submitted GPU requests remain alive beyond LRU eviction, so
these controls describe retained cache capacity, not the entire graph's VRAM
usage. At 1080p with default overlap, one cached luma spectrum occupies about
146 MiB and all YUV420 planes about 220 MiB. See
[docs/temporal-cache-design.md](docs/temporal-cache-design.md) for layout,
ownership and request-order behavior. On discrete devices, preprocessing
copies source pixels to a device-local buffer before repeatedly loading them
for the FFT; unified-memory devices skip that copy. The source/output clip
format is unchanged. Explicit cache budgets can exceed the automatic choice,
while the core's global GPU allocation and submission limits remain active.

Spatial borders reflect without repeating their endpoints; temporal borders
repeat the first/last frame. Short clips and tiny planes have defined behavior.
The algorithm restores the full windowed mean spectrum when `zmean=True`.
Sigma is the original DFTTest power parameter and is not squared. See
[docs/algorithm.md](docs/algorithm.md) for formulas, reference differences and
the known DFTTest2 CPU synthesis-window defect affecting nonrectangular `twin`.

## Build

Use CMake 3.20 or newer, a C++20 compiler, Ninja or another CMake generator,
Python 3.11 or newer and Git. Runtime requires the VapourSynth R81 GPU API and a Vulkan 1.4
device/driver. The Vulkan function table must match the installed core's
headers; the pinned SDK below matches the R81 environment used for this work.

```powershell
python tools/fetch_dependencies.py --with-vapoursynth
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release `
  -DVAPOURSYNTH_INCLUDE_DIR="$PWD/dependencies/vapoursynth/include"
cmake --build build
```

If using the installed SDK instead, omit `VAPOURSYNTH_INCLUDE_DIR` and select
the correct Python with `-DPython3_EXECUTABLE=C:/path/to/python.exe`; CMake
queries `vapoursynth.get_include()`. `VULKAN_HEADERS` can select an existing
Vulkan-Headers include directory. Only headers are needed; no Vulkan, CUDA or
FFT library is linked. On Windows the result is `build/dfttestvk.dll` with
Ninja, or under the selected configuration directory with a multi-config
generator. Load it explicitly during development.

The tested Windows toolchain is MinGW-w64 GCC 16.2.0, CMake 4.4.2 and Ninja
1.13.2; CMake uses `-O3` and static MinGW runtime linkage. The MSVC branch uses
`/O2 /fp:precise`. Shader source is compiled at filter creation by the core,
so graph creation time is distinct from steady-state throughput.
The local clean rebuild with freshly fetched pinned SDKs produced the same
DLL SHA256 as the development build; details are in [docs/build.md](docs/build.md).

Linux x86_64 GCC/Clang builds and R81 software-Vulkan execution were also
verified in Docker with Python 3.13 / Debian trixie; see [docs/linux-validation.md](docs/linux-validation.md).

## Reproduce transforms and references

The generated shader header is committed, so building does not require
fetching the reference implementations. To inspect/regenerate it:

```powershell
python tools/fetch_references.py
python tools/generate_shaders.py dependencies/dfttest2-api4/plugins/vapoursynth-dfttest2/nvrtc_source/dft_kernels.hpp
python tools/verify_fft_codelets.py --compiler C:/path/to/g++.exe
```

The fetch scripts refuse to reset dirty or differently pinned source trees.
`reference/source-lock.json` records source and artifact commits, binary
versions and SHA256 hashes. Optional Windows NVRTC/cuFFT references are staged
separately, without changing the globally installed package:

```powershell
python tools/fetch_references.py --skip-sources --gpu-reference
python tools/fetch_references.py --skip-sources --gpu-reference --verify-only
```

## Validation and measurements

See [tests/README.md](tests/README.md) for the independent NumPy FFT oracle,
all-pixel comparisons, request-order/concurrent checks and rotated
fresh-process performance runs. A quick numerical check after building is:

```powershell
python tests/correctness.py --backend vk --against oracle --suite smoke --output tests/results/vk-smoke.json
```

Compare complete CPU-output graphs against original DFTTest, DFTTest2 CPU,
NVRTC and cuFFT using the same input, parameters, request concurrency and
output completion rule. A GPU-resident graph and a CPU-output graph have
different transfer costs; keep those measurements explicit. Profiling
`DFTTESTVK_PROFILE=1` emits per-plane fused/gather GPU durations to stderr and
adds timestamp overhead, so leave it unset for reported throughput.

On the measured RTX 3090 Ti, default 1080p Gray16 three-frame denoising was
about 4.8% faster than DFTTest2 NVRTC with eight streams. These medians come
from three fresh-process runs with complete CPU outputs and required
transfers included:

| Implementation | Median frames/s |
| --- | ---: |
| Original DFTTest CPU | 29.58 |
| DFTTest2 CPU | 60.65 |
| DFTTest2 cuFFT | 102.95 |
| DFTTest2 NVRTC | 503.00 |
| DFTTestVK | 527.24 |

Version 0.1.1 consumes cached frequency rows one at a time for `tbsize=3`,
shortening the lifetime of private coefficient arrays. A new matched A/B on
RTX 3090 Ti measured 522.63 → 556.36 fps for Gray16 (+6.5%) and
306.30 → 346.42 fps for all YUV420P16 planes (+13.1%), including complete CPU
outputs and transfers. Other temporal sizes retain their earlier schedule.
See [the optimization report](docs/r2-optimization.md) for raw repetitions,
numerical checks and the rejected T=7 experiment. The earlier comparison table
above describes the original 0.1.0 measurement set.

The completed T=7 comparisons showed about 29% higher median throughput for
Gray16 and 38% for all YUV420P16 planes. NVRTC was faster at T=1 in both
layouts. These results apply to the measured workloads and driver clock
policy; other GPUs require their own measurements. See
[docs/benchmark.md](docs/benchmark.md) for repeat ranges, exact settings,
output comparisons, memory telemetry and the observed memory-clock difference
between Vulkan and CUDA.

## License

GNU GPL version 3. See [LICENSE](LICENSE) and
[THIRD_PARTY.md](THIRD_PARTY.md) for source attribution.
