# Linux validation in Python 3.13 / Debian trixie

The Linux x86_64 plugin was compiled and loaded on 2026-10-12 inside Docker
Desktop's Linux environment, using `python:3.13-slim-trixie` and the requested
proxy `http://host.docker.internal:7890`. GCC and Clang native builds passed.
VapourSynth R81 loaded both plugins. GCC completed the 98-case full correctness
suite and 15 cache cases; Clang completed eight smoke cases. Compute ran
through Mesa's software Vulkan implementation, with numerical checks against
the independent NumPy FFT oracle. This is Linux build and software-Vulkan functional evidence;
the native A100 result below is the separate hardware-GPU evidence.

The same source was then built natively on Ubuntu 22.04 (glibc 2.35) with
GCC 11.4.0 and Miniforge Python 3.13. R81 selected NVIDIA A100-PCIE-40GB
UUID GPU-3d6249d3-3e1a-3281-6aba-bc2d7a1fb83b, driver 580.95.05 and Vulkan
API 1.4.312. The full correctness, cache, generic, resident, invalid and
stress suite passed; the separate Khronos validation-layer run passed both
synchronization stages without a VUID or synchronization hazard.

## Recorded environment

| Component | Value |
| --- | --- |
| Base image | `python:3.13-slim-trixie@sha256:9d2e5553305c7c7b0097999bb17187c69b921ccd6bc9d40e4bb5ebe652c00285` |
| Python | 3.13.15 |
| Distribution / libc | Debian GNU/Linux 13.6 trixie / glibc 2.41 |
| Kernel / architecture | WSL2 Linux 6.18.40.1 / x86_64 |
| Compilers | GCC 14.2.0; Clang 19.1.7; Clang uses trixie's libstdc++ |
| Build tools | CMake 3.31.6; Ninja 1.12.1 |
| Runtime | VapourSynth 81, `cp312-abi3` manylinux x86_64 wheel, running on Python 3.13 |
| NumPy | 2.5.4 |
| Vulkan | Mesa 25.0.7 llvmpipe, LLVM 19.1.7, Vulkan API 1.4.305 |
| Network | HTTP and HTTPS proxy `http://host.docker.internal:7890` |

The VapourSynth and Vulkan-Headers source locks are the same as the Windows
build in [build.md](build.md). The runtime wheel's `VSVulkan4.h` was compared
with the pinned SDK. These checks concern the matching runtime and SDK;
they do not imply that any unrelated R81-labelled header is interchangeable.

## Completed checks and scope

| Check | Evidence and result |
| --- | --- |
| GCC and Clang release builds | Both emit native ELF64 x86_64 `dfttestvk.so` files. CMake configure and build returned zero. |
| Exports and dynamic linkage | `VapourSynthPluginInit2` is exported. `ldd -r` resolves every dependency and reports no undefined symbols. |
| API4 initializer | A ctypes callback harness invokes the real exported initializer and checks the plugin ID, namespace, API4.3, CPU / GPU signatures and callable pointers. It does not create a Vulkan device. |
| Fixed and generic FFT arithmetic | Both compilers passed `verify_fft_codelets.py` and `verify_generic_fft.py` against NumPy. Fixed transforms cover T=1/3/5/7; the generic tool covers nine N/T configurations including N=1/3/8/12/15/16/32/63/64. The actual generated transform arithmetic runs on the CPU in these checks. |
| Actual R81 / SDK / device | `std.LoadPlugin` loaded the GCC ELF; the installed `VSVulkan4.h` equals the locked SDK. R81 reports usable llvmpipe API 1.4.305, device type `cpu`, unified memory. |
| GCC full oracle suite | All 98 cases passed every pixel, selected plane, frame properties and request-order checks through llvmpipe upload / compute / download. Includes Gray 8–16-bit / float32, YUV / RGB, T=1/3/5/7, five filter types, 12 windows, curves, short clips and overlap. Float maximum `7.748603820800781e-7`; integer maximum one storage code. |
| GCC smoke | Eight cases passed the same native path and all-pixel oracle checks. Stored metadata identifies the actual SO and SHA256. |
| GCC cache / request order | All 15 cases passed at seven frames, eight workers and one shuffled round: capacities 1/2/12, T=1/3/7, budget fallback, eviction and concurrent duplicates. Cache output matches direct processing within the recorded budgets; reverse / shuffled / duplicate requests match the same node's serial pixels and properties exactly. |
| Clang runtime smoke | The distinct Clang SO was loaded by R81 and all eight smoke cases passed the oracle through llvmpipe. |
| Native A100 hardware GPU | GCC build on glibc 2.35; 12 serial stages including validation-layer synchronization stages passed on the selected A100. |
| Windows/Linux fixture comparison | Ten canonical Gray16/GrayS cases, 50 outputs and 256,000 samples were bit-exact between the Windows RTX build and this A100 build. This bounded fixture does not guarantee universal cross-platform bit identity. |

Build commands, return codes, hashes, symbol-version requirements and the
initializer records are in
[`results/linux-trixie/final/verification.json`](../results/linux-trixie/final/verification.json),
with sibling logs. Actual runtime and driver provenance is in
[`runtime-info.json`](../results/linux-trixie/final/runtime-info.json). Native
output results are in
[`gcc-runtime-full.json`](../results/linux-trixie/final/gcc-runtime-full.json) and
[`gcc-runtime-smoke.json`](../results/linux-trixie/final/gcc-runtime-smoke.json).
Cache and Clang results are
[`gcc-runtime-cache.json`](../results/linux-trixie/final/gcc-runtime-cache.json) and
[`clang-runtime-smoke.json`](../results/linux-trixie/final/clang-runtime-smoke.json).
The earlier `results/linux-trixie/verification.json` is exploratory evidence;
the `final/` record contains both compilers and the current checks.

| Compiler | Binary bytes | SHA256 |
| --- | ---: | --- |
| GCC | 873712 | `b18db7e0a82cd9bf574acc4b5a569b2ec51bc0f8bf33c58dd37f42a7fb60c4ff` |
| Clang | 898392 | `c5beece669fee7d03d5a1e75b8e9c2dc948d9105436982e1b0a0ccf091a9aa15` |

These particular trixie builds require symbols through `GLIBC_2.38` and `GLIBCXX_3.4.31`.
The GCC ELF additionally requires `CXXABI_1.3.15`; use a compatible libstdc++
runtime, such as the tested GCC 14 library. These trixie binaries are not a
manylinux compatibility claim and should not be presented as portable to
older Linux distributions. Clang's use of libstdc++ is recorded explicitly.

GLIBC 2.38 is a requirement of these artifacts, not an intrinsic minimum of
DFTTestVK. The 2.38 reference is `__isoc23_strtol`, selected by the newer
system headers for the ordinary `std::strtol` calls that parse environment
variables. Builds for other Linux targets should use the target's older
glibc/sysroot and a suitable C++20 toolchain, then inspect symbol versions and
revalidate against the corresponding VapourSynth and driver runtimes.
Their compatibility floor must be measured from those artifacts rather than
copied from this trixie result; no lower-glibc package has been verified here.

## Reproduction

The maintained entry point builds the Docker image, mounts source read-only,
runs both compilers and preserves results in a fresh directory:

```powershell
python tools/fetch_dependencies.py --with-vapoursynth
& ./tools/linux/run.ps1 -Runtime
```

Proxy defaults to `http://host.docker.internal:7890`; override it with `-Proxy`.
`-Output` selects a new results directory. Omitting `-Runtime` runs just the
native compilation / ELF / initializer / CPU-FFT checks. With `-Runtime`, the
script also captures R81 / SDK / device metadata and runs GCC full 98, GCC
cache 15 and Clang smoke eight. The initial GCC smoke was run separately.

Run from the project root in PowerShell. Fetch the locked headers once, use a
fresh output directory, and mount source read-only. The Dockerfile has separate
`validation` and `runtime` targets, so compilation does not require a Vulkan
driver or a VapourSynth runtime. Build-time proxy values are supplied to Docker
as standard proxy arguments, rather than committed into the image.

```powershell
python tools/fetch_dependencies.py --with-vapoursynth
$dftProxy = 'http://host.docker.internal:7890'
$dftSource = (Get-Location).Path
$dftOutput = Join-Path $dftSource 'results/linux-reproduction'
New-Item -ItemType Directory -Path $dftOutput

docker build -f tools/linux/Dockerfile --target validation `
  --build-arg HTTP_PROXY=$dftProxy --build-arg HTTPS_PROXY=$dftProxy `
  -t dfttestvk-linux-validation:py313 .
docker run --rm --mount "type=bind,source=$dftSource,target=/src,readonly" `
  --mount "type=bind,source=$dftOutput,target=/out" `
  -e http_proxy=$dftProxy -e https_proxy=$dftProxy `
  dfttestvk-linux-validation:py313 python /src/tools/linux/verify.py `
  --source /src --output /out

docker build -f tools/linux/Dockerfile --target runtime `
  --build-arg HTTP_PROXY=$dftProxy --build-arg HTTPS_PROXY=$dftProxy `
  -t dfttestvk-linux-runtime:py313 .
docker run --rm --mount "type=bind,source=$dftSource,target=/src,readonly" `
  --mount "type=bind,source=$dftOutput,target=/out" -e LP_NUM_THREADS=4 `
  dfttestvk-linux-runtime:py313 python /src/tests/correctness.py `
  --backend vk --against oracle --dll /out/build-gcc/dfttestvk.so `
  --suite full --threads 4 --output /out/gcc-runtime-full.json

docker run --rm --mount "type=bind,source=$dftSource,target=/src,readonly" `
  --mount "type=bind,source=$dftOutput,target=/out" `
  dfttestvk-linux-runtime:py313 python /src/tools/linux/runtime_info.py
```

The recorded local images were `dfttestvk-linux-validation:py313`
(`sha256:e44591ed5fa0ea8b9315d95039808c67014d4575815c6a39b0947cadde269e8d`)
and `dfttestvk-linux-runtime:py313`
(`sha256:66c2fc6749dbd591362d1b5ccc913aad830300a4a7951c15a9e35d09af0e0178`).
The Dockerfile's base tag and apt / pip repositories can change; rebuilding
later requires storing the new image digest and package versions. The hashes
above identify this validation run, not a promise of byte-identical future
builds.

llvmpipe executes Vulkan compute on the CPU, while the native A100 run covers
one Linux hardware GPU and driver. The A100 Gray16/T=3 endpoint benchmark used
16 workers, eight prefetched requests and completed CPU outputs, producing
503.98, 506.30 and 508.62 fps (median 506.30); no Linux NVRTC baseline was
available on that host. These numbers do not establish behavior or throughput
for other vendors or devices. The Windows RTX benchmark remains separately
scoped in [benchmark.md](benchmark.md).
