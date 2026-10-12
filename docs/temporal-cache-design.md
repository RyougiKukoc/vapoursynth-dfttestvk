# Spatial FFT reuse

The current temporal 16x16 path stores source-frame spatial spectra in
explicit device-local GPU buffers and reuses them between output requests.
The direct fused path remains available with `cache=0`. Supported temporal
sizes are 1, 3, 5 and 7; automatic selection uses the cache only when T>1.

## Dataflow and numerical contract

A preprocessing dispatch transforms each source frame's blocks once. It uses
the original sample type, 8-bit amplitude scale, reflected coordinates and
central analysis window `W[T/2,:,:]` before the two spatial transforms. The
finishing dispatch reads the T neighboring cached half spectra, multiplies
slice t by `temporal_window[t]/temporal_window[T/2]`, performs the temporal
transform, mean removal/filtering/restoration and central inverse transforms.
The existing gather dispatch performs spatial overlap-add.

For default rectangular temporal windows, every ratio is one. For other
windows this moves a multiplication through the spatial FFT; it is
mathematically equivalent but introduces different FP32 rounding. Numerical
validation must cover nonrectangular windows and discontinuous thresholds.
The central temporal window must be nonzero.

Sequential T=3 processing changes the spatial-transform work from three
forward transforms and one inverse per output to one newly needed forward
transform and one inverse. It also adds a cached-spectrum write and T spectrum
reads. Random access can cause misses and recomputation, so a speedup cannot
be inferred from the transform count alone.

On discrete devices preprocessing first copies source pixels from their frame
plane into an ordinary device-local buffer. The copy completes through a
transfer-write to compute-read barrier. It is part of preprocessing, not an
extra CPU upload/download. Constants are also placed in device-local memory.

## Buffer layout

Each spatial block occupies one row of 16*9 complex FP32 values, interleaved
real/imaginary: 288 floats, 1152 bytes. The selected original planes occupy
contiguous row ranges in plane order. `cacheRow[p]` is the first row of plane p;
row count is its number of padded block origins. Unprocessed planes are omitted.

Preprocessing writes row `block_id` relative to each plane's row offset.
Finishing binds the corresponding range and indexes
`block_id*288 + 2*(frequency_y*9+frequency_x)`. Descriptor offsets must satisfy
`minStorageBufferOffsetAlignment`, and offset+range must fit the buffer. Each
preprocessing destination descriptor covers just the plane it writes, making
Y/U/V writes disjoint. Buffer byte sizes and shader indices are checked against
the device storage-buffer range and signed integer limits.

For 1920x1080 YUV420, B=16 and step=4, Y has 132825 rows and each chroma plane
has 33777 rows. An all-plane spectrum has 200379 rows and occupies 230836608
bytes, or 220.143 MiB. Twelve such entries occupy 2.580 GiB before source
frames, output frames, scratch, in-flight pins and allocator bucket overhead.
A luma-only spectrum occupies 145.926 MiB.

## Ownership and synchronization

A filter owns separate preprocessing and finishing execution pools. Entries
hold a device-local storage buffer and a tiny GPU-frame token whose producer
pair signals when preprocessing has been submitted and finishes. The token
is a synchronization object represented through the existing frame API; it
contains no image data required by the filter. There is no internal tall-frame
node and no node/filter reference cycle.

Before preprocessing, an entry is inserted in a mutex-protected map keyed by
original frame number. A shared promise/future deduplicates concurrent requests
for that number. Waiters wait for successful submission or receive the creator's
exception; they then use the token's producer pair for device-side completion.
Waiting never holds a cache mutex or an execution context. The preprocessing
context retains source frames and a shared entry until completion. The
finishing context retains every distinct entry and registers its token as a
read, so storage cannot disappear when LRU eviction removes a map reference.

Evicted entry references are released outside the cache mutex. Each entry
releases its token and GPU buffer when the last host/submission reference goes.
Filter destruction drains finishing and preprocessing pools before clearing the
map, constants or pipelines. Release callbacks only free the entry resources;
they do not destroy a filter, acquire a context or create/free/wait on a pool.

Endpoint clamping repeats the same source entry. Reverse, jump and repeat
requests are keyed lookups or fresh preprocessing operations, with no private
rolling cursor. Unprocessed output planes and all output properties come from
the original center frame. The core supplies producer waits and keeps declared
frame reads alive through their submissions.

## Cache budget

`cache=-1` selects reuse automatically for B=16,T>1, `cache=0` disables it,
and `cache=1` requires it. `cache_frames` selects the retained ready-entry count,
default 12 and maximum 128. `cache_mb=0` chooses a creation-time byte budget of
one quarter of `getVulkanCoreInfo().limit`; positive values supply a budget in
MiB. The effective count is the smaller of the requested count and the number
of complete spectra fitting that budget. Automatic mode falls back to direct
processing when the budget cannot hold one entry; forced caching reports an
error instead. A positive `cache_mb` replaces the automatic quarter-limit
choice and may request a larger cache; the core's global allocation and
submission limits still apply.

The core's configured VRAM limit is used rather than the physical heap size,
so the default respects a limit selected before graph creation. This remains
a per-instance cache-capacity choice. Pending preprocessing, entries retained
by active callers or queued finishing work, source-copy buffers, output frames
and scratch can consume additional VRAM. LRU removal changes recomputation
cost, not correctness. Reducing the core limit after graph creation does not
retroactively redefine the creation-time cache budget.

Implementation history is documented separately in
[optimization-notes.md](optimization-notes.md); its removed frame-backed cache
is not the current storage or ownership design.

## Evidence protocol

Validate cached output against the direct path and independent oracle for
all temporal windows, filters and curves; integer rounding; one-frame/short
clips; odd/tiny dimensions; subsampling and plane subsets; random/reverse/repeat
and concurrent requests; repeated destruction; and low VRAM budgets. Run
Vulkan synchronization validation on all-plane preprocessing, transfer copies,
entry tokens and finishing reads.

Benchmark cold setup, steady sequential throughput and random latency
separately. Store cache controls, actual retained bytes, source/output transfer
boundaries and source/DLL hashes. Finishing-node fused timestamps exclude
preprocessing work, so preprocessing must also be measured or complete
CPU-output timing must establish the total result. Initial exploration does
not establish a general speedup across devices and workloads.
