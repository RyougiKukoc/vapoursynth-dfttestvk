#pragma once

// General block-size path. Power-of-two spatial sizes use an in-place radix-2
// FFT; other sizes use a direct DFT. The 16-point codelets in shaders.h remain
// the optimized path. One 64-lane workgroup processes one independent block.
namespace generic_shader {
inline const char* fused = R"glsl(
#extension GL_EXT_shader_8bit_storage : enable
#extension GL_EXT_shader_16bit_storage : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int16 : enable
#extension GL_EXT_shader_explicit_arithmetic_types_float16 : enable
layout(local_size_x=64, local_size_y=1, local_size_z=1) in;
layout(push_constant, std430) uniform Push {
    int width, height, dstStride, blocksX, blocksY, padX, padY;
    int srcStride[7];
} pc;
layout(set=0, binding=0, std430) readonly buffer Frame0 { SAMPLE src0[]; };
layout(set=0, binding=1, std430) readonly buffer Frame1 { SAMPLE src1[]; };
layout(set=0, binding=2, std430) readonly buffer Frame2 { SAMPLE src2[]; };
layout(set=0, binding=3, std430) readonly buffer Frame3 { SAMPLE src3[]; };
layout(set=0, binding=4, std430) readonly buffer Frame4 { SAMPLE src4[]; };
layout(set=0, binding=5, std430) readonly buffer Frame5 { SAMPLE src5[]; };
layout(set=0, binding=6, std430) readonly buffer Frame6 { SAMPLE src6[]; };
layout(set=0, binding=7, std430) buffer Scratch { float block[]; };
#ifndef EMBED_CONSTANTS
#define EMBED_CONSTANTS 0
#endif
#if !EMBED_CONSTANTS
layout(set=0, binding=8, std430) readonly buffer Constants { float constants[]; };
#endif
const int WINDOW_FREQ = T * N * N;
const int SIGMA_OFFSET = WINDOW_FREQ + 2 * T * N * (N / 2 + 1);
#ifndef ROW_CONSTANTS
#define ROW_CONSTANTS 0
#endif
#if !ROW_CONSTANTS
float analysisWindow(int row,int lane) { return constants[row*N+lane]; }
vec2 windowSpectrum(int row,int lane) {
    int frequency=row*(N/2+1)+lane;
    return vec2(constants[WINDOW_FREQ+2*frequency],constants[WINDOW_FREQ+2*frequency+1]);
}
float synthesisWindow(int localY,int localX) {
    return constants[(T/2)*N*N+localY*N+localX];
}
#endif
const float PI2 = 6.283185307179586476925286766559;
shared vec2 transposeData[N * (N + 1)];
shared float blockMean;

int reflectCoordinate(int value, int size) {
    if (size == 1) return 0;
    int period = 2 * size - 2;
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
        case 6: result = float(src6[index]); break;
        default: result = 0.0; break;
    }
    return result * float(SCALE);
}

vec2 multiplyComplex(vec2 a, vec2 b) {
    return vec2(a.x*b.x-a.y*b.y, a.x*b.y+a.y*b.x);
}

void spatialTransform(inout vec2 value[N], bool inverse) {
#if (N & (N - 1)) == 0
    // Bit-reversal permutation followed by decimation-in-time butterflies.
    int reverse = 0;
    for (int index=1; index<N; ++index) {
        int bit = N / 2;
        while ((reverse & bit) != 0) {
            reverse ^= bit;
            bit >>= 1;
        }
        reverse ^= bit;
        if (index < reverse) {
            vec2 temporary = value[index];
            value[index] = value[reverse];
            value[reverse] = temporary;
        }
    }
    for (int length=2; length<=N; length*=2) {
        int halfLength = length / 2;
        for (int offset=0; offset<N; offset+=length) {
            for (int k=0; k<halfLength; ++k) {
                float phase = (inverse ? PI2 : -PI2) * float(k) / float(length);
                vec2 a = value[offset+k];
                vec2 b = multiplyComplex(value[offset+k+halfLength], vec2(cos(phase),sin(phase)));
                value[offset+k] = a + b;
                value[offset+k+halfLength] = a - b;
            }
        }
    }
#else
    vec2 transformed[N];
    for (int frequency=0; frequency<N; ++frequency) {
        vec2 sum = vec2(0.0);
        for (int index=0; index<N; ++index) {
            // Reduce the phase index before sin/cos to preserve small sizes'
            // accuracy and exploit periodicity for large direct-DFT blocks.
            float phase = (inverse ? PI2 : -PI2) * float((index*frequency)%N) / float(N);
            sum += multiplyComplex(value[index], vec2(cos(phase),sin(phase)));
        }
        transformed[frequency] = sum;
    }
    for (int index=0; index<N; ++index) value[index] = transformed[index];
#endif
}

void temporalTransform(inout vec2 value[T]) {
#if T > 1
    vec2 transformed[T];
    for (int frequency=0; frequency<T; ++frequency) {
        vec2 sum = vec2(0.0);
        for (int index=0; index<T; ++index) {
            float phase = -PI2 * float((index*frequency)%T) / float(T);
            sum += multiplyComplex(value[index], vec2(cos(phase),sin(phase)));
        }
        transformed[frequency] = sum;
    }
    for (int index=0; index<T; ++index) value[index] = transformed[index];
#endif
}

#ifndef SIGMA_IS_SCALAR
#define SIGMA_IS_SCALAR 0
#endif
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

void main() {
    int lane = int(gl_LocalInvocationID.x);
    int blockId = int(gl_WorkGroupID.y * gl_NumWorkGroups.x + gl_WorkGroupID.x);
    bool validBlock = blockId < pc.blocksX * pc.blocksY;
    int originX = (blockId % pc.blocksX) * STEP - pc.padX;
    int originY = (blockId / pc.blocksX) * STEP - pc.padY;
    vec2 spectrum[T * N];
    for (int t=0; t<T; ++t) {
        if (lane < N) {
            int x = reflectCoordinate(originX + lane, pc.width);
            for (int y=0; y<N; ++y) {
                float pixel = validBlock ? sampleFrame(t, x, reflectCoordinate(originY+y, pc.height)) : 0.0;
                transposeData[y*(N+1)+lane] = vec2(pixel*analysisWindow(t*N+y,lane),0.0);
            }
        }
        barrier();
        vec2 row[N];
        if (lane < N)
            for (int x=0; x<N; ++x) row[x] = transposeData[lane*(N+1)+x];
        barrier();
        if (lane < N) {
            spatialTransform(row, false);
            for (int x=0; x<N; ++x) transposeData[lane*(N+1)+x] = row[x];
        }
        barrier();
        if (lane < N) {
            for (int y=0; y<N; ++y) row[y] = transposeData[y*(N+1)+lane];
            spatialTransform(row, false);
            for (int y=0; y<N; ++y) spectrum[t*N+y] = row[y];
        }
        barrier();
    }
    if (lane < N) {
        for (int y=0; y<N; ++y) {
            vec2 temporal[T];
            for (int t=0; t<T; ++t) temporal[t] = spectrum[t*N+y];
            temporalTransform(temporal);
            for (int t=0; t<T; ++t) spectrum[t*N+y] = temporal[t];
        }
    }
#if ZMEAN
    if (lane == 0) blockMean = spectrum[0].x / windowSpectrum(0,0).x;
    barrier();
#endif
    vec2 column[N];
    if (lane < N) {
        for (int y=0; y<N; ++y) {
            vec2 sum = vec2(0.0);
            for (int t=0; t<T; ++t) {
                // The negative-x half of a real-input spectrum is the complex
                // conjugate of (-t,-y,-x), including its sigma/window lookup.
                bool conjugate = lane > N / 2;
                int fx = conjugate ? N-lane : lane;
                int fy = conjugate ? (N-y)%N : y;
                int ft = conjugate ? (T-t)%T : t;
                int frequency = (ft*N+fy)*(N/2+1)+fx;
                vec2 value = spectrum[t*N+y];
#if ZMEAN
                vec2 meanFrequency = blockMean*windowSpectrum(ft*N+fy,fx);
                if (conjugate) meanFrequency.y = -meanFrequency.y;
                value -= meanFrequency;
#endif
                value = filterFrequency(value, getSigma(frequency));
#if ZMEAN
                value += meanFrequency;
#endif
                float phase = PI2 * float((t*(T/2))%T) / float(T);
                sum += multiplyComplex(value,vec2(cos(phase),sin(phase)));
            }
            column[y] = sum;
        }
        spatialTransform(column,true);
        for (int y=0; y<N; ++y) transposeData[y*(N+1)+lane] = column[y];
    }
    barrier();
    if (lane < N)
        for (int x=0; x<N; ++x) column[x] = transposeData[lane*(N+1)+x];
    barrier();
    if (lane < N) {
        spatialTransform(column,true);
        for (int x=0; x<N; ++x) transposeData[x*(N+1)+lane] = column[x];
    }
    barrier();
    if (validBlock && lane < N)
        for (int y=0; y<N; ++y) block[blockId*N*N+y*N+lane] = transposeData[lane*(N+1)+y].x;
}
)glsl";
} // namespace generic_shader
