# Numerical and performance checks

Use `C:/green/WPy64-31470/python/python.exe` in this workspace. Scripts select
backends explicitly, record loaded native paths and SHA256, and refuse to
overwrite evidence. Vulkan loads `build/dfttestvk.dll` by default; `--dll`
selects another build. CPU-baseline checks can run during GPU development.

```
python tests/correctness.py --backend cpu --suite smoke --output tests/results/cpu-smoke.json
python tests/correctness.py --backend cpu --case "^(?!window)" --output tests/results/cpu-twin7.json
python tests/correctness.py --backend vk --against oracle --output tests/results/vk-oracle.json
python tests/correctness.py --backend vk --against cpu --case "^(?!window)" --output tests/results/vk-cpu.json
python tests/correctness.py --backend vk --against oracle --suite tiny --output tests/results/vk-tiny.json
python tests/correctness.py --backend vk --against oracle --suite degenerate --output tests/results/vk-b1-curves.json
```

The full matrix compares all pixels of all frames and planes, reports plane
RMS/max/P99/different fraction and max difference in 8-bit amplitude units,
checks properties and unchanged planes, and repeats requests in serial random,
reverse, endpoint and concurrent orders with the output cache disabled. It
covers 8/9/10/12/14/16-bit integer and float gray, YUV and RGB, constant/ramp/noise/
impulse patterns, clips of 1/2/5 frames, temporal sizes 1/3/5/7, filter types
0–4, nondefault f0beta, all windows, frequency-dependent sigma and overlap
steps 1/2/4/8/10/16. `--case` is a case-name regex and `--list` shows matches.

`--suite half` optionally checks float16 Gray/YUV/RGB storage against the same
oracle rounded to float16. It reports RMS/max/P99 and per-sample maximum half
ULP distance, with the explicit `--half-ulps` budget (default one), so ordinary
half quantization is not incorrectly judged by the float32 absolute budget.
Source values themselves are quantized before entering the oracle.

```
python tests/correctness.py --backend vk --suite half --output tests/results/vk-half.json
python tests/invalid.py --backend vk --output tests/results/vk-invalid.json
python tests/stress.py --backend vk --instances 6 --rounds 6 --output tests/results/vk-stress.json
python tests/stress.py --backend vk --instances 6 --rounds 6 --threads 24 --cache 1 --cache-frames 1 --output tests/results/vk-stress-cap1.json
python tests/cache.py --threads 24 --output tests/results/vk-cache.json
```

Invalid cases exercise finite numeric parameters, valid overlap, frequency
profile knots/endpoints and unique valid planes. R81 Python bindings perform
automatic transfer when residency differs from a native function signature;
the test verifies the resulting output residency and records that transfer
rather than expecting those calls to fail. Omitted/None
planes means all planes; the Python binding rejects explicit empty arrays
before native filter creation. The invalid
matrix does not freeze the current supported transform size range. Stress
creates distinct clips/filter instances with different dimensions, planes,
sigma, f0beta and temporal sizes, then interleaves random concurrent requests
and compares each output/its properties with the instance's serial result.
`cache.py` compares cache frame caps 1/2/12 against cache=0, exercises eviction
and concurrent duplicates, includes T=1, generic auto fallback, tiny planes
and a spectrum larger than an explicit 1 MiB cache budget. Results contain
the actual loaded DLL hash and diagnostic environment. Public cache modes
are -1 auto, 0 off and 1 force; budgets/caps do not substitute for validation.

`oracle.py` uses independently implemented windows/curves, NumPy float64
RFFT/IRFFT and scalar overlap-add. Native FP32 FFT order can differ. Default
exploratory budgets are float max `2e-5` and integer max one storage code; use
`--float-atol` / `--int-atol` to state a different budget. These budgets are
not bit-exact claims, and a threshold-sensitive hard/range filter needs its
individual deviations examined.

DFTTest2 CPU has a known nonrectangular temporal synthesis-window defect;
use the oracle or original CPU for `window*` cases. Original CPU rejects
`tbsize > num_frames`, so use the oracle for short clips. Small dimensions
requiring repeated reflection have undefined reads in upstream padding and
are validated against the mathematical oracle. See `docs/algorithm.md`.

`--suite degenerate` covers B=1, T=1/3 frequency curves with both sigma
systems, active temporal curves and inactive spatial curves. Curve root and
radial dimensionality count only axes whose transform length exceeds one.
For scalar B=T=1, the defined finite behavior evaluates the common/temporal
curve at DC; this intentionally avoids the original CPU's zero-dimensional
division (radial NaNs) and its omission of the provided separable curve.
Original CPU B=1,T=3 provides independent compatibility evidence for 12 cases.

For complete CPU-output throughput, `bench_factory.py:make` works with the
holywu-vulkan skill harness and supports source/original/cpu/nvrtc/cuda/vk/
vk-resident. The resident path includes upload and download. Preload, format
conversion and graph creation are measured separately; timed frames use fresh
output numbers and complete before timing stops. The 1080p matrix uses luma
from the workspace's 144-frame YUV420P16 real sample. The first 24 consecutive
frames repeat, with an explicitly recorded temporal wrap every 24 frames.

```
python tests/run_benchmarks.py --matrix tests/matrix-1080p.json --results tests/results/1080p-r1 --frames 512 --repeats 3 --threads 16 --prefetch 8 --telemetry
```

This executes configurations serially, rotates their order each round, starts
a fresh process for each repeat, saves raw JSON/NPZ/logs/native hashes and GPU
telemetry, and summarizes median/min/max FPS. Run GPU benchmarks only when
no other GPU test is active. Choose enough frames that each timing spans several
seconds; sub-second smoke timings do not establish a performance advantage.
The source baseline must be substantially faster than the filter. Compare
stored samples with the skill's `compare_outputs.py`; full correctness remains
the job of `correctness.py`, not three benchmark sample frames.

```
python tests/compare_real.py --backend vk --against cpu,nvrtc --format gray16 --output tests/results/vk-real-gray16.json
python tests/compare_real.py --backend vk --against nvrtc --format grays --output tests/results/vk-real-float.json
```

`compare_real.py` compares every pixel in selected real 1080p frames, including
the first/last frames of a finite 24-frame clip. Native CUDA/NVRTC baselines load
from `reference/backend-artifacts/dfttest2-cu129-win64/dfttest2`, with dependency
search directories kept alive; this does not replace the installed CPU plugin.
The benchmark factory also records CUDA dependency hashes. CUDA baseline tests
and all Vulkan tests must share the same exclusive GPU test period.

```
python tools/analyze_benchmarks.py --results results/final-main results/final-gpu --output results/final-analysis.json
```

The analysis tool requires three completed repeats, filter durations of at
least three seconds (source-only baseline is exempt), matching current
Vulkan/source/native/factory/helper hashes and recorded diagnostic mappings.
It writes a Markdown median/range table and JSON with effective parameters,
resource settings, sample quality comparisons and sampled telemetry. CPU/NVRTC
NPZ comparisons require matching source, frame keys, dtype and workload and
use max one native integer code or explicit float budget. `--references auto`
checks the CPU/NVRTC baselines present in each directory; specify `nvrtc` for
an intentionally GPU-only comparison. `--allow-partial` creates an interim
report while preserving its incomplete status. Sampled device-wide VRAM is
neither plugin-only allocation nor a guarantee of the actual peak.

For diagnostic API/synchronization validation, `run_validation.py` starts a
fresh child with `VS_VULKAN_VALIDATION=1` and a local `VK_LAYER_PATH`; it does
not alter the registry or global Vulkan installation. The staged MSYS2
validation layer 1.4.304.1 and SPIRV-Tools packages are checked against the local
MSYS2 package database SHA256, with source metadata under
`dependencies/validation_layers/source.json`. The wrapper hashes the DLLs,
captures the loader/layer log, requires evidence that the layer was inserted,
and reports VUID/synchronization messages. These runs are outside performance
measurements and should be repeated after a final shader/host build.
The child bootstrap preloads the isolated GCC/SPIRV/layer DLLs before importing
VapourSynth, preventing autoloaded plugins from occupying dependency DLL names.
Layer insertion is matched by the exact Khronos name in the loader log.

```
python tests/run_validation.py --sync --thread-safety --output tests/results/validation-stress.json -- tests/stress.py --backend vk --instances 6 --rounds 6 --output tests/results/validation-stress-frames.json
```
