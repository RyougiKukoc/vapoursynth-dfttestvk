# Optimization history and boundaries

These notes explain implementation decisions, not a final speed claim.
Exploratory records under project-root `results/` belong to their stored DLL
hashes, configurations and durations. Final-DLL correctness and repeated
matched throughput results are separate evidence.

## Retained design

The starting point was DFTTest2's FFTW-derived fixed transform codelets and
fused CUDA pipeline. Vulkan compute keeps the same mathematical transforms,
mean response and filter parameters, with direct reflected frame loads and
gather-based overlap-add. Removing unnecessary inverse temporal output slices
keeps only the center needed by DFTTest.

Static generation expands the small transform loops so private-array indices
are literal before GLSL compilation. Workgroups use 16 tiles when their thread
and shared-memory limits permit it, with 8/4/2-tile alternatives. Reflection
uses its expensive periodic mapping only outside the source plane. Constants
are uploaded to device-local buffers once.

The larger algorithmic change reuses spatial FFT results between temporal
outputs. A manual cache deduplicates pending creators with shared futures,
keeps ready entries by LRU and retains evicted entries until every GPU user
finishes. Ordinary device-local buffers store half spectra. On discrete
devices, preprocessing first copies source pixels to a device-local buffer to
reduce repeated access cost. Public cache controls and creation-time budgets
make this strategy reviewable and allow `cache=0` comparisons.

## Removed frame-backed cache

An intermediate experiment used a tall GRAYS frame to represent all selected
planes' block spectra and an internal `DFTTestSpatialFFT` node for reuse. Its
height was legal because core GPU planes are buffers, but measurements showed
poor throughput. Changing its frame representation to ordinary GPU buffers,
deduplicating work explicitly and testing source-copy placement led to the
current implementation. The older experiment does not describe current data
flow and is retained only as diagnostic history.

The core node-cache API also distinguished a fixed frame count from an
adaptive tuning target: fixed caches were excluded from ordinary pressure
eviction; adaptive caches could grow beyond the initially selected count.
The current manual LRU therefore does not use that API to promise a bounded
persistent entry count. Its pending and in-flight references still count as
additional active memory, as documented in the cache design.

## Diagnostics kept separate from defaults

Workgroup/subgroup mapping, loop hints, explicit FFT FMA, alternate shared
padding, embedded constant tables and per-row literal tables were tested as
separate candidates. They should not be presumed faster on another device.
The default selection follows the retained implementation and its resource
checks; diagnostic environment variables remain experiments.

SPIR-V optimization and a FloatControls2 division experiment preserve source
modules and hashes. `patch_div_fastmath.py` decorates only division results
with `AllowRecip|AllowContract` and introduces zero default flags without
relaxing NaN, infinity or signed-zero assumptions. Validation proves module
legality, not accuracy or throughput. That experiment did not provide a
useful observed gain and is not a production default.

## Measurement boundary

Finishing fused/gather timestamps exclude cached preprocessing. They cannot
be compared with a direct fused timestamp as though both cover the complete
filter. Report preprocessing separately or time completed CPU outputs with
all required source copies, uploads, downloads and synchronization included.
T=1/T=3/T=5/T=7, plane counts, input formats, overlap amounts and random versus
sequential access create different work and reuse opportunities.

A final performance conclusion requires several fresh-process repetitions,
rotated backend order, the same source and filter parameters, explicit request
concurrency, sufficiently long timings and stored hashes/numerical samples.
Short exploratory timings and a single NVIDIA card do not establish a general
advantage over NVRTC or over other Vulkan devices.
