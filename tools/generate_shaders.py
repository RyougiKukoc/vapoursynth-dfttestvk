"""Regenerate GLSL FFT codelets from the GPL-3.0 dfttest2/FFTW source.

Usage: python tools/generate_shaders.py path/to/nvrtc_source/dft_kernels.hpp

The surrounding Vulkan implementation is maintained here, so regeneration also
records the exact input digest and preserves the source codelet attribution.
"""
from __future__ import annotations

import hashlib
from pathlib import Path
import re
import sys


def codelet(source: str, kind: str, n: int, output_name: str) -> str:
    raw_blocks = re.findall(r'R"""\((.*?)\)"""', source, re.S)
    raw = next(x for x in raw_blocks if re.search(rf"void {kind}<{n}>\(", x))
    body = raw[raw.index("{") + 1:raw.rindex("}")]
    body = re.sub(r"\s*using E = float;", "", body)
    body = re.sub(r"\s*auto \w+ = .*?;", "", body)
    body = re.sub(r"#pragma unroll\s+for \(int i = 0; i < 32; i\+\+\) \{\s*data\[i \* stride\] = 0\.0f;\s*\}", "", body)
    body = re.sub(r"\bE\b", "float", body)
    if kind in {"rdft", "irdft"}:
        for alias in ("R0", "Cr"):
            body = re.sub(rf"{alias}\[(\d+)\]", lambda m: f"data[{m[1]}]", body)
        for alias in ("R1", "Ci"):
            body = re.sub(rf"{alias}\[(\d+)\]", lambda m: f"data[{int(m[1]) + 1}]", body)
        signature = f"void {output_name}(inout float data[{n + 2}])"
    else:
        body = re.sub(r"(ri|ii|ro|io)\[(\d+) \* stride\]",
                      lambda m: f"data[{int(m[2]) // 2}].{'x' if m[1] in {'ri', 'ro'} else 'y'}", body)
        signature = f"void {output_name}(inout vec2 data[{n}])"
    return signature + " {" + body + "}\n"


INTERFACE = r'''
#extension GL_EXT_shader_8bit_storage : enable
#extension GL_EXT_shader_16bit_storage : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int16 : enable
#extension GL_EXT_shader_explicit_arithmetic_types_float16 : enable
layout(push_constant, std430) uniform Push {
    int width, height, dstStride, blocksX, blocksY, padX, padY;
    int srcStride[7];
} pc;
layout(set=0, binding=7, std430) buffer Scratch { float block[]; };
#ifndef EMBED_CONSTANTS
#define EMBED_CONSTANTS 0
#endif
#if !EMBED_CONSTANTS
layout(set=0, binding=8, std430) readonly buffer Constants { float constants[]; };
#endif
#ifndef ROW_CONSTANTS
#define ROW_CONSTANTS 0
#endif
// Constants: normalized synthesis/analysis window, its half spectrum, sigma.
const int WINDOW_FREQ = T * N * N;
const int SIGMA_OFFSET = WINDOW_FREQ + 2 * T * N * (N / 2 + 1);
#if !ROW_CONSTANTS
float analysisWindow(int row,int lane) {
    return constants[row*N+lane];
}
vec2 windowSpectrum(int row,int lane) {
    int frequency=row*(N/2+1)+lane;
    return vec2(constants[WINDOW_FREQ+2*frequency],constants[WINDOW_FREQ+2*frequency+1]);
}
float synthesisWindow(int localY,int localX) {
    return constants[(T/2)*N*N+localY*N+localX];
}
#endif
'''

FUSED = r'''
#ifndef TILES
#define TILES 4
#endif
#ifndef SUBGROUP
#define SUBGROUP 0
#endif
#ifndef UNROLL
#define UNROLL 0
#endif
#ifndef STATIC_MAIN
#define STATIC_MAIN 0
#endif
#ifndef SHORT_REFLECT
#define SHORT_REFLECT 0
#endif
#ifndef FFT_FMA
#define FFT_FMA 0
#endif
#ifndef REAL_SHARED
#define REAL_SHARED 0
#endif
#ifndef SHARED_PAD
#define SHARED_PAD 0
#endif
#ifndef SIGMA_IS_SCALAR
#define SIGMA_IS_SCALAR 0
#endif
#ifndef WARP_TILE
#define WARP_TILE 0
#endif
#if WARP_TILE && !SUBGROUP
#error WARP_TILE requires SUBGROUP and a full 32-lane subgroup at launch
#endif
#if SUBGROUP
#extension GL_KHR_shader_subgroup_basic : require
#define TILE_BARRIER() subgroupMemoryBarrierShared(); subgroupBarrier()
#else
#define TILE_BARRIER() barrier()
#endif
#if UNROLL
#extension GL_EXT_control_flow_attributes : require
#define UNROLL_LOOP [[unroll]]
#else
#define UNROLL_LOOP
#endif
#if WARP_TILE
layout(local_size_x=32*TILES, local_size_y=1, local_size_z=1) in;
#else
layout(local_size_x=N*TILES, local_size_y=1, local_size_z=1) in;
#endif
layout(set=0, binding=0, std430) readonly buffer Frame0 { SAMPLE src0[]; };
layout(set=0, binding=1, std430) readonly buffer Frame1 { SAMPLE src1[]; };
layout(set=0, binding=2, std430) readonly buffer Frame2 { SAMPLE src2[]; };
layout(set=0, binding=3, std430) readonly buffer Frame3 { SAMPLE src3[]; };
layout(set=0, binding=4, std430) readonly buffer Frame4 { SAMPLE src4[]; };
layout(set=0, binding=5, std430) readonly buffer Frame5 { SAMPLE src5[]; };
layout(set=0, binding=6, std430) readonly buffer Frame6 { SAMPLE src6[]; };

// The extra column avoids a power-of-two shared stride. All participating
// invocations synchronize, including inactive tail tiles. SUBGROUP requires
// the host to select full 32-lane subgroups; otherwise synchronization covers
// the entire workgroup and does not assume a vendor subgroup width.
#if SHARED_PAD
// The odd float stride puts the two 16-lane tiles of a 32-lane subgroup on
// disjoint bank parity when reading or writing the real component.
shared float transposeStorage[TILES * (2 * N * (N + 1) + 1)];
int transposeAddress(int index) {
#if WARP_TILE
    int tile = int(gl_SubgroupID);
#elif SUBGROUP
    int tile = int(gl_SubgroupID) * (int(gl_SubgroupSize) / N) + int(gl_SubgroupInvocationID) / N;
#else
    int tile = int(gl_LocalInvocationID.x) / N;
#endif
    return 2*index + tile;
}
vec2 transposeLoad(int index) {
    int address=transposeAddress(index);
    return vec2(transposeStorage[address],transposeStorage[address+1]);
}
void transposeStore(int index,vec2 value) {
    int address=transposeAddress(index);
    transposeStorage[address]=value.x;
    transposeStorage[address+1]=value.y;
}
void transposeStoreReal(int index,float value) {
    transposeStorage[transposeAddress(index)]=value;
}
float transposeLoadReal(int index) {
    return transposeStorage[transposeAddress(index)];
}
#else
shared vec2 transposeData[TILES * N * (N + 1)];
vec2 transposeLoad(int index) { return transposeData[index]; }
void transposeStore(int index,vec2 value) { transposeData[index]=value; }
void transposeStoreReal(int index,float value) { transposeData[index].x=value; }
float transposeLoadReal(int index) { return transposeData[index].x; }
#endif
shared float blockMean[TILES];

int reflectCoordinate(int value, int size) {
#if SHORT_REFLECT
    if (uint(value) < uint(size)) return value;
#endif
    if (size == 1) return 0;
    int period = 2 * size - 2;
    // GLSL leaves signed remainder with negative operands implementation
    // dependent. Mirror symmetry allows a nonnegative remainder instead.
    int q = abs(value) % period;
    return q < size ? q : period - q;
}

float sampleFrame(int frame, int x, int y) {
    int index = y * pc.srcStride[frame] + x;
    float result;
    switch (frame) {
        case 0: result = float(src0[index]); break;
        case 1: result = float(src1[index]); break;
        case 2: result = float(src2[index]); break;
        case 3: result = float(src3[index]); break;
        case 4: result = float(src4[index]); break;
        case 5: result = float(src5[index]); break;
        default: result = float(src6[index]); break;
    }
    return result * float(SCALE);
}

float getSigma(int index) {
#if SIGMA_IS_SCALAR
    return float(SIGMA_SCALAR);
#else
    return constants[SIGMA_OFFSET+index];
#endif
}

vec2 filterFrequency(vec2 value, float sigma) {
#if FTYPE == 2
    return value * sigma;
#else
    float psd = dot(value, value);
#if FTYPE == 1
    return psd < sigma ? vec2(0.0) : value;
#elif FTYPE == 0
    float multiplier = max((psd - sigma) / (psd + 1e-15), 0.0);
#elif FTYPE == 3
    float multiplier = psd >= float(PMIN) && psd <= float(PMAX) ? sigma : float(SIGMA2);
#elif FTYPE == 4
    float multiplier = sigma * sqrt(psd * (float(PMAX) /
        ((psd + float(PMIN)) * (psd + float(PMAX)) + 1e-15)));
#elif FTYPE == 5
    float multiplier = 1.0;
    if (float(PMIN) != 0.0)
        multiplier = pow(max((psd - sigma) / (psd + 1e-15), 0.0), float(PMIN));
#else
    float multiplier = sqrt(max((psd - sigma) / (psd + 1e-15), 0.0));
#endif
#if FTYPE != 1
    return value * multiplier;
#endif
#endif
}

// Only the central temporal sample survives. Computing this row directly
// avoids the other T-1 output rows of the inverse temporal transform.
vec2 inverseTemporalCenter(vec2 value[T]) {
#if T == 1
    return value[0];
#elif T == 3
    const vec2 phase[3] = vec2[3](vec2(1.0,0.0), vec2(-0.5,0.8660254037844386), vec2(-0.5,-0.8660254037844386));
#elif T == 5
    const vec2 phase[5] = vec2[5](vec2(1.0,0.0), vec2(-0.8090169943749475,0.5877852522924731), vec2(0.3090169943749475,-0.9510565162951535), vec2(0.3090169943749475,0.9510565162951535), vec2(-0.8090169943749475,-0.5877852522924731));
#elif T == 7
    const vec2 phase[7] = vec2[7](vec2(1.0,0.0), vec2(-0.9009688679024191,0.4338837391175581), vec2(0.6234898018587336,-0.7818314824680298), vec2(-0.2225209339563144,0.9749279121818236), vec2(-0.2225209339563144,-0.9749279121818236), vec2(0.6234898018587336,0.7818314824680298), vec2(-0.9009688679024191,-0.4338837391175581));
#endif
#if T != 1
    vec2 result = vec2(0.0);
    for (int t=0; t<T; ++t)
        result += vec2(value[t].x * phase[t].x - value[t].y * phase[t].y,
                       value[t].x * phase[t].y + value[t].y * phase[t].x);
    return result;
#endif
}

void main() {
#if WARP_TILE
    int tile = int(gl_SubgroupID);
    int lane = int(gl_SubgroupInvocationID);
    // The host launches full 32-lane subgroups. The upper half exits before
    // any shared access; subsequent subgroup barriers synchronize the same
    // surviving 16 invocations and never synchronize the whole workgroup.
    if (lane >= N) return;
#elif SUBGROUP
    int tile = int(gl_SubgroupID) * (int(gl_SubgroupSize) / N) + int(gl_SubgroupInvocationID) / N;
    int lane = int(gl_SubgroupInvocationID) % N;
#else
    int tile = int(gl_LocalInvocationID.x) / N;
    int lane = int(gl_LocalInvocationID.x) % N;
#endif
    int groupId = int(gl_WorkGroupID.y * gl_NumWorkGroups.x + gl_WorkGroupID.x);
    int blockId = groupId * TILES + tile;
    bool validBlock = blockId < pc.blocksX * pc.blocksY;
    int originX = (blockId % pc.blocksX) * STEP - pc.padX;
    int originY = (blockId / pc.blocksX) * STEP - pc.padY;
    int sharedBase = tile * N * (N + 1);
    vec2 spectrum[T * N];

    for (int t=0; t<T; ++t) {
        int x = reflectCoordinate(originX + lane, pc.width);
        for (int y=0; y<N; ++y) {
            float v = validBlock ? sampleFrame(t, x, reflectCoordinate(originY+y, pc.height)) : 0.0;
#if REAL_SHARED
            transposeStoreReal(sharedBase + y * (N + 1) + lane,
                v * analysisWindow(t*N+y,lane));
#else
            transposeStore(sharedBase + y * (N + 1) + lane,
                vec2(v * analysisWindow(t*N+y,lane), 0.0));
#endif
        }
        TILE_BARRIER();
        float realData[N + 2];
        for (int x=0; x<N; ++x)
            realData[x] = transposeLoadReal(sharedBase + lane * (N + 1) + x);
        TILE_BARRIER();
        rdft16(realData);
        for (int x=0; x<=N/2; ++x)
            transposeStore(sharedBase + lane * (N + 1) + x,vec2(realData[2*x], realData[2*x+1]));
        TILE_BARRIER();
        if (lane <= N/2) {
            vec2 column[N];
            for (int y=0; y<N; ++y)
                column[y] = transposeLoad(sharedBase + y * (N + 1) + lane);
            dft16(column);
            for (int y=0; y<N; ++y) spectrum[t*N+y] = column[y];
        }
        TILE_BARRIER();
    }

    if (lane <= N/2) {
        for (int y=0; y<N; ++y) {
            vec2 temporal[T];
            for (int t=0; t<T; ++t) temporal[t] = spectrum[t*N+y];
            dftTemporal(temporal);
            for (int t=0; t<T; ++t) spectrum[t*N+y] = temporal[t];
        }
    }
#if ZMEAN
    if (lane == 0) blockMean[tile] = spectrum[0].x / windowSpectrum(0,0).x;
    TILE_BARRIER();
#endif
    vec2 column[N];
    if (lane <= N/2) {
        for (int y=0; y<N; ++y) {
            vec2 temporal[T];
            for (int t=0; t<T; ++t) {
                int frequency = (t * N + y) * (N / 2 + 1) + lane;
                vec2 v = spectrum[t*N+y];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(t*N+y,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[t] = v;
            }
            column[y] = inverseTemporalCenter(temporal);
        }
        idft16(column);
        for (int y=0; y<N; ++y)
            transposeStore(sharedBase + y * (N + 1) + lane,column[y]);
    }
    TILE_BARRIER();
    float realData[N + 2];
    for (int x=0; x<=N/2; ++x) {
        vec2 v = transposeLoad(sharedBase + lane * (N + 1) + x);
        realData[2*x] = v.x;
        realData[2*x+1] = v.y;
    }
    TILE_BARRIER();
    irdft16(realData);
    // FFTW's generated r2cb codelet uses the reverse real ordering.
    for (int x=0; x<N; ++x)
        transposeStoreReal(sharedBase + x * (N + 1) + lane,realData[x == 0 ? 0 : N-x]);
    TILE_BARRIER();
    if (validBlock)
        for (int y=0; y<N; ++y)
            block[blockId*N*N + y*N + lane] = transposeLoadReal(sharedBase + lane*(N+1)+y);
}
'''

GATHER = r'''
layout(local_size_x=16, local_size_y=16, local_size_z=1) in;
layout(set=0, binding=9, std430) writeonly buffer Destination { SAMPLE dst[]; };

void main() {
    int x = int(gl_GlobalInvocationID.x);
    int y = int(gl_GlobalInvocationID.y);
    if (x >= pc.width || y >= pc.height) return;
    int px = x + pc.padX;
    int py = y + pc.padY;
    int firstY = max(0, (py - N + STEP) / STEP);
    int lastY = min(py / STEP, pc.blocksY - 1);
    int firstX = max(0, (px - N + STEP) / STEP);
    int lastX = min(px / STEP, pc.blocksX - 1);
    float value = 0.0;
    for (int by=firstY; by<=lastY; ++by) {
        int localY = py - by * STEP;
        for (int bx=firstX; bx<=lastX; ++bx) {
            int localX = px - bx * STEP;
            int index = localY * N + localX;
            value += block[(by * pc.blocksX + bx) * N * N + index] *
                     synthesisWindow(localY,localX);
        }
    }
    value /= float(SCALE);
#if INTEGER
    value = clamp(value + 0.5, 0.0, float(PEAK));
#endif
    dst[y * pc.dstStride + x] = SAMPLE(value);
}
'''


def expand_fixed_loops(text: str, n: int, t: int) -> str:
    """Expand this template's counted loops with scoped constant substitutions.

    Each expanded for remains one compound statement, so an enclosing if
    controls the whole original loop. Iteration scopes preserve the lifetime
    of temporary arrays/scalars and allow the compiler to reuse registers.
    """
    pattern = re.compile(r"for \(int ([a-z]+)=0; \1(<|<=)(N/2|N|T); \+\+\1\)")
    sizes = {"N": n, "N/2": n//2, "T": t}
    while match := pattern.search(text):
        variable, operator, bound = match.groups()
        start = match.end()
        while text[start].isspace():
            start += 1
        if text[start] == "{":
            depth = 1
            end = start + 1
            while depth:
                if text[end] == "{":
                    depth += 1
                elif text[end] == "}":
                    depth -= 1
                end += 1
            body = text[start+1:end-1]
        else:
            end = text.index(";",start)+1
            body = text[start:end]
        # Expand inner loops before replacing an outer counter. All counters
        # are scoped and distinct from inner counters in this GLSL template.
        body = expand_fixed_loops(body,n,t)
        count = sizes[bound] + int(operator == "<=")
        # A spatial counter named x/y must not replace vector .x/.y members.
        iterations = ["{\n" + re.sub(rf"(?<![.\w]){variable}\b",str(index),body) + "\n}" for index in range(count)]
        expanded = "{\n" + "\n".join(iterations) + "\n}"
        text = text[:match.start()] + expanded + text[end:]
    if "for (" in text:
        raise ValueError("Unexpected loop in static main; update the expander")
    return text


def cached_mains() -> tuple[str,str]:
    """Split the measured direct kernel at its spatial/temporal boundary.

    Cache the center analysis window's half spectrum once per source frame.
    The finish kernel restores every temporal slice's analysis factor before
    the temporal transform; all later frequency/inverse arithmetic is shared
    with the direct path.
    """
    direct = FUSED[FUSED.index("void main() {"):]
    spatial_start = direct.index("    for (int t=0; t<T; ++t) {")
    temporal_start = direct.index("    if (lane <= N/2) {\n        for (int y=0; y<N; ++y) {\n            vec2 temporal[T];")
    preamble = direct[:spatial_start]
    spatial = direct[spatial_start:temporal_start]
    # Remove the outer temporal loop. A single original source frame gets
    # multiplied by the exact stored central window before either spatial FFT.
    first_open = spatial.index("{")
    spatial = spatial[first_open+1:spatial.rfind("}")]
    spatial = re.sub(r"(?<![.\w])t\b", "0", spatial)
    spatial = spatial.replace("analysisWindow(0*N+y,lane)","analysisWindow((T/2)*N+y,lane)")
    spatial = spatial.replace("spectrum[0*N+y] = column[y]", "spectrum[y] = column[y]")
    forward = preamble.replace("vec2 spectrum[T * N];","vec2 spectrum[N];") + spatial
    forward += r'''
    if (validBlock && lane <= N/2) {
        for (int y=0; y<N; ++y) {
            int destination = blockId*pc.dstStride + 2*(y*(N/2+1)+lane);
            cachedDestination[destination]=spectrum[y].x;
            cachedDestination[destination+1]=spectrum[y].y;
        }
    }
}
'''
    finish = preamble + r'''
    if (lane <= N/2) {
        for (int t=0; t<T; ++t) {
            for (int y=0; y<N; ++y) {
                spectrum[t*N+y] = validBlock ? readCachedSpectrum(t,blockId,y,lane)*temporalFactors[t] : vec2(0.0);
            }
        }
    }
''' + direct[temporal_start:]
    return forward,finish


def render_main(main_body: str) -> str:
    rendered = "#if STATIC_MAIN\n"
    for t in (1,3,5,7):
        rendered += f"#if T == {t}\n" + expand_fixed_loops(main_body,16,t) + "#endif\n"
    rendered += "#else\n" + re.sub(r"(?m)^(\s*)(for \(int [^\n]+)",r"\1UNROLL_LOOP \2",main_body) + "#endif\n"
    return rendered


def main() -> None:
    source_path = Path(sys.argv[1])
    source_bytes = source_path.read_bytes()
    source = source_bytes.decode("utf-8")
    generated = """#pragma once

// GLSL codelets adapted from vapoursynth-dfttest2's nvrtc_source/dft_kernels.hpp,
// derived there from FFTW 3.3.10. The FFTW codelet generator commands are
// preserved in the source reference. Both projects are licensed GPL-3.0.
// Regenerate with tools/generate_shaders.py; source SHA256:
// """ + hashlib.sha256(source_bytes).hexdigest() + "\n\nnamespace shader {\ninline const char* fused = R\"glsl(\n"
    fused_body_start = FUSED.index("#if WARP_TILE\nlayout(local_size_x=")
    common = FUSED[:fused_body_start] + INTERFACE
    common += """
#define DK(name, value) const float name = float(value)
#if FFT_FMA
#define FMA(a,b,c) fma((a),(b),(c))
#define FMS(a,b,c) fma((a),(b),-(c))
#define FNMA(a,b,c) (-fma((a),(b),(c)))
#define FNMS(a,b,c) fma(-(a),(b),(c))
#else
#define FMA(a,b,c) ((a)*(b)+(c))
#define FMS(a,b,c) ((a)*(b)-(c))
#define FNMA(a,b,c) (-((a)*(b)+(c)))
#define FNMS(a,b,c) ((c)-(a)*(b))
#endif
"""
    for kind in ("rdft", "dft", "idft", "irdft"):
        common += codelet(source, kind, 16, kind + "16")
    for n in (1, 3, 5, 7):
        common += f"#if T == {n}\n" + codelet(source, "dft", n, "dftTemporal") + "#endif\n"
    generated += common
    # The fixed-size FFT loops have private-array indexing. Explicit optional
    # unroll controls allow profiling how this affects register allocation.
    # STATIC_MAIN provides a stronger ablation: private-array accesses are
    # already literal indices before GLSL->SPIR-V compilation, so this path
    # does not depend on the driver's interpretation of loop hints.
    fused_body = FUSED[fused_body_start:]
    main_start = fused_body.index("void main() {")
    inverse_start = fused_body.index("vec2 inverseTemporalCenter(")
    helpers = fused_body[:inverse_start]
    inverse = fused_body[inverse_start:main_start]
    main_body = fused_body[main_start:]
    fused = helpers + "#if STATIC_MAIN\n"
    for t in (1, 3, 5, 7):
        fused += f"#if T == {t}\n" + expand_fixed_loops(inverse,16,t) + expand_fixed_loops(main_body,16,t) + "#endif\n"
    fused += "#else\n" + re.sub(r"(?m)^(\s*)(for \(int [^\n]+)", r"\1UNROLL_LOOP \2", inverse + main_body) + "#endif\n"
    generated += fused + ')glsl";\n\ninline const char* gather = R"glsl(\n' + INTERFACE + GATHER + ')glsl";\n} // namespace shader\n'
    destination = Path(__file__).resolve().parents[1] / "src" / "shaders.h"
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(generated, encoding="utf-8", newline="\n")
    print(destination)
    # These are independent shaders, so only the cache preprocessing pass
    # writes binding 9. Finish writes binding 7 and reuses the existing gather.
    cached_forward,cached_finish = cached_mains()
    forward_common = common + helpers + r'''
layout(set=0,binding=9,std430) writeonly buffer CachedDestination { float cachedDestination[]; };
'''
    finish_common = common + helpers
    finish_common = re.sub(r"(readonly buffer Frame[0-6] \{ )SAMPLE",r"\1float",finish_common)
    finish_common += r'''
vec2 readCachedSpectrum(int frame,int blockId,int y,int lane) {
    int index=blockId*pc.srcStride[frame]+2*(y*(N/2+1)+lane);
    switch (frame) {
        case 0:return vec2(src0[index],src0[index+1]);
        case 1:return vec2(src1[index],src1[index+1]);
        case 2:return vec2(src2[index],src2[index+1]);
        case 3:return vec2(src3[index],src3[index+1]);
        case 4:return vec2(src4[index],src4[index+1]);
        case 5:return vec2(src5[index],src5[index+1]);
        default:return vec2(src6[index],src6[index+1]);
    }
}
'''
    cached = generated[:generated.index("namespace shader {")] + "namespace cached_shader {\n"
    cached += 'inline const char* forward = R"glsl(\n' + forward_common + render_main(cached_forward) + ')glsl";\n'
    cached += 'inline const char* finish = R"glsl(\n' + finish_common + render_main(inverse+cached_finish) + ')glsl";\n'
    cached += "} // namespace cached_shader\n"
    cached_destination = destination.with_name("cached_shaders.h")
    cached_destination.write_text(cached,encoding="utf-8",newline="\n")
    print(cached_destination)


if __name__ == "__main__":
    main()
