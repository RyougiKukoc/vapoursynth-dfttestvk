# Verified 1080p benchmarks

On the measured RTX 3090 Ti, DFTTestVK processed the default three-frame
1920x1080 Gray16 workload at a median 527.24 completed CPU frames/s, compared
with 503.00 frames/s for DFTTest2 NVRTC using eight streams. This is a 4.82%
throughput advantage for this workload under the machine's normal driver
clock policy. The additional completed matrix below shows larger temporal
reuse gains at T=7 and an NVRTC advantage at T=1. These results do not establish
other GPU vendors or clock-normalized kernel efficiency.

## Results

Each row contains three fresh-process runs. Configurations ran serially,
with their order rotated between repeats.

| Implementation | Median frames/s | Min–max frames/s |
| --- | ---: | ---: |
| Original DFTTest CPU | 29.58 | 28.60–30.44 |
| DFTTest2 CPU | 60.65 | 60.59–60.71 |
| DFTTest2 cuFFT | 102.95 | 102.68–103.79 |
| DFTTest2 NVRTC, 8 streams | 503.00 | 492.78–504.81 |
| DFTTestVK, production defaults | 527.24 | 522.03–528.40 |

The source-only median was 3947.97 frames/s, substantially faster than the
filters. The Vulkan runs each timed about 3.88–3.92 seconds, and the NVRTC
runs about 4.06–4.16 seconds. CPU and cuFFT rows ran longer for the same
frame count. These are complete graph throughput measurements; they include
required uploads, source-buffer copies, downloads and output completion.

## Completed temporal and plane-layout matrix

Each additional workload used 4096 timed outputs, three fresh processes,
16 warmup frames, 16 worker threads and eight prefetched requests. NVRTC
used eight streams. Source sample, preloading, 24-frame repetition, output
completion and normal unlocked driver policy matched the main protocol.
Gray16 processes luma; YUV420P16 processes all three planes, including
960x540 chroma planes. Filter settings matched the main defaults except T.
The Gray16 T=3 row comes from the 2048-frame main matrix above.

| 1080p format and T | Vulkan median (min–max) FPS | NVRTC median (min–max) FPS | Vulkan median relative to NVRTC |
| --- | ---: | ---: | ---: |
| Gray16, T=1 | 758.29 (752.50–765.15) | 967.34 (965.11–996.21) | −21.61% |
| Gray16, T=3 | 527.24 (522.03–528.40) | 503.00 (492.78–504.81) | +4.82% |
| Gray16, T=7 | 292.75 (291.90–325.61) | 226.74 (226.61–228.10) | +29.11% |
| YUV420P16, T=1 | 492.84 (484.71–495.76) | 563.76 (562.07–565.77) | −12.58% |
| YUV420P16, T=3 | 324.95 (324.09–326.20) | 314.69 (313.38–314.98) | +3.26% |
| YUV420P16, T=7 | 199.30 (193.76–201.09) | 144.52 (144.17–145.08) | +37.91% |

T=1 has no temporal spatial-FFT reuse under the automatic policy, and NVRTC
was faster in both measured layouts. T=7 showed the strongest reuse advantage.
The Vulkan Gray16 T=7 run range was wider than the other Vulkan rows, with
one faster repeat; the median and full range are both retained rather than
selecting that run as the result.

## Exact workload and protocol

The environment was Windows with VapourSynth R81, NVIDIA GeForce RTX 3090 Ti
and driver 617.42. Every main-matrix configuration used 16 VapourSynth worker threads,
eight prefetched output requests, 16 warmup frames and 2048 timed sequential
outputs. Timed output numbers were fresh, rather than repeated cached output
requests. DFTTest2 NVRTC selected eight streams explicitly; DFTTestVK used
its production defaults without diagnostic mapping, SPIR-V overrides,
validation layers or GPU timestamp profiling.

The source was a 144-frame raw 1920x1080 YUV420P16 sample. The factory
preloaded source frames 0–23, selected luma and repeated those 24 consecutive
frames to supply the requested length. Preload and format preparation were
outside the timed interval. Repetition introduces a disclosed temporal
discontinuity at every 24-frame wrap; decoding was not measured.

Source SHA256:
`9bd42cbecbd611156f6633757d83a2c83e53c18adc7b0a3a452588bd947e798c`.

The effective filter settings were:

```
ftype=0, sigma=8, sigma2=8, pmin=0, pmax=500
sbsize=16, smode=1, sosize=12
tbsize=3, tmode=0, tosize=0
swin=0, twin=7, sbeta=2.5, tbeta=2.5
zmean=True, f0beta=1, ssystem=0, planes=all
```

The Vulkan-only controls were `cache=-1`, `cache_frames=12`, `cache_mb=0`.
This enables spatial-spectrum reuse, with the creation-time cache budget
derived from one quarter of the core's VRAM limit. This discrete device also
uses the default source copy to a device-local buffer before preprocessing.

Final Vulkan DLL SHA256:
`f51789ed92aad8aa793e55d7d8f6cf5a65327305de7ff875675c2be558e66c26`.

The original CPU DLL hash was
`416a8eabb88655fc91a11806d90dc553b6e3843804f7fd838b16d5d2f4041b9f`.
DFTTest2 CPU used
`b5a35ea756119e2545f42d36e0d045e8b47edc16b9edafd0645d560d76f40b54`.
NVRTC and cuFFT came from the locked CUDA129 release
`vapoursynth-dfttest2-cu129-v10.2+alice.1`, with hashes and source commits in
`reference/source-lock.json`. Per-run records identify actual loaded paths,
hashes, runtime signatures and source metadata.

## Output checks and evidence

`results/final-main-analysis.json` inspected 18 run JSON records and reports
`complete_and_passed=true`, no collection errors and no per-group protocol
issues. It checks the selected Vulkan hash, common source hash, complete
repeat counts, matched effective filter parameters and request resources,
and a minimum three-second filter timing. The source-only control is
intentionally exempt from that filter-duration requirement.

Samples at output frames 16, 1040 and 2063 were saved as NPZ arrays in every
repeat. Six same-repeat comparison pairs—Vulkan against DFTTest2 CPU and
NVRTC—passed with a maximum difference of one 16-bit storage code, equivalent
to 1/256 in the filter's 8-bit amplitude scale. Against NVRTC, the differing
fraction was about 0.0017–0.0035% across those sample frames; P99 difference
was zero. These sample checks support this benchmark's numerical alignment;
they do not replace the separate full correctness and request-order matrix.
See [validation.md](validation.md) for the scopes of that evidence.

Raw JSON, NPZ, stderr logs and telemetry CSV files are under
`results/final-main/`; its `summary.json` records each repeat and the
median/min/max values. The selected matrix is `tests/matrix-main-final.json`.
The analysis is a completed snapshot and does not include unfinished or
unrelated GPU matrices.

The combined completed snapshot is `results/final-analysis.json`: all 48
run records and 21 same-repeat sample comparison pairs passed its protocol
and output checks. The additional NVRTC comparisons also stayed within one
16-bit storage code. Their records and telemetry are under
`results/final-gpu/`, selected by `tests/matrix-gpu-final.json`. The full
correctness and original-CPU reference checks remain separately documented
in `docs/validation.md`; sample agreement is not a bit-exact claim.

## Telemetry and limits

Telemetry sampled device-wide GPU memory, not memory owned solely by the
plugin. In the main Gray16 T=3 workload, the largest observed readings were
5650 MiB for Vulkan and 5264 MiB for NVRTC, including desktop/background use, core allocations and active
requests. They are sampled maxima rather than precise per-plugin peaks.
Persistent cache capacity is also distinct from pending/in-flight pins and
the complete graph's memory footprint.

High-utilization samples showed both backends around 2010 MHz SM clocks.
Vulkan's observed temperature range during these samples was 42–51°C, and
NVRTC's was 38–49°C. Clocks were not locked: observed memory clocks were
10501 MHz for Vulkan and 10251 MHz for NVRTC. The throughput comparison
therefore includes the driver's API-dependent operating policy; the 4.82%
result cannot all be assigned to algorithm changes independent of clocks.
The original telemetry CSVs preserve setup, warmup and timed samples.

Across the additional temporal/layout matrix, sampled device-wide maxima
ranged from 3018 to 6549 MiB for Vulkan and from 3147 to 9563 MiB for NVRTC.
The bounds belong to different workloads and are not a uniform memory saving.
High-utilization samples across those runs showed similar SM-clock ranges
and temperatures around 42–63°C, while the same Vulkan/CUDA memory-clock
difference persisted. No clocks were fixed for either matrix.

No result in this document establishes T=5, RGB all-plane throughput,
random-access speed, another resolution or another GPU.
Additional workloads need their own completed repeated runs and output
checks. In particular, temporal FFT reuse and its memory costs change with
T, overlap, plane count and request order.

## Reproduction

With the locked final build and staged references, use a fresh results
directory and the same raw sample:

```console
python tests/run_benchmarks.py --matrix tests/matrix-main-final.json --results results/main-reproduction --frames 2048 --warmup 16 --repeats 3 --threads 16 --prefetch 8 --telemetry
python tests/run_benchmarks.py --matrix tests/matrix-gpu-final.json --results results/gpu-reproduction --frames 4096 --warmup 16 --repeats 3 --threads 16 --prefetch 8 --telemetry
```

The matrix's raw-source path must be adjusted for another machine; verify
its SHA256 before treating the input as the same workload. Keep GPU tests and
validation outside timing runs, and preserve the raw artifacts rather than
only the summarized throughput table.
