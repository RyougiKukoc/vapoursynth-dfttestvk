# DFTTest Vulkan algorithm contract

Audit date: 2026-10-12. This document describes the requested DFTTest semantics and identifies reference implementation defects that must not become Vulkan behavior.

## Locked references

- Original CPU algorithm: [HomeOfVapourSynthEvolution/VapourSynth-DFTTest at bc5e0186a7f309556f20a8e9502f2238e39179b8](https://github.com/HomeOfVapourSynthEvolution/VapourSynth-DFTTest/tree/bc5e0186a7f309556f20a8e9502f2238e39179b8). This was the repository HEAD during this audit. `DFTTest.cpp` remains API3 upstream; the local `fix-yvsfunc-ee2x/plugins/vapoursynth-dfttest` copy supplies an API4 port with the same algorithm.
- Optimized algorithm: [AliceTeaParty/vapoursynth-api4-wheels at d30ef2febe1b518ff8f2eda001d1d8506dababb6, plugins/vapoursynth-dfttest2](https://github.com/AliceTeaParty/vapoursynth-api4-wheels/tree/d30ef2febe1b518ff8f2eda001d1d8506dababb6/plugins/vapoursynth-dfttest2). HEAD confirmed with `git ls-remote`; the Python helper was read directly from this locked URL.
- CPU installed for verification: VapourSynth R81; `dfttest2` Python version 0.4.0; `core.dfttest2_cpu.Version()` reports `vapoursynth-dfttest2-cpu-v10.2-0-g4574285`, targets `auto,sse2,avx2,avx512f`. Native original CPU plugin: `dfttest.dll`; installed reference hashes are recorded in the source lock.
- Available CUDA release: `vapoursynth-dfttest2-cu129-v10.2+alice.1`, published 2026-10-07; Windows zip includes CPU, NVRTC and cuFFT plugins. It is staged separately under `reference/backend-artifacts`, without altering global installation.

## Public parameter scope

Use original DFTTest names for an ordinary API4 CPU-frame plugin. Defaults are `ftype=0, sigma=8, sigma2=8, pmin=0, pmax=500, sbsize=16, smode=1, sosize=12, tbsize=3, tmode=0, tosize=0, swin=0, twin=7, sbeta=2.5, tbeta=2.5, zmean=True, f0beta=1, ssystem=0, planes=all`.

GPU reuse controls are `cache=-1` (automatic for B=16,T>1), `cache=0` (direct processing), or `cache=1` (required reuse with B=16), plus `cache_frames=12` (valid 1–128) and `cache_mb=0` (automatic creation-time budget of one quarter of the core VRAM limit). Positive `cache_mb` supplies an explicit MiB budget instead. The effective persistent LRU count is reduced to the number of spectra fitting that budget. Pending work and active GPU pins can outlive LRU eviction and consume additional memory. Cache misses, eviction and different request orders do not change the intended filter result. See [temporal-cache-design.md](temporal-cache-design.md).

The fast DFTTest2 CPU/NVRTC/HIPRTC implementations require `sbsize=16` and `tbsize in {1,3,5,7}`. Their cuFFT/hipFFT paths accept other transform sizes. Vulkan's specialized path uses 16x16 blocks and those four temporal sizes, all five public filter types, both mean choices, all window types, 8–16-bit integer and 16/32-bit float storage, arbitrary valid planes and valid overlap amounts. Other block sizes from 1 to 64 use generic transforms, subject to device resource limits. Unsupported temporal lengths are rejected rather than silently changing parameters. Float16 storage is an extension beyond the CPU references and still uses FP32 transform arithmetic.

Original CPU additionally supports odd `sbsize` with `smode=0` (one output pixel per block), odd `tbsize` up to 15, and `nlocation` noise-spectrum estimation. DFTTest2 explicitly rejects `smode!=1` and any non-None `nlocation`; it omits `tmode/tosize` from its Python function because only `tmode=0,tosize=0` exists. These features must be documented separately from the fast implementation's compatibility claims.

Vulkan validates parameters more strictly than the DFTTest2 Python helper: `tbsize in {1,3,5,7}`; `0 <= sosize < sbsize`; `step=sbsize-sosize`; overlap over 50% requires `sbsize % step == 0`; windows 0–11; `ssystem` 0 or 1; finite, nonnegative power/range parameters; nonnegative `f0beta`; distinct plane indices in the clip format. Constants must fit finite binary32 storage. DFTTest2 currently floors `(tbsize-1)//2`, so even `tbsize` can silently become a smaller odd transform; Vulkan rejects them. The original CPU rejects `tbsize > clip.num_frames`; Vulkan and DFTTest2 support short clips by clamping neighboring frame indices.

## Spatial and temporal boundaries

For plane dimension `L`, block size `B`, step `D`:

```
P(L) = ceil(L/B)*B + 2*max(B-D,D)
offset(L) = floor((P(L)-L)/2)
num_blocks(L) = floor((P(L)-B)/D)+1
block origins = 0,D,2D,...,(num_blocks-1)*D in padded coordinates
```

Centered padding uses reflection without repeating endpoints: let `q=padded_coordinate-offset`; map `q<0` to `-q`, and `q>=L` to `2*L-2-q`. This is NumPy's `reflect`, not `symmetric` or edge clamping. The reference copies a single reflected region. If the padding exceeds the dimension, it can read uninitialized data; Vulkan extends this mathematically with a period `2*(L-1)` and special-cases `L=1` to 0. Standard dimensions retain exactly the original coordinates.

For output frame `n` and temporal radius `r=(T-1)/2`, slice `j=0..T-1` reads frame `clamp(n+j-r,0,num_frames-1)`. Request only distinct in-range frames, then reuse the first/last frame for out-of-range slices. Temporal boundaries repeat endpoints; they do not reflect. Produce only inverse-transform slice `r`. There is spatial overlap-add but no temporal overlap-add. Independent requests, jumps, repeats and reverse order must give the same result.

## Sample scale, window and normalization

Transform samples use an 8-bit amplitude scale:

```
x_scaled = x * 2^(8-bits)       # integer clips
x_scaled = x * 255             # float clips, including signed chroma
```

Convert back with the reciprocal. Integer output rounds by `floor(clamp(x/scale+0.5,0,peak))`, where `peak=2^bits-1`. Float output is divided by 255 without clipping. Pixel storage remains uint16 for 9–16-bit clips; do not conflate nominal depth with storage width.

All windows are evaluated at half-sample locations `i+0.5`. Spatial base window `S0` is normalized for overlap-add:

```
S[q] = S0[q] / sqrt(sum(S0[h]^2 for h in [0,B) if h% D == q% D))
W[t,y,x] = T0[t] * S[y] * S[x] / sqrt(T*B*B)
```

The temporal base window is not overlap-normalized because temporal mode is center-slice output. Use `W` for analysis and `W[r,:,:]` for synthesis. Forward and inverse FFTs are both unnormalized, so the two window factors already supply the required `1/(T*B*B)` overall scale; adding an FFT inverse normalization again is wrong. For identity filtering, overlap-add gives `T0[r]^2 * input`. Most window center values are 1; flat-top mode 6 has a different center value and deliberately changes the identity gain.

Window modes match the helper/source: 0 Hann, 1 Hamming, 2 Blackman, 3 four-term Blackman-Harris, 4 Kaiser-Bessel, 5 seven-term Blackman-Harris, 6 flat top, 7 rectangular, 8 Bartlett, 9 Bartlett-Hann, 10 Nuttall, 11 Blackman-Nuttall. Preserve mode 9's existing signed linear term `0.62 - 0.48*(location/size - 0.5) - 0.38*cos(2*pi*location/size)`; the source lacks an absolute value. Correcting this textbook discrepancy would alter compatibility.

Let `E=sum(W^2)` over the entire temporal/spatial block. For `ftype<2`, multiply scalar or array `sigma` and `sigma2` by `E`; for `ftype>=2`, sigma values are raw amplitude multipliers. Multiply `pmin,pmax` by `E` for their PSD uses. The exponent `f0beta` is dimensionless and is never scaled. Sigma here is a noise power threshold parameter, not an amplitude standard deviation, so do not square it.

CPU original accumulates window energy in float; DFTTest2 Python uses double `math.fsum`, then casts constants to float at the native boundary. Transform and accumulation order differ slightly. Compatibility should use a stated numerical error budget rather than bit identity.

## FFT layout and filtering

Compute a real forward transform over `(T,B,B)` (or `(B,B)` when T=1). Stored coefficients are shaped `(T,B,B/2+1)` with x fastest. Both FFTW and cuFFT forward signs are negative. Use the full conjugate symmetry of the multidimensional transform if reconstructing omitted x frequencies: `F[t,y,B-x]=conj(F[(-t)%T,(-y)%B,x])`, not merely a row-local x mirror. NVRTC implements fixed-size real/complex transforms in registers, uses shared memory for transposes, and inverts only temporal center slice before finishing the inverse spatial transform.

For `zmean=True`, precompute `G=DFT(255*W)` in the exact same frequency layout. For each block:

```
g = real(F[0,0,0]) / real(G[0,0,0])
R[k] = F[k] - g*G[k]
F_filtered[k] = filter(R[k]) + g*G[k]
```

This subtracts the entire windowed mean response, including non-DC coefficients. Zeroing only DC changes the algorithm. The same operation can be performed in samples before the forward transform if its weighted mean and rounding are validated.

Let `P = re^2+im^2`, `eps=1e-15`, and multiply both components by the multiplier below. For hard threshold, explicitly set both to zero when the strict condition holds.

| ftype | Multiplier |
| --- | --- |
| 0 | `max((P-sigma)/(P+eps),0)^f0beta` |
| 1 | `0 if P<sigma else 1` |
| 2 | `sigma` |
| 3 | `sigma if pmin<=P<=pmax else sigma2` |
| 4 | `sigma*sqrt(P*pmax/((P+pmin)*(P+pmax)+eps))` |

The helper maps public ftype 0 to internal types 0 (beta near 1), 6 (beta near 0.5), or 5 (general power). The tolerances are `abs(beta-1)<0.00005` and `abs(beta-0.5)<0.0005`; internal type 5 stores its unscaled exponent in `pmin`. A Vulkan specialization may preserve these shortcuts and exact comparisons.

For spatial output pixel `(px,py)` in padded coordinates, sum transformed center-slice block samples times `W[r,offset_y,offset_x]` over every block covering that pixel. Gather per pixel avoids floating point atomics and fixes sum order. A first fused path should store only the center-slice `B*B` real block, not T slices or all complex spectra.

## Frequency-dependent sigma

Frequency coordinate for position `k` and length `L` is `0` for `L=1`; otherwise `k/floor(L/2)` up to the midpoint and `(L-k)/floor(L/2)` thereafter. Positive x storage ends at `B/2`, while y/t wrap through the full transform dimensions.

Frequency curves use `(position,value)` pairs, sorted on [0,1], with both endpoints present. Interpolate the rooted knot values, not the root of interpolated raw power. The number of active frequency dimensions is `ndim=(B>1 ? 2 : 0)+(T>1 ? 1 : 0)`. Root exponent is `1/ndim` for nondegenerate transforms, so B=1,T>1 is one-dimensional and uses raw knot values. `slocation` supplies the same curve on every active axis; per-axis `ssx,ssy,sst` supply separate curves with omitted axes using constant sigma. For `ssystem=0`, multiply interpolated values on active axes only. For `ssystem=1`, compute `sqrt((loc_t^2+loc_y^2+loc_x^2)/ndim)` and evaluate the temporal/common curve there. In the original source the root preprocessing is bypassed only when both `slocation` and `ssystem=1` are set; preserve that detail for separately supplied curves.

A 1x1x1 transform has a single DC coefficient and no active frequency dimension. Vulkan defines its profile at frequency zero with an effective normalization dimension of one: use `slocation(0)` when supplied, otherwise `sst(0)`, otherwise scalar sigma. Both `ssystem` values follow this rule. `ssx/ssy` refer to inactive axes and do not change that coefficient. This is a deliberate definition for a degenerate case, not a claim of original CPU equivalence: original `getSVal` returns one on every inactive axis, so `ssystem=0` with any curve uses multiplier/threshold one; original radial `ssystem=1` divides zero by zero and produces NaN. A native original-CPU check using constant GRAYS 0.5, `ftype=2,sigma=0.8,sbsize=1,sosize=0,tbsize=1,zmean=False` returned 0.4000000358 without a curve, 0.5 with `slocation=[0,2,1,4],ssystem=0`, and NaN with the same curve and `ssystem=1`. Vulkan's defined DC value for that curve is 2, giving output 1.0 in either system.

## Reference defects and verification evidence

1. DFTTest2 CPU and legacy GCC `getframe_impl.cpp` use synthesis window offset `radius*block_size*2*16`, rather than `radius*block_size*16`. With B=16, this selects temporal slice `2*radius` (the last) instead of the center. Default rectangular `twin=7` hides the defect. CPU runtime confirmation on a 64x48 GRAYS constant 0.5, T=3: original CPU gives 0.5; DFTTest2 CPU gives 0.125 for twin=0 and 0.0278225 for twin=3. The result also fails ftype=2,sigma=1,zmean=False, so it is synthesis indexing rather than denoising. NVRTC/cuFFT `col2im` use the correct radius slice. Vulkan must use the correct center and use original CPU or GPU DFTTest2 for nonrectangular temporal-window validation.
2. Current Alice helper fixes two older defects: nondefault f0beta previously received an energy-scaled exponent; CPU zmean=False previously failed to reach the native code. Locked current helper passes f0beta unchanged and forwards zero_mean.
3. Original CPU rejects temporal sizes beyond the clip duration; DFTTest2 does not. Validate short clips against clamped-frame math or DFTTest2, not an original call that refuses them.
4. Original and optimized source only reflect once in padding. Tests on dimensions smaller than the reflection width must use a defined independent reference rather than claim equality to uninitialized reference memory.
5. Original B=1,T=1 frequency profiles have a degenerate dimensionality; use the documented single-DC rule above rather than copying the original radial NaN or its nonradial implicit threshold of one.

An inline CPU comparison used 64x48, seven GRAYS frames with pattern `0.15+((11*x+7*y+19*n+x*y%41)%220)/300`, requested first/center/last frames, and one VS worker thread. Original CPU vs installed DFTTest2 CPU for defaults, T=1/5/7, zmean=False, f0beta=0.7/2, ftype=1/2/4 and slocation with ssystem=0/1 yielded maximum differences at most `2.3841858e-7` and RMS at most `4.85e-8`. Ftype=3 with sigma=0.7,sigma2=0.9,pmin=50,pmax=10000 yielded max `5.9425831e-5`, RMS `2.0980696e-6`, consistent with discontinuous threshold decisions near float precision boundaries. No GPU timing was run during this audit.

Use original CPU and NVRTC as distinct correctness references; use DFTTest2 CPU only with rectangular temporal windows. Include default denoising, identity, zero multiplier with/without mean, all temporal lengths, first/last frames, odd dimensions, subsampling, integer rounding, nondefault windows, curve sigma, and random request order. Performance comparisons must include the fast NVRTC backend, not only the CPU or cuFFT backend.
