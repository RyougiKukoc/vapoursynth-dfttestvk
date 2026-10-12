# Build and evidence protocol

## SDK lock

`tools/fetch_dependencies.py --with-vapoursynth` checks out clean, detached
commits without changing an existing differently pinned worktree:

| Dependency | Commit |
| --- | --- |
| Vulkan-Headers | `e3b1eec08173d6b825cd3ac88c885a63b621504a` |
| VapourSynth | `dd11a9da6f8e2bb24ab4bb084d44bf01fb93612a` |

The pinned `VSVulkan4.h` matches the R81 header installed in the development
environment. The GPU function table changes with the matching core headers;
build against that core's SDK rather than treating any R81-labelled header as
interchangeable. This plugin resolves Vulkan entrypoints from the core and
does not link the Vulkan loader or CUDA.

## Local build

Python 3.11 or newer, Git, CMake 3.20 or newer, a C++20 compiler and Ninja are
sufficient for a headers-only build. NumPy is additionally needed by the
CPU-only transform verification tool and correctness tests.

```console
python tools/fetch_dependencies.py --with-vapoursynth
python tools/build.py --compiler g++ --parallel 4
python tools/verify_fft_codelets.py --compiler g++
```

`build.py` accepts `--vs-include`, `--vulkan-headers`, `--cmake`, `--compiler`,
`--generator`, `--configuration` and `--build-dir`. With no explicit SDK,
it prefers the pinned headers under `dependencies/vapoursynth/include`; if
those do not exist CMake queries the selected Python's installed VapourSynth
headers. `--dry-run` prints the exact configure/build commands without running
them. Additional options use `--cmake-arg=-DNAME=VALUE`.

Ninja emits `build/dfttestvk.dll` on Windows and `build/dfttestvk.so` on Linux.
The tested Windows toolchain is MinGW-w64 GCC 16.2.0, CMake 4.4.2 and Ninja
1.13.2. The native source uses ordinary FP32 computations and does not enable
compiler fast-math. MinGW linkage disables the PE insertion timestamp with
`--no-insert-timestamp`; Windows runtime dependencies are statically linked.
Runtime GLSL compute compilation and pipeline creation are graph setup costs
and must be recorded separately from steady-state throughput.

The production defaults select static 16-point codelet indexing, a fast path
for coordinates inside the plane, device-local constants and a workgroup
tile count chosen from 16/8/4/2 against device limits. B=16,T>1 reuses spatial
spectra automatically; B=1–64 and T=1/3/5/7 retain direct/generic paths.
`cache=0` provides the direct comparison. Cache controls are public API
parameters and should be stored explicitly in benchmark configurations.

## CI coverage

`.github/workflows/build.yml` compiles Windows x86_64 with MSYS2 MINGW64 and
Linux x86_64 inside `python:3.13-slim-trixie`, with GCC and Clang. Both use locked API/Vulkan headers, run
the actual generated transform codelets against NumPy FFT, and attach a zip
artifact. They do not publish releases or install a plugin globally.

Hosted build runners do not validate Vulkan output or throughput. A GPU runner
using the matching R81 core should run the correctness matrix before any
performance benchmark, including tiny dimensions, nonrectangular temporal
windows, all planes and concurrent/random/reverse requests. The tests' output
reports include loaded plugin paths and hashes so an autoloaded reference
cannot satisfy a test accidentally. See `tests/README.md`.

Local Linux x86_64 GCC/Clang builds and a VapourSynth R81 software-Vulkan
runtime check were completed in `python:3.13-slim-trixie` with Docker and
proxy port 7890. See [linux-validation.md](linux-validation.md) for the native
ELF evidence, runtime scope, libc requirements and reproduction commands.

## Deterministic packaging

```console
python tools/package_release.py --binary build/dfttestvk.dll --platform windows
```

Version defaults to `project(... VERSION ...)` in CMake and may be selected
with `--version`. Platform and architecture form the archive name. A package
contains the plugin, its VapourSynth manifest, README, license/attributions,
algorithm/build/cache/validation notes, source lock and `manifest-sha256.json` recording every
payload file's size and SHA256. The manifest excludes its own hash. A sibling
`.zip.sha256` records the complete archive hash.

Zip entries are sorted, use fixed permissions and timestamps, and omit local
absolute paths. The timestamp defaults to 1980-01-01 UTC or uses
`SOURCE_DATE_EPOCH` when set. Equal binary/document bytes and packaging settings
produce equal archives. Compiler/linker determinism is a separate property;
the package tool does not claim that different toolchains produce identical
DLL/SO files. Existing output archives are not overwritten.

The final Windows DLL was rebuilt in a fresh `build-clean` directory using
newly fetched clean copies of the locked SDKs and the same GCC 16.2.0 toolchain.
Its SHA256 equals the development `build` DLL:
`f51789ed92aad8aa793e55d7d8f6cf5a65327305de7ff875675c2be558e66c26`.
This verifies those two clean/development builds despite different dependency
paths; it does not establish equality across compilers, platforms or versions.

## Performance evidence

Benchmark the completed graph with explicitly selected references: original
DFTTest CPU, DFTTest2 CPU, optimized DFTTest2 NVRTC, cuFFT and DFTTestVK. Use
the same source, format, plane list, transform sizes, sigma/window settings,
frame count and request concurrency. Time output completion; rotate backend
order and start a fresh process for each repeat. Preserve raw results, binary
hashes, numerical samples and optional GPU telemetry.

`DFTTESTVK_PROFILE=1` enables diagnostic per-plane fused/gather timestamp
reports on stderr. Leave it unset for throughput runs. GPU-resident and
CPU-output graphs have distinct transfer boundaries and need separate labels.
An optimization or speed advantage is established only by the stored results
for the workload/device actually measured.

Earlier exploratory choices and the removed frame-backed cache experiment
are described in [optimization-notes.md](optimization-notes.md). That history
does not substitute for final-DLL validation or repeated throughput evidence.
