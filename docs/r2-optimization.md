# Cached frequency-row scheduling in 0.1.1

The retained optimization consumes one cached spatial frequency row at a time
for `tbsize=3`: load three neighboring spectra for that row, perform the temporal
transform and filtering, and retain only the central inverse result for the
spatial inverse. The earlier schedule kept `T*N` complex coefficients across
separate temporal-transform and filtering loops. The new schedule shortens
private-array lifetimes; actual hardware register counts were not measured.

Mean removal still needs the transformed DC coefficient before every lane
filters. Lane zero computes that small transform first and publishes the mean
through the existing shared buffer and barrier. All lanes, including inactive
tail tiles, reach the barrier. This duplicates one small DC transform rather
than adding another shared spectrum buffer. FFT codelets, windows, sigma
semantics, arithmetic precision, overlap-add and cache ownership are unchanged.

This follows the storage/lifetime decisions emphasized by BM3DVK2 r2. DFTTestVK
already deduplicates pending spectrum creators; it does not need BM3D's output
accumulator ring or block-search selection network.

## Selection by measurement

The exploratory row schedule passed the full numerical and cache suites but
slightly slowed T=7 on the RTX 3090 Ti: Gray16 median 328.35 → 323.24 fps,
all YUV420P16 217.10 → 214.07 fps. The final implementation selects it only
at T=3. T=1/5/7 and `cache=0` retain the previous schedule. This is an explicit
choice for the measured workload, not a claim about universal GPU occupancy.

## Final matched throughput

Windows 10, RTX 3090 Ti, NVIDIA 617.42, VapourSynth R81; ordinary desktop active,
with other GPU applications not controlled. Three serial fresh-process runs
per configuration use rotated A/B order, 2,048 timed frames after 16 warmups,
16 threads and prefetch eight. Both builds process the same 24 preloaded
consecutive 1080p real-video frames, repeated periodically with new output
frame numbers, `sbsize=16,sosize=12,tbsize=3,twin=7` and default filtering.

| Complete CPU output | Previous schedule, median (range) fps | New schedule, median (range) fps |
|---|---:|---:|
| Gray16 | 522.63 (522.25–525.50) | 556.36 (556.05–557.08) |
| All YUV420P16 planes | 306.30 (304.71–306.68) | 346.42 (345.13–347.43) |

The measured increases are 6.5% and 13.1%. Required uploads, source device
copies, preprocessing, cache accesses, finishing, gather and downloads are
included. Decode/preload and compilation are excluded. These are full-frame
throughput measurements, not finishing-dispatch timestamps. They establish
no throughput result for other devices, drivers, access orders or formats.

The baseline is the same-toolchain rebuild from `f476044`; records identify
measured DLL hashes and source input hashes. Hosted binaries have their own
package manifests. Numerical budgets remain the existing one native integer
code, FP32 absolute 2e-5 and FP16 one ULP; none was loosened for this change.
The paired real-video samples differ by at most one integer storage code.

Raw measurements, sample comparisons and final-source Windows/Linux checks
are retained under [validation/v0.1.1](validation/v0.1.1). Pixel NPZ samples
and the external source video remain local; the records contain hashes.
Use [the numerical tools](../tests/README.md) to repeat the independent oracle,
cache eviction, concurrent requests, FP16 and validation-layer checks.

Final Windows checks passed 98 full oracle cases, 12 FP16 cases, 15 cache cases,
concurrent cap-one stress under Khronos synchronization/thread-safety validation,
and five real-video positions in both Gray16 and YUV420P16 against NVRTC.
Linux GCC and Clang builds/registration/FFT checks passed; Mesa llvmpipe passed
98 full cases, 15 cache cases and eight Clang-build smoke cases. Linux records
establish software-Vulkan behavior; performance above is Windows NVIDIA only.
