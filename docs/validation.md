# Numerical validation and evidence status

Evidence below was inspected on 2026-10-12 with VapourSynth R81, Windows,
NumPy and an NVIDIA RTX 3090 Ti. The final candidate completed all numerical,
cache/request and diagnostic validation checks below. Performance repeats are
separate evidence. A pass applies to its recorded DLL and mapping, not
automatically to later shader or host changes.

## Results already verified

| Check | Scope and result | Stored evidence |
| --- | --- | --- |
| Independent FFT oracle | 98 cases; all pixels, frames and planes passed. Gray 8/9/10/12/14/16-bit and float32, YUV/RGB plane selection, constant/ramp/noise/impulse, temporal sizes 1/3/5/7, short clips, ftypes 0–4, f0beta including zero, 12 windows, frequency curves, overlap steps and request order. Float max `7.1525574e-7`; integer max 1 storage code. | `tests/results/final-r3-full.json` |
| Tiny planes and repeated reflection | 1x1, 2x3, 7x5 and 17x19; 4 integer cases passed exactly against the oracle. | `tests/results/final-r3-tiny.json` |
| Optional float16 storage | GrayH, YUV444PH and RGBH, four patterns each; 12 cases passed. Maximum one half ULP per sample; max absolute difference `0.00048828125`. Arithmetic remains FP32. | `tests/results/final-r3-half.json` |
| Independent filter instances | Six nodes with distinct inputs, dimensions, planes, sigma, f0beta and temporal sizes. Each of cache cap 1, cap 2 and GPU-resident entry point passed 486 interleaved concurrent requests against serial pixels/properties exactly; 24 workers. | `tests/results/final-r3-stress-cap1.json`, `final-r3-stress-cap2.json`, `final-r3-stress-resident.json` |
| General spatial transforms | 67 cases: B=1/3/8/12/15/32/63/64, T=1/3/5/7, Gray16/GrayS plus asymmetric odd-size spectrum checks. Float max `4.1723251e-7`; integer max 1 storage code. | `tests/results/final-r3-generic.json` |
| Invalid parameters and entry points | 91 malformed parameter cases correctly rejected, including cache controls. Five interface/None-plane checks passed: CPU/GPU output residency and binding auto transfers match their signatures. | `tests/results/final-r3-invalid.json` |
| Cache behavior and direct path | 15 cases covering caps 1/2/12, T=1/3/7, eviction, concurrent duplicates, 1 MiB budgets and tiny/generic auto fallbacks matched cache=0 exactly. A separate 32-case direct-path oracle matrix passed with cache=0. | `tests/results/final-r3-cache.json`, `final-r3-cacheoff-oracle.json` |
| Scalar/temporal frequency curves | 24 B=1,T=1/3 cases passed for separable/radial systems, ftypes 0/2 and common/temporal/inactive spatial curves. Float max `2.3841858e-7`. | `tests/results/final-r3-degenerate.json` |
| Real 1080p video | Every pixel in 24 consecutive finite-clip frames passed against DFTTest2 CPU and NVRTC, including endpoints. Gray16/YUV420P16 max one storage code; GrayS max `2.9802322e-7` vs CPU and `2.3841858e-7` vs NVRTC. | `tests/results/final-r3-real-gray16.json`, `final-r3-real-yuv420p16.json`, `final-r3-real-grays.json` |
| Vulkan API/synchronization validation | Isolated Khronos layer inserted into instance/device chains. Cache matrix with 24 workers, cap-1 independent instances and general N=63,T=7/N=3 asymmetric paths passed; no VUID or synchronization hazard. Synchronization and thread-safety validation enabled. | `tests/results/final-r3-validation-cache.json`, `final-r3-validation-small.json`, `final-r3-validation-generic.json` and their frame records/logs |
| Oracle reference checks | Installed DFTTest2 CPU passed 72 rectangular-temporal cases; original CPU passed all 12 window cases. Additional 9/12/14-bit cases, zero f0beta and None planes passed separately. | `tests/results/oracle-cpu-full-twin7.json`, `oracle-original-windows.json`, extra-depth/None-plane records |
| B=1 temporal frequency curves | Original CPU B=1,T=3 passed 12 curve cases against the corrected dimensional oracle; float max `2.3841858e-7`. Six B=T=1 DC cases passed direct analytical expectations. | `tests/results/original-b1-t3-curves.json`, `oracle-b1-t1-analytical.json` |

All final-r3 native results loaded DLL SHA256
`f51789ed92aad8aa793e55d7d8f6cf5a65327305de7ff875675c2be558e66c26`.
Each native result contains the actual loaded plugin path/hash and signatures;
diagnostic environment mappings are recorded and unset for these final runs.
The layer wrappers record layer DLL hashes; their frame records supply native
provenance. `tests/results/final-r3-native-provenance-audit.json` confirmed all
17 native records passed and matched the current DLL hash. Earlier development
results remain historical evidence and were not substituted for this audit.

The reported integer maximum is a storage-code difference, not a uniform
8-bit difference: one 10-bit code is 0.25 in the filter's 8-bit amplitude scale,
and one 16-bit code is 1/256. Every comparison records plane RMS, maximum,
P99, differing fraction, per-frame metrics and maximum in 8-bit amplitude
units. Float budgets are not integer quantization budgets. Float16 uses an
explicit per-sample ULP budget after quantizing oracle input and output.

## Reference limitations and defined edge behavior

The implementation is not bit-exact against DFTTest2 CPU or NVRTC. In the
24-frame real 1080p Gray16 test, 0.001460825% of samples differed from NVRTC
and 0.092242959% differed from CPU; every nonzero difference was one 16-bit
storage code. Across all YUV420P16 planes, weighted by their sample counts,
the corresponding fractions were 0.001370403% and 0.084206747%. Four of the
24 frames matched NVRTC in every integer sample; the other frames did not.

GrayS also was not bit-exact: its maximum absolute difference was
2.3841858e-7 against NVRTC and 2.9802322e-7 against CPU. A small absolute
error does not imply equality of all floating-point samples. FP16 tests
allowed one half ULP. Passing these budgets is distinct from bit identity.

The 15 recorded cache/direct comparisons produced equal numerical values,
and repeated/concurrent request checks matched their own serial outputs.
These are scoped consistency results, not a promise for every window or
backend. Windows and Linux were independently checked against the oracle;
their output bits have not been compared directly across platforms.

The oracle independently implements NumPy float64 RFFT/IRFFT, windows,
frequency curves and scalar overlap-add; it imports no project shader or
DFTTest2 implementation. Unnormalized inverse scaling is explicit. Native
FP32 transform/accumulation order differs, so the default exploratory limits
are float max `2e-5`, integer max one storage code and half max one ULP.
Threshold-sensitive hard/range-filter deviations require inspection rather
than a claim of bit identity.

DFTTest2 CPU incorrectly selects its last temporal synthesis-window slice
for nonrectangular temporal windows. Those tests use the independent oracle
and original CPU; the CPU baseline is compared only with rectangular twin=7.
Original CPU rejects temporal sizes longer than the clip, and upstream padding
can read uninitialized values when one reflection is insufficient. Short
clips and tiny planes therefore use clamped temporal mathematics and periodic
reflection in the oracle. See [algorithm.md](algorithm.md).

Frequency curves count only transform axes whose length exceeds one. B=1,
T>1 uses a one-dimensional temporal curve without square/cube rooting. For
B=T=1, both sigma systems evaluate the common/temporal curve at DC; inactive
spatial curves do not influence it. This finite definition deliberately
changes the original CPU's zero-dimensional curve behavior, including radial
NaNs. The final host/shader implementation passed the degenerate suite.

R81 Python function bindings insert transfers when the input residency differs
from the native signature. Tests verify the resulting residency rather than
expect those calls to fail. `DFTTest` returns CPU frames; `DFTTestGPU` returns
GPU frames. Throughput uses completed CPU outputs and includes required
upload/download. It does not count submission as completion.

## Completed performance evidence

The full, tiny, half, degenerate, generic, invalid, cache, independent-instance,
real-video and isolated validation suites have completed for the hash above.
Any subsequent native change requires evidence for the resulting DLL hash.

The formal comparison has completed under `results/final-main/` and
`results/final-gpu/`: 48 runs, 16 configurations, three fresh processes per
configuration, serial execution with rotated order, common source/effective
parameters, recorded concurrency, completed CPU outputs and separate warmup.
Every filter timing lasted at least `3.8758331` seconds; the source-only
baseline is explicitly exempt. Raw JSON, NPZ, logs and telemetry are retained.

The main workload is 1920x1080 Gray16, T=3, compared against original DFTTest
CPU, DFTTest2 CPU, NVRTC and cuFFT, plus the source control. The extended GPU
workloads compare Vulkan with NVRTC on Gray16 T=1/7 and YUV420P16 T=1/3/7.
The main matrix times 2048 outputs per run; the extended GPU matrix times
4096. Both use 16 workers, eight prefetched requests and 16 warmup frames.
All runs use the same final Vulkan DLL hash as the numerical validation.
Twenty-one same-repeat sample comparison pairs passed, with maximum difference
one native integer storage code. This does not extend the full-frame evidence
to unmeasured workloads or devices.

`results/final-analysis.json` reports `complete_and_passed=true`; its SHA256 is
`36ac34f48c9825b9338e28f27bbaa263198e93efcb29b5099d51638e8fb8ccf9`.
It checks source/native/factory/helper hashes, three-repeat completeness,
effective configurations/resources, minimum timing, stored sample alignment
and sampled telemetry. See [benchmark.md](benchmark.md) for measured medians,
ranges, operating-clock observations and the scope of the speed comparisons.
Earlier `explore-*`/`tune-*` results remain development evidence, separate
from the completed formal matrices.

## Reproduction

From the project root, select the Python containing the tested VapourSynth
runtime and use new output filenames:

```powershell
python tests/correctness.py --backend vk --against oracle --output tests/results/final-full.json
python tests/correctness.py --backend vk --against oracle --suite tiny --output tests/results/final-tiny.json
python tests/correctness.py --backend vk --against oracle --suite half --output tests/results/final-half.json
python tests/correctness.py --backend vk --against oracle --suite degenerate --output tests/results/final-degenerate.json
python tools/probe_generic.py --output tests/results/final-generic.json
python tests/invalid.py --backend vk --output tests/results/final-invalid.json
python tests/stress.py --backend vk --output tests/results/final-stress.json
python tests/compare_real.py --backend vk --against cpu,nvrtc --format gray16 --output tests/results/final-real-gray16.json
python tests/run_validation.py --sync --thread-safety --output tests/results/final-validation.json -- tests/stress.py --backend vk --output tests/results/final-validation-frames.json
python tests/run_benchmarks.py --matrix tests/matrix-main-final.json --results results/new-main --frames 2048 --repeats 3 --threads 16 --prefetch 8 --telemetry
python tests/run_benchmarks.py --matrix tests/matrix-gpu-final.json --results results/new-gpu --frames 4096 --repeats 3 --threads 16 --prefetch 8 --telemetry
```

The validation wrapper preloads isolated GCC/SPIRV/layer DLLs before importing
VapourSynth to prevent dependency-name conflicts with autoloaded plugins. It
requires the exact Khronos insertion line, hashes the layer files and preserves
VUID/hazard lines. It changes only the child process environment. Validation
runs stay outside timing runs. A silent loader failure is not a successful
validation check.

`tools/audit_release.py` checks archive manifest hashes, the selected latest
DLL hash, explicit result passes and their loaded Vulkan DLL hashes. It is a
mechanical evidence check; a pass does not establish scope coverage, numerical
budget suitability, algorithm compatibility or completion of the whole task.
