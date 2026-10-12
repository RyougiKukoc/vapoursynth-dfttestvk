#pragma once

// GLSL codelets adapted from vapoursynth-dfttest2's nvrtc_source/dft_kernels.hpp,
// derived there from FFTW 3.3.10. The FFTW codelet generator commands are
// preserved in the source reference. Both projects are licensed GPL-3.0.
// Regenerate with tools/generate_shaders.py; source SHA256:
// 65f5f05fdab3e10acbb2ee0ac18b92a35c612c41e25c7f47c6e3c38bce474d79

namespace cached_shader {
inline const char* forward = R"glsl(

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
void rdft16(inout float data[18]) {

    DK(KP923879532, +0.923879532511286756128183189396788286822416626);
    DK(KP707106781, +0.707106781186547524400844362104849039284835938);
    DK(KP414213562, +0.414213562373095048801688724209698078569671875);

    float T3;
    float T6;
    float T7;
    float T49;
    float T37;
    float T10;
    float T13;
    float T14;
    float T50;
    float T40;
    float T22;
    float T55;
    float T34;
    float T44;
    float T29;
    float T54;
    float T35;
    float T47;
    float T38;
    float T39;
    float T53;
    float T56;
    {
        float T1;
        float T2;
        float T4;
        float T5;
        T1 = data[0];
        T2 = data[8];
        T3 = T1 + T2;
        T4 = data[4];
        T5 = data[12];
        T6 = T4 + T5;
        T7 = T3 + T6;
        T49 = T4 - T5;
        T37 = T1 - T2;
    }
    {
        float T8;
        float T9;
        float T11;
        float T12;
        T8 = data[2];
        T9 = data[10];
        T10 = T8 + T9;
        T38 = T8 - T9;
        T11 = data[14];
        T12 = data[6];
        T13 = T11 + T12;
        T39 = T11 - T12;
    }
    T14 = T10 + T13;
    T50 = T39 - T38;
    T40 = T38 + T39;
    {
        float T18;
        float T42;
        float T21;
        float T43;
        {
            float T16;
            float T17;
            float T19;
            float T20;
            T16 = data[1];
            T17 = data[9];
            T18 = T16 + T17;
            T42 = T16 - T17;
            T19 = data[5];
            T20 = data[13];
            T21 = T19 + T20;
            T43 = T19 - T20;
        }
        T22 = T18 - T21;
        T55 = FMA(KP414213562, T42, T43);
        T34 = T18 + T21;
        T44 = FNMS(KP414213562, T43, T42);
    }
    {
        float T25;
        float T45;
        float T28;
        float T46;
        {
            float T23;
            float T24;
            float T26;
            float T27;
            T23 = data[15];
            T24 = data[7];
            T25 = T23 + T24;
            T45 = T23 - T24;
            T26 = data[3];
            T27 = data[11];
            T28 = T26 + T27;
            T46 = T27 - T26;
        }
        T29 = T25 - T28;
        T54 = FMA(KP414213562, T45, T46);
        T35 = T25 + T28;
        T47 = FNMS(KP414213562, T46, T45);
    }
    data[8] = T7 - T14;
    data[9] = T35 - T34;
    {
        float T15;
        float T30;
        float T31;
        float T32;
        T15 = T3 - T6;
        T30 = T22 + T29;
        data[12] = FNMS(KP707106781, T30, T15);
        data[4] = FMA(KP707106781, T30, T15);
        T31 = T13 - T10;
        T32 = T29 - T22;
        data[5] = FMA(KP707106781, T32, T31);
        data[13] = FMS(KP707106781, T32, T31);
    }
    {
        float T33;
        float T36;
        float T41;
        float T48;
        T33 = T7 + T14;
        T36 = T34 + T35;
        data[16] = T33 - T36;
        data[0] = T33 + T36;
        T41 = FMA(KP707106781, T40, T37);
        T48 = T44 + T47;
        data[14] = FNMS(KP923879532, T48, T41);
        data[2] = FMA(KP923879532, T48, T41);
    }
    T53 = FNMS(KP707106781, T50, T49);
    T56 = T54 - T55;
    data[3] = FMS(KP923879532, T56, T53);
    data[15] = FMA(KP923879532, T56, T53);
    {
        float T57;
        float T58;
        float T51;
        float T52;
        T57 = FNMS(KP707106781, T40, T37);
        T58 = T55 + T54;
        data[10] = FNMS(KP923879532, T58, T57);
        data[6] = FMA(KP923879532, T58, T57);
        T51 = FMA(KP707106781, T50, T49);
        T52 = T47 - T44;
        data[7] = FMA(KP923879532, T52, T51);
        data[11] = FMS(KP923879532, T52, T51);
    }
    data[1] = 0.0f;
    data[17] = 0.0f;
}
void dft16(inout vec2 data[16]) {

    DK(KP923879532, +0.923879532511286756128183189396788286822416626);
    DK(KP414213562, +0.414213562373095048801688724209698078569671875);
    DK(KP707106781, +0.707106781186547524400844362104849039284835938);

    float T7;
    float T115;
    float T129;
    float T38;
    float T49;
    float T95;
    float T105;
    float T83;
    float T29;
    float T126;
    float T141;
    float T73;
    float T78;
    float T102;
    float T123;
    float T101;
    float T14;
    float T116;
    float T130;
    float T45;
    float T52;
    float T84;
    float T85;
    float T55;
    float T22;
    float T121;
    float T140;
    float T62;
    float T67;
    float T99;
    float T118;
    float T98;
    {
        float T3;
        float T47;
        float T34;
        float T82;
        float T6;
        float T81;
        float T37;
        float T48;
        {
            float T1;
            float T2;
            float T32;
            float T33;
            T1 = data[0].x;
            T2 = data[8].x;
            T3 = T1 + T2;
            T47 = T1 - T2;
            T32 = data[0].y;
            T33 = data[8].y;
            T34 = T32 + T33;
            T82 = T32 - T33;
        }
        {
            float T4;
            float T5;
            float T35;
            float T36;
            T4 = data[4].x;
            T5 = data[12].x;
            T6 = T4 + T5;
            T81 = T4 - T5;
            T35 = data[4].y;
            T36 = data[12].y;
            T37 = T35 + T36;
            T48 = T35 - T36;
        }
        T7 = T3 + T6;
        T115 = T3 - T6;
        T129 = T34 - T37;
        T38 = T34 + T37;
        T49 = T47 - T48;
        T95 = T47 + T48;
        T105 = T82 - T81;
        T83 = T81 + T82;
    }
    {
        float T25;
        float T74;
        float T72;
        float T124;
        float T28;
        float T69;
        float T77;
        float T125;
        {
            float T23;
            float T24;
            float T70;
            float T71;
            T23 = data[15].x;
            T24 = data[7].x;
            T25 = T23 + T24;
            T74 = T23 - T24;
            T70 = data[15].y;
            T71 = data[7].y;
            T72 = T70 - T71;
            T124 = T70 + T71;
        }
        {
            float T26;
            float T27;
            float T75;
            float T76;
            T26 = data[3].x;
            T27 = data[11].x;
            T28 = T26 + T27;
            T69 = T26 - T27;
            T75 = data[3].y;
            T76 = data[11].y;
            T77 = T75 - T76;
            T125 = T75 + T76;
        }
        T29 = T25 + T28;
        T126 = T124 - T125;
        T141 = T124 + T125;
        T73 = T69 + T72;
        T78 = T74 - T77;
        T102 = T72 - T69;
        T123 = T25 - T28;
        T101 = T74 + T77;
    }
    {
        float T10;
        float T51;
        float T41;
        float T50;
        float T13;
        float T53;
        float T44;
        float T54;
        {
            float T8;
            float T9;
            float T39;
            float T40;
            T8 = data[2].x;
            T9 = data[10].x;
            T10 = T8 + T9;
            T51 = T8 - T9;
            T39 = data[2].y;
            T40 = data[10].y;
            T41 = T39 + T40;
            T50 = T39 - T40;
        }
        {
            float T11;
            float T12;
            float T42;
            float T43;
            T11 = data[14].x;
            T12 = data[6].x;
            T13 = T11 + T12;
            T53 = T11 - T12;
            T42 = data[14].y;
            T43 = data[6].y;
            T44 = T42 + T43;
            T54 = T42 - T43;
        }
        T14 = T10 + T13;
        T116 = T41 - T44;
        T130 = T13 - T10;
        T45 = T41 + T44;
        T52 = T50 - T51;
        T84 = T53 - T54;
        T85 = T51 + T50;
        T55 = T53 + T54;
    }
    {
        float T18;
        float T63;
        float T61;
        float T119;
        float T21;
        float T58;
        float T66;
        float T120;
        {
            float T16;
            float T17;
            float T59;
            float T60;
            T16 = data[1].x;
            T17 = data[9].x;
            T18 = T16 + T17;
            T63 = T16 - T17;
            T59 = data[1].y;
            T60 = data[9].y;
            T61 = T59 - T60;
            T119 = T59 + T60;
        }
        {
            float T19;
            float T20;
            float T64;
            float T65;
            T19 = data[5].x;
            T20 = data[13].x;
            T21 = T19 + T20;
            T58 = T19 - T20;
            T64 = data[5].y;
            T65 = data[13].y;
            T66 = T64 - T65;
            T120 = T64 + T65;
        }
        T22 = T18 + T21;
        T121 = T119 - T120;
        T140 = T119 + T120;
        T62 = T58 + T61;
        T67 = T63 - T66;
        T99 = T61 - T58;
        T118 = T18 - T21;
        T98 = T63 + T66;
    }
    {
        float T15;
        float T30;
        float T143;
        float T144;
        T15 = T7 + T14;
        T30 = T22 + T29;
        data[8].x = T15 - T30;
        data[0].x = T15 + T30;
        T143 = T38 + T45;
        T144 = T140 + T141;
        data[8].y = T143 - T144;
        data[0].y = T143 + T144;
    }
    {
        float T31;
        float T46;
        float T139;
        float T142;
        T31 = T29 - T22;
        T46 = T38 - T45;
        data[4].y = T31 + T46;
        data[12].y = T46 - T31;
        T139 = T7 - T14;
        T142 = T140 - T141;
        data[12].x = T139 - T142;
        data[4].x = T139 + T142;
    }
    {
        float T117;
        float T131;
        float T128;
        float T132;
        float T122;
        float T127;
        T117 = T115 + T116;
        T131 = T129 - T130;
        T122 = T118 + T121;
        T127 = T123 - T126;
        T128 = T122 + T127;
        T132 = T127 - T122;
        data[10].x = FNMS(KP707106781, T128, T117);
        data[6].y = FMA(KP707106781, T132, T131);
        data[2].x = FMA(KP707106781, T128, T117);
        data[14].y = FNMS(KP707106781, T132, T131);
    }
    {
        float T133;
        float T137;
        float T136;
        float T138;
        float T134;
        float T135;
        T133 = T115 - T116;
        T137 = T130 + T129;
        T134 = T121 - T118;
        T135 = T123 + T126;
        T136 = T134 - T135;
        T138 = T134 + T135;
        data[14].x = FNMS(KP707106781, T136, T133);
        data[2].y = FMA(KP707106781, T138, T137);
        data[6].x = FMA(KP707106781, T136, T133);
        data[10].y = FNMS(KP707106781, T138, T137);
    }
    {
        float T57;
        float T93;
        float T87;
        float T89;
        float T80;
        float T88;
        float T92;
        float T94;
        float T56;
        float T86;
        T56 = T52 - T55;
        T57 = FMA(KP707106781, T56, T49);
        T93 = FNMS(KP707106781, T56, T49);
        T86 = T84 - T85;
        T87 = FNMS(KP707106781, T86, T83);
        T89 = FMA(KP707106781, T86, T83);
        {
            float T68;
            float T79;
            float T90;
            float T91;
            T68 = FMA(KP414213562, T67, T62);
            T79 = FNMS(KP414213562, T78, T73);
            T80 = T68 - T79;
            T88 = T68 + T79;
            T90 = FMA(KP414213562, T73, T78);
            T91 = FNMS(KP414213562, T62, T67);
            T92 = T90 - T91;
            T94 = T91 + T90;
        }
        data[11].x = FNMS(KP923879532, T80, T57);
        data[11].y = FNMS(KP923879532, T92, T89);
        data[3].x = FMA(KP923879532, T80, T57);
        data[3].y = FMA(KP923879532, T92, T89);
        data[7].y = FNMS(KP923879532, T88, T87);
        data[7].x = FNMS(KP923879532, T94, T93);
        data[15].y = FMA(KP923879532, T88, T87);
        data[15].x = FMA(KP923879532, T94, T93);
    }
    {
        float T97;
        float T109;
        float T107;
        float T113;
        float T104;
        float T108;
        float T112;
        float T114;
        float T96;
        float T106;
        T96 = T85 + T84;
        T97 = FMA(KP707106781, T96, T95);
        T109 = FNMS(KP707106781, T96, T95);
        T106 = T52 + T55;
        T107 = FNMS(KP707106781, T106, T105);
        T113 = FMA(KP707106781, T106, T105);
        {
            float T100;
            float T103;
            float T110;
            float T111;
            T100 = FMA(KP414213562, T99, T98);
            T103 = FNMS(KP414213562, T102, T101);
            T104 = T100 + T103;
            T108 = T103 - T100;
            T110 = FNMS(KP414213562, T98, T99);
            T111 = FMA(KP414213562, T101, T102);
            T112 = T110 - T111;
            T114 = T110 + T111;
        }
        data[9].x = FNMS(KP923879532, T104, T97);
        data[9].y = FNMS(KP923879532, T114, T113);
        data[1].x = FMA(KP923879532, T104, T97);
        data[1].y = FMA(KP923879532, T114, T113);
        data[13].y = FNMS(KP923879532, T108, T107);
        data[13].x = FNMS(KP923879532, T112, T109);
        data[5].y = FMA(KP923879532, T108, T107);
        data[5].x = FMA(KP923879532, T112, T109);
    }
}
void idft16(inout vec2 data[16]) {

    DK(KP923879532, +0.923879532511286756128183189396788286822416626);
    DK(KP414213562, +0.414213562373095048801688724209698078569671875);
    DK(KP707106781, +0.707106781186547524400844362104849039284835938);

    float T7;
    float T115;
    float T129;
    float T38;
    float T49;
    float T95;
    float T105;
    float T83;
    float T29;
    float T126;
    float T141;
    float T73;
    float T78;
    float T99;
    float T123;
    float T98;
    float T14;
    float T116;
    float T130;
    float T45;
    float T52;
    float T84;
    float T85;
    float T55;
    float T22;
    float T121;
    float T140;
    float T62;
    float T67;
    float T102;
    float T118;
    float T101;
    {
        float T3;
        float T81;
        float T34;
        float T48;
        float T6;
        float T47;
        float T37;
        float T82;
        {
            float T1;
            float T2;
            float T32;
            float T33;
            T1 = data[0].x;
            T2 = data[8].x;
            T3 = T1 + T2;
            T81 = T1 - T2;
            T32 = data[0].y;
            T33 = data[8].y;
            T34 = T32 + T33;
            T48 = T32 - T33;
        }
        {
            float T4;
            float T5;
            float T35;
            float T36;
            T4 = data[4].x;
            T5 = data[12].x;
            T6 = T4 + T5;
            T47 = T4 - T5;
            T35 = data[4].y;
            T36 = data[12].y;
            T37 = T35 + T36;
            T82 = T35 - T36;
        }
        T7 = T3 + T6;
        T115 = T34 - T37;
        T129 = T3 - T6;
        T38 = T34 + T37;
        T49 = T47 + T48;
        T95 = T81 + T82;
        T105 = T48 - T47;
        T83 = T81 - T82;
    }
    {
        float T25;
        float T74;
        float T72;
        float T124;
        float T28;
        float T69;
        float T77;
        float T125;
        {
            float T23;
            float T24;
            float T70;
            float T71;
            T23 = data[15].x;
            T24 = data[7].x;
            T25 = T23 + T24;
            T74 = T23 - T24;
            T70 = data[15].y;
            T71 = data[7].y;
            T72 = T70 - T71;
            T124 = T70 + T71;
        }
        {
            float T26;
            float T27;
            float T75;
            float T76;
            T26 = data[3].x;
            T27 = data[11].x;
            T28 = T26 + T27;
            T69 = T26 - T27;
            T75 = data[3].y;
            T76 = data[11].y;
            T77 = T75 - T76;
            T125 = T75 + T76;
        }
        T29 = T25 + T28;
        T126 = T124 - T125;
        T141 = T124 + T125;
        T73 = T69 + T72;
        T78 = T74 - T77;
        T99 = T74 + T77;
        T123 = T25 - T28;
        T98 = T72 - T69;
    }
    {
        float T10;
        float T50;
        float T41;
        float T51;
        float T13;
        float T54;
        float T44;
        float T53;
        {
            float T8;
            float T9;
            float T39;
            float T40;
            T8 = data[2].x;
            T9 = data[10].x;
            T10 = T8 + T9;
            T50 = T8 - T9;
            T39 = data[2].y;
            T40 = data[10].y;
            T41 = T39 + T40;
            T51 = T39 - T40;
        }
        {
            float T11;
            float T12;
            float T42;
            float T43;
            T11 = data[14].x;
            T12 = data[6].x;
            T13 = T11 + T12;
            T54 = T11 - T12;
            T42 = data[14].y;
            T43 = data[6].y;
            T44 = T42 + T43;
            T53 = T42 - T43;
        }
        T14 = T10 + T13;
        T116 = T10 - T13;
        T130 = T44 - T41;
        T45 = T41 + T44;
        T52 = T50 + T51;
        T84 = T50 - T51;
        T85 = T54 + T53;
        T55 = T53 - T54;
    }
    {
        float T18;
        float T63;
        float T61;
        float T119;
        float T21;
        float T58;
        float T66;
        float T120;
        {
            float T16;
            float T17;
            float T59;
            float T60;
            T16 = data[1].x;
            T17 = data[9].x;
            T18 = T16 + T17;
            T63 = T16 - T17;
            T59 = data[1].y;
            T60 = data[9].y;
            T61 = T59 - T60;
            T119 = T59 + T60;
        }
        {
            float T19;
            float T20;
            float T64;
            float T65;
            T19 = data[5].x;
            T20 = data[13].x;
            T21 = T19 + T20;
            T58 = T19 - T20;
            T64 = data[5].y;
            T65 = data[13].y;
            T66 = T64 - T65;
            T120 = T64 + T65;
        }
        T22 = T18 + T21;
        T121 = T119 - T120;
        T140 = T119 + T120;
        T62 = T58 + T61;
        T67 = T63 - T66;
        T102 = T63 + T66;
        T118 = T18 - T21;
        T101 = T61 - T58;
    }
    
    {
        float T15;
        float T30;
        float T139;
        float T142;
        T15 = T7 + T14;
        T30 = T22 + T29;
        data[8].x = T15 - T30;
        data[0].x = T15 + T30;
        T139 = T38 + T45;
        T142 = T140 + T141;
        data[8].y = T139 - T142;
        data[0].y = T139 + T142;
    }
    {
        float T31;
        float T46;
        float T143;
        float T144;
        T31 = T22 - T29;
        T46 = T38 - T45;
        data[4].y = T31 + T46;
        data[12].y = T46 - T31;
        T143 = T7 - T14;
        T144 = T141 - T140;
        data[12].x = T143 - T144;
        data[4].x = T143 + T144;
    }
    {
        float T117;
        float T131;
        float T128;
        float T132;
        float T122;
        float T127;
        T117 = T115 - T116;
        T131 = T129 + T130;
        T122 = T118 - T121;
        T127 = T123 + T126;
        T128 = T122 - T127;
        T132 = T122 + T127;
        data[14].y = FNMS(KP707106781, T128, T117);
        data[2].x = FMA(KP707106781, T132, T131);
        data[6].y = FMA(KP707106781, T128, T117);
        data[10].x = FNMS(KP707106781, T132, T131);
    }
    {
        float T133;
        float T137;
        float T136;
        float T138;
        float T134;
        float T135;
        T133 = T116 + T115;
        T137 = T129 - T130;
        T134 = T118 + T121;
        T135 = T126 - T123;
        T136 = T134 + T135;
        T138 = T135 - T134;
        data[10].y = FNMS(KP707106781, T136, T133);
        data[6].x = FMA(KP707106781, T138, T137);
        data[2].y = FMA(KP707106781, T136, T133);
        data[14].x = FNMS(KP707106781, T138, T137);
    }
    {
        float T57;
        float T89;
        float T87;
        float T93;
        float T80;
        float T88;
        float T92;
        float T94;
        float T56;
        float T86;
        T56 = T52 + T55;
        T57 = FMA(KP707106781, T56, T49);
        T89 = FNMS(KP707106781, T56, T49);
        T86 = T84 + T85;
        T87 = FNMS(KP707106781, T86, T83);
        T93 = FMA(KP707106781, T86, T83);
        {
            float T68;
            float T79;
            float T90;
            float T91;
            T68 = FMA(KP414213562, T67, T62);
            T79 = FNMS(KP414213562, T78, T73);
            T80 = T68 + T79;
            T88 = T79 - T68;
            T90 = FNMS(KP414213562, T62, T67);
            T91 = FMA(KP414213562, T73, T78);
            T92 = T90 - T91;
            T94 = T90 + T91;
        }
        data[9].y = FNMS(KP923879532, T80, T57);
        data[9].x = FNMS(KP923879532, T94, T93);
        data[1].y = FMA(KP923879532, T80, T57);
        data[1].x = FMA(KP923879532, T94, T93);
        data[13].x = FNMS(KP923879532, T88, T87);
        data[13].y = FNMS(KP923879532, T92, T89);
        data[5].x = FMA(KP923879532, T88, T87);
        data[5].y = FMA(KP923879532, T92, T89);
    }
    {
        float T97;
        float T113;
        float T107;
        float T109;
        float T104;
        float T108;
        float T112;
        float T114;
        float T96;
        float T106;
        T96 = T55 - T52;
        T97 = FMA(KP707106781, T96, T95);
        T113 = FNMS(KP707106781, T96, T95);
        T106 = T84 - T85;
        T107 = FNMS(KP707106781, T106, T105);
        T109 = FMA(KP707106781, T106, T105);
        {
            float T100;
            float T103;
            float T110;
            float T111;
            T100 = FMA(KP414213562, T99, T98);
            T103 = FNMS(KP414213562, T102, T101);
            T104 = T100 - T103;
            T108 = T103 + T100;
            T110 = FMA(KP414213562, T101, T102);
            T111 = FNMS(KP414213562, T98, T99);
            T112 = T110 - T111;
            T114 = T110 + T111;
        }
        data[11].x = FNMS(KP923879532, T104, T97);
        data[11].y = FNMS(KP923879532, T112, T109);
        data[3].x = FMA(KP923879532, T104, T97);
        data[3].y = FMA(KP923879532, T112, T109);
        data[7].y = FNMS(KP923879532, T108, T107);
        data[7].x = FNMS(KP923879532, T114, T113);
        data[15].y = FMA(KP923879532, T108, T107);
        data[15].x = FMA(KP923879532, T114, T113);
    }
}
void irdft16(inout float data[18]) {

    DK(KP1_847759065, +1.847759065022573512256366378793576573644833252);
    DK(KP414213562, +0.414213562373095048801688724209698078569671875);
    DK(KP1_414213562, +1.414213562373095048801688724209698078569671875);
    DK(KP2_000000000, +2.000000000000000000000000000000000000000000000);

    float T5;
    float T47;
    float T19;
    float T39;
    float T8;
    float T48;
    float T24;
    float T40;
    float T12;
    float T51;
    float T15;
    float T52;
    float T30;
    float T35;
    float T53;
    float T50;
    float T43;
    float T42;
    {
        float T4;
        float T18;
        float T3;
        float T17;
        float T1;
        float T2;
        T4 = data[8];
        T18 = data[9];
        T1 = data[0];
        T2 = data[16];
        T3 = T1 + T2;
        T17 = T1 - T2;
        T5 = FMA(KP2_000000000, T4, T3);
        T47 = FNMS(KP2_000000000, T4, T3);
        T19 = FMA(KP2_000000000, T18, T17);
        T39 = FNMS(KP2_000000000, T18, T17);
    }
    {
        float T6;
        float T7;
        float T20;
        float T21;
        float T22;
        float T23;
        T6 = data[4];
        T7 = data[12];
        T20 = T6 - T7;
        T21 = data[5];
        T22 = data[13];
        T23 = T21 + T22;
        T8 = T6 + T7;
        T48 = T21 - T22;
        T24 = T20 + T23;
        T40 = T23 - T20;
    }
    {
        float T26;
        float T33;
        float T34;
        float T29;
        {
            float T10;
            float T11;
            float T31;
            float T32;
            T10 = data[2];
            T11 = data[14];
            T12 = T10 + T11;
            T26 = T10 - T11;
            T31 = data[3];
            T32 = data[15];
            T33 = T31 + T32;
            T51 = T31 - T32;
        }
        {
            float T13;
            float T14;
            float T27;
            float T28;
            T13 = data[10];
            T14 = data[6];
            T15 = T13 + T14;
            T34 = T13 - T14;
            T27 = data[11];
            T28 = data[7];
            T29 = T27 + T28;
            T52 = T27 - T28;
        }
        T30 = T26 + T29;
        T35 = T33 - T34;
        T53 = T51 - T52;
        T50 = T12 - T15;
        T43 = T34 + T33;
        T42 = T26 - T29;
    }
    {
        float T9;
        float T16;
        float T55;
        float T56;
        T9 = FMA(KP2_000000000, T8, T5);
        T16 = T12 + T15;
        data[8] = FNMS(KP2_000000000, T16, T9);
        data[0] = FMA(KP2_000000000, T16, T9);
        T55 = FNMS(KP2_000000000, T48, T47);
        T56 = T53 - T50;
        data[14] = FNMS(KP1_414213562, T56, T55);
        data[6] = FMA(KP1_414213562, T56, T55);
    }
    {
        float T57;
        float T58;
        float T25;
        float T36;
        T57 = FNMS(KP2_000000000, T8, T5);
        T58 = T52 + T51;
        data[12] = FNMS(KP2_000000000, T58, T57);
        data[4] = FMA(KP2_000000000, T58, T57);
        T25 = FMA(KP1_414213562, T24, T19);
        T36 = FMA(KP414213562, T35, T30);
        data[9] = FNMS(KP1_847759065, T36, T25);
        data[1] = FMA(KP1_847759065, T36, T25);
    }
    {
        float T37;
        float T38;
        float T45;
        float T46;
        T37 = FNMS(KP1_414213562, T24, T19);
        T38 = FNMS(KP414213562, T30, T35);
        data[13] = FNMS(KP1_847759065, T38, T37);
        data[5] = FMA(KP1_847759065, T38, T37);
        T45 = FMA(KP1_414213562, T40, T39);
        T46 = FMA(KP414213562, T42, T43);
        data[11] = FNMS(KP1_847759065, T46, T45);
        data[3] = FMA(KP1_847759065, T46, T45);
    }
    {
        float T49;
        float T54;
        float T41;
        float T44;
        T49 = FMA(KP2_000000000, T48, T47);
        T54 = T50 + T53;
        data[10] = FNMS(KP1_414213562, T54, T49);
        data[2] = FMA(KP1_414213562, T54, T49);
        T41 = FNMS(KP1_414213562, T40, T39);
        T44 = FNMS(KP414213562, T43, T42);
        data[7] = FNMS(KP1_847759065, T44, T41);
        data[15] = FMA(KP1_847759065, T44, T41);
    }
}
#if T == 1
void dftTemporal(inout vec2 data[1]) {
}
#endif
#if T == 3
void dftTemporal(inout vec2 data[3]) {

    DK(KP866025403, +0.866025403784438646763723170752936183471402627);
    DK(KP500000000, +0.500000000000000000000000000000000000000000000);

    float T1;
    float T9;
    float T4;
    float T12;
    float T8;
    float T10;
    float T5;
    float T11;
    T1 = data[0].x;
    T9 = data[0].y;
    {
        float T2;
        float T3;
        float T6;
        float T7;
        T2 = data[1].x;
        T3 = data[2].x;
        T4 = T2 + T3;
        T12 = T3 - T2;
        T6 = data[1].y;
        T7 = data[2].y;
        T8 = T6 - T7;
        T10 = T6 + T7;
    }
    data[0].x = T1 + T4;
    data[0].y = T9 + T10;
    T5 = FNMS(KP500000000, T4, T1);
    data[2].x = FNMS(KP866025403, T8, T5);
    data[1].x = FMA(KP866025403, T8, T5);
    T11 = FNMS(KP500000000, T10, T9);
    data[1].y = FMA(KP866025403, T12, T11);
    data[2].y = FNMS(KP866025403, T12, T11);
}
#endif
#if T == 5
void dftTemporal(inout vec2 data[5]) {

    DK(KP951056516, +0.951056516295153572116439333379382143405698634);
    DK(KP559016994, +0.559016994374947424102293417182819058860154590);
    DK(KP250000000, +0.250000000000000000000000000000000000000000000);
    DK(KP618033988, +0.618033988749894848204586834365638117720309180);

    float T1;
    float T21;
    float T8;
    float T29;
    float T10;
    float T28;
    float T14;
    float T26;
    float T17;
    float T24;
    T1 = data[0].x;
    T21 = data[0].y;
    {
        float T2;
        float T3;
        float T4;
        float T5;
        float T6;
        float T7;
        T2 = data[1].x;
        T3 = data[4].x;
        T4 = T2 + T3;
        T5 = data[2].x;
        T6 = data[3].x;
        T7 = T5 + T6;
        T8 = T4 + T7;
        T29 = T5 - T6;
        T10 = T4 - T7;
        T28 = T2 - T3;
    }
    {
        float T12;
        float T13;
        float T22;
        float T15;
        float T16;
        float T23;
        T12 = data[1].y;
        T13 = data[4].y;
        T22 = T12 + T13;
        T15 = data[2].y;
        T16 = data[3].y;
        T23 = T15 + T16;
        T14 = T12 - T13;
        T26 = T22 - T23;
        T17 = T15 - T16;
        T24 = T22 + T23;
    }
    data[0].x = T1 + T8;
    data[0].y = T21 + T24;
    {
        float T18;
        float T20;
        float T11;
        float T19;
        float T9;
        T18 = FMA(KP618033988, T17, T14);
        T20 = FNMS(KP618033988, T14, T17);
        T9 = FNMS(KP250000000, T8, T1);
        T11 = FMA(KP559016994, T10, T9);
        T19 = FNMS(KP559016994, T10, T9);
        data[4].x = FNMS(KP951056516, T18, T11);
        data[3].x = FMA(KP951056516, T20, T19);
        data[1].x = FMA(KP951056516, T18, T11);
        data[2].x = FNMS(KP951056516, T20, T19);
    }
    {
        float T30;
        float T32;
        float T27;
        float T31;
        float T25;
        T30 = FMA(KP618033988, T29, T28);
        T32 = FNMS(KP618033988, T28, T29);
        T25 = FNMS(KP250000000, T24, T21);
        T27 = FMA(KP559016994, T26, T25);
        T31 = FNMS(KP559016994, T26, T25);
        data[1].y = FNMS(KP951056516, T30, T27);
        data[3].y = FNMS(KP951056516, T32, T31);
        data[4].y = FMA(KP951056516, T30, T27);
        data[2].y = FMA(KP951056516, T32, T31);
    }
}
#endif
#if T == 7
void dftTemporal(inout vec2 data[7]) {

    DK(KP974927912, +0.974927912181823607018131682993931217232785801);
    DK(KP900968867, +0.900968867902419126236102319507445051165919162);
    DK(KP692021471, +0.692021471630095869627814897002069140197260599);
    DK(KP801937735, +0.801937735804838252472204639014890102331838324);
    DK(KP554958132, +0.554958132087371191422194871006410481067288862);
    DK(KP356895867, +0.356895867892209443894399510021300583399127187);

    float T1;
    float T35;
    float T4;
    float T44;
    float T10;
    float T42;
    float T7;
    float T43;
    float T11;
    float T25;
    float T55;
    float T50;
    float T45;
    float T30;
    float T16;
    float T37;
    float T22;
    float T38;
    float T19;
    float T36;
    float T23;
    float T28;
    float T52;
    float T47;
    float T39;
    float T33;
    T1 = data[0].x;
    T35 = data[0].y;
    {
        float T2;
        float T3;
        float T14;
        float T15;
        T2 = data[1].x;
        T3 = data[6].x;
        T4 = T2 + T3;
        T44 = T3 - T2;
        {
            float T8;
            float T9;
            float T5;
            float T6;
            T8 = data[3].x;
            T9 = data[4].x;
            T10 = T8 + T9;
            T42 = T9 - T8;
            T5 = data[2].x;
            T6 = data[5].x;
            T7 = T5 + T6;
            T43 = T6 - T5;
        }
        T11 = FNMS(KP356895867, T7, T4);
        T25 = FNMS(KP356895867, T4, T10);
        T55 = FMA(KP554958132, T42, T44);
        T50 = FMA(KP554958132, T43, T42);
        T45 = FNMS(KP554958132, T44, T43);
        T30 = FNMS(KP356895867, T10, T7);
        T14 = data[2].y;
        T15 = data[5].y;
        T16 = T14 - T15;
        T37 = T14 + T15;
        {
            float T20;
            float T21;
            float T17;
            float T18;
            T20 = data[3].y;
            T21 = data[4].y;
            T22 = T20 - T21;
            T38 = T20 + T21;
            T17 = data[1].y;
            T18 = data[6].y;
            T19 = T17 - T18;
            T36 = T17 + T18;
        }
        T23 = FMA(KP554958132, T22, T19);
        T28 = FMA(KP554958132, T16, T22);
        T52 = FNMS(KP356895867, T37, T36);
        T47 = FNMS(KP356895867, T36, T38);
        T39 = FNMS(KP356895867, T38, T37);
        T33 = FNMS(KP554958132, T19, T16);
    }
    data[0].x = T1 + T4 + T7 + T10;
    data[0].y = T35 + T36 + T37 + T38;
    {
        float T24;
        float T13;
        float T12;
        float T56;
        float T54;
        float T53;
        T24 = FMA(KP801937735, T23, T16);
        T12 = FNMS(KP692021471, T11, T10);
        T13 = FNMS(KP900968867, T12, T1);
        data[6].x = FNMS(KP974927912, T24, T13);
        data[1].x = FMA(KP974927912, T24, T13);
        T56 = FMA(KP801937735, T55, T43);
        T53 = FNMS(KP692021471, T52, T38);
        T54 = FNMS(KP900968867, T53, T35);
        data[1].y = FMA(KP974927912, T56, T54);
        data[6].y = FNMS(KP974927912, T56, T54);
    }
    {
        float T29;
        float T27;
        float T26;
        float T51;
        float T49;
        float T48;
        T29 = FNMS(KP801937735, T28, T19);
        T26 = FNMS(KP692021471, T25, T7);
        T27 = FNMS(KP900968867, T26, T1);
        data[5].x = FNMS(KP974927912, T29, T27);
        data[2].x = FMA(KP974927912, T29, T27);
        T51 = FNMS(KP801937735, T50, T44);
        T48 = FNMS(KP692021471, T47, T37);
        T49 = FNMS(KP900968867, T48, T35);
        data[2].y = FMA(KP974927912, T51, T49);
        data[5].y = FNMS(KP974927912, T51, T49);
    }
    {
        float T34;
        float T32;
        float T31;
        float T46;
        float T41;
        float T40;
        T34 = FNMS(KP801937735, T33, T22);
        T31 = FNMS(KP692021471, T30, T4);
        T32 = FNMS(KP900968867, T31, T1);
        data[4].x = FNMS(KP974927912, T34, T32);
        data[3].x = FMA(KP974927912, T34, T32);
        T46 = FNMS(KP801937735, T45, T42);
        T40 = FNMS(KP692021471, T39, T36);
        T41 = FNMS(KP900968867, T40, T35);
        data[3].y = FMA(KP974927912, T46, T41);
        data[4].y = FNMS(KP974927912, T46, T41);
    }
}
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

layout(set=0,binding=9,std430) writeonly buffer CachedDestination { float cachedDestination[]; };
#if STATIC_MAIN
#if T == 1
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
    vec2 spectrum[N];


        int x = reflectCoordinate(originX + lane, pc.width);
        {
{

            float v = validBlock ? sampleFrame(0, x, reflectCoordinate(originY+0, pc.height)) : 0.0;
#if REAL_SHARED
            transposeStoreReal(sharedBase + 0 * (N + 1) + lane,
                v * analysisWindow((T/2)*N+0,lane));
#else
            transposeStore(sharedBase + 0 * (N + 1) + lane,
                vec2(v * analysisWindow((T/2)*N+0,lane), 0.0));
#endif
        
}
{

            float v = validBlock ? sampleFrame(0, x, reflectCoordinate(originY+1, pc.height)) : 0.0;
#if REAL_SHARED
            transposeStoreReal(sharedBase + 1 * (N + 1) + lane,
                v * analysisWindow((T/2)*N+1,lane));
#else
            transposeStore(sharedBase + 1 * (N + 1) + lane,
                vec2(v * analysisWindow((T/2)*N+1,lane), 0.0));
#endif
        
}
{

            float v = validBlock ? sampleFrame(0, x, reflectCoordinate(originY+2, pc.height)) : 0.0;
#if REAL_SHARED
            transposeStoreReal(sharedBase + 2 * (N + 1) + lane,
                v * analysisWindow((T/2)*N+2,lane));
#else
            transposeStore(sharedBase + 2 * (N + 1) + lane,
                vec2(v * analysisWindow((T/2)*N+2,lane), 0.0));
#endif
        
}
{

            float v = validBlock ? sampleFrame(0, x, reflectCoordinate(originY+3, pc.height)) : 0.0;
#if REAL_SHARED
            transposeStoreReal(sharedBase + 3 * (N + 1) + lane,
                v * analysisWindow((T/2)*N+3,lane));
#else
            transposeStore(sharedBase + 3 * (N + 1) + lane,
                vec2(v * analysisWindow((T/2)*N+3,lane), 0.0));
#endif
        
}
{

            float v = validBlock ? sampleFrame(0, x, reflectCoordinate(originY+4, pc.height)) : 0.0;
#if REAL_SHARED
            transposeStoreReal(sharedBase + 4 * (N + 1) + lane,
                v * analysisWindow((T/2)*N+4,lane));
#else
            transposeStore(sharedBase + 4 * (N + 1) + lane,
                vec2(v * analysisWindow((T/2)*N+4,lane), 0.0));
#endif
        
}
{

            float v = validBlock ? sampleFrame(0, x, reflectCoordinate(originY+5, pc.height)) : 0.0;
#if REAL_SHARED
            transposeStoreReal(sharedBase + 5 * (N + 1) + lane,
                v * analysisWindow((T/2)*N+5,lane));
#else
            transposeStore(sharedBase + 5 * (N + 1) + lane,
                vec2(v * analysisWindow((T/2)*N+5,lane), 0.0));
#endif
        
}
{

            float v = validBlock ? sampleFrame(0, x, reflectCoordinate(originY+6, pc.height)) : 0.0;
#if REAL_SHARED
            transposeStoreReal(sharedBase + 6 * (N + 1) + lane,
                v * analysisWindow((T/2)*N+6,lane));
#else
            transposeStore(sharedBase + 6 * (N + 1) + lane,
                vec2(v * analysisWindow((T/2)*N+6,lane), 0.0));
#endif
        
}
{

            float v = validBlock ? sampleFrame(0, x, reflectCoordinate(originY+7, pc.height)) : 0.0;
#if REAL_SHARED
            transposeStoreReal(sharedBase + 7 * (N + 1) + lane,
                v * analysisWindow((T/2)*N+7,lane));
#else
            transposeStore(sharedBase + 7 * (N + 1) + lane,
                vec2(v * analysisWindow((T/2)*N+7,lane), 0.0));
#endif
        
}
{

            float v = validBlock ? sampleFrame(0, x, reflectCoordinate(originY+8, pc.height)) : 0.0;
#if REAL_SHARED
            transposeStoreReal(sharedBase + 8 * (N + 1) + lane,
                v * analysisWindow((T/2)*N+8,lane));
#else
            transposeStore(sharedBase + 8 * (N + 1) + lane,
                vec2(v * analysisWindow((T/2)*N+8,lane), 0.0));
#endif
        
}
{

            float v = validBlock ? sampleFrame(0, x, reflectCoordinate(originY+9, pc.height)) : 0.0;
#if REAL_SHARED
            transposeStoreReal(sharedBase + 9 * (N + 1) + lane,
                v * analysisWindow((T/2)*N+9,lane));
#else
            transposeStore(sharedBase + 9 * (N + 1) + lane,
                vec2(v * analysisWindow((T/2)*N+9,lane), 0.0));
#endif
        
}
{

            float v = validBlock ? sampleFrame(0, x, reflectCoordinate(originY+10, pc.height)) : 0.0;
#if REAL_SHARED
            transposeStoreReal(sharedBase + 10 * (N + 1) + lane,
                v * analysisWindow((T/2)*N+10,lane));
#else
            transposeStore(sharedBase + 10 * (N + 1) + lane,
                vec2(v * analysisWindow((T/2)*N+10,lane), 0.0));
#endif
        
}
{

            float v = validBlock ? sampleFrame(0, x, reflectCoordinate(originY+11, pc.height)) : 0.0;
#if REAL_SHARED
            transposeStoreReal(sharedBase + 11 * (N + 1) + lane,
                v * analysisWindow((T/2)*N+11,lane));
#else
            transposeStore(sharedBase + 11 * (N + 1) + lane,
                vec2(v * analysisWindow((T/2)*N+11,lane), 0.0));
#endif
        
}
{

            float v = validBlock ? sampleFrame(0, x, reflectCoordinate(originY+12, pc.height)) : 0.0;
#if REAL_SHARED
            transposeStoreReal(sharedBase + 12 * (N + 1) + lane,
                v * analysisWindow((T/2)*N+12,lane));
#else
            transposeStore(sharedBase + 12 * (N + 1) + lane,
                vec2(v * analysisWindow((T/2)*N+12,lane), 0.0));
#endif
        
}
{

            float v = validBlock ? sampleFrame(0, x, reflectCoordinate(originY+13, pc.height)) : 0.0;
#if REAL_SHARED
            transposeStoreReal(sharedBase + 13 * (N + 1) + lane,
                v * analysisWindow((T/2)*N+13,lane));
#else
            transposeStore(sharedBase + 13 * (N + 1) + lane,
                vec2(v * analysisWindow((T/2)*N+13,lane), 0.0));
#endif
        
}
{

            float v = validBlock ? sampleFrame(0, x, reflectCoordinate(originY+14, pc.height)) : 0.0;
#if REAL_SHARED
            transposeStoreReal(sharedBase + 14 * (N + 1) + lane,
                v * analysisWindow((T/2)*N+14,lane));
#else
            transposeStore(sharedBase + 14 * (N + 1) + lane,
                vec2(v * analysisWindow((T/2)*N+14,lane), 0.0));
#endif
        
}
{

            float v = validBlock ? sampleFrame(0, x, reflectCoordinate(originY+15, pc.height)) : 0.0;
#if REAL_SHARED
            transposeStoreReal(sharedBase + 15 * (N + 1) + lane,
                v * analysisWindow((T/2)*N+15,lane));
#else
            transposeStore(sharedBase + 15 * (N + 1) + lane,
                vec2(v * analysisWindow((T/2)*N+15,lane), 0.0));
#endif
        
}
}
        TILE_BARRIER();
        float realData[N + 2];
        {
{
realData[0] = transposeLoadReal(sharedBase + lane * (N + 1) + 0);
}
{
realData[1] = transposeLoadReal(sharedBase + lane * (N + 1) + 1);
}
{
realData[2] = transposeLoadReal(sharedBase + lane * (N + 1) + 2);
}
{
realData[3] = transposeLoadReal(sharedBase + lane * (N + 1) + 3);
}
{
realData[4] = transposeLoadReal(sharedBase + lane * (N + 1) + 4);
}
{
realData[5] = transposeLoadReal(sharedBase + lane * (N + 1) + 5);
}
{
realData[6] = transposeLoadReal(sharedBase + lane * (N + 1) + 6);
}
{
realData[7] = transposeLoadReal(sharedBase + lane * (N + 1) + 7);
}
{
realData[8] = transposeLoadReal(sharedBase + lane * (N + 1) + 8);
}
{
realData[9] = transposeLoadReal(sharedBase + lane * (N + 1) + 9);
}
{
realData[10] = transposeLoadReal(sharedBase + lane * (N + 1) + 10);
}
{
realData[11] = transposeLoadReal(sharedBase + lane * (N + 1) + 11);
}
{
realData[12] = transposeLoadReal(sharedBase + lane * (N + 1) + 12);
}
{
realData[13] = transposeLoadReal(sharedBase + lane * (N + 1) + 13);
}
{
realData[14] = transposeLoadReal(sharedBase + lane * (N + 1) + 14);
}
{
realData[15] = transposeLoadReal(sharedBase + lane * (N + 1) + 15);
}
}
        TILE_BARRIER();
        rdft16(realData);
        {
{
transposeStore(sharedBase + lane * (N + 1) + 0,vec2(realData[2*0], realData[2*0+1]));
}
{
transposeStore(sharedBase + lane * (N + 1) + 1,vec2(realData[2*1], realData[2*1+1]));
}
{
transposeStore(sharedBase + lane * (N + 1) + 2,vec2(realData[2*2], realData[2*2+1]));
}
{
transposeStore(sharedBase + lane * (N + 1) + 3,vec2(realData[2*3], realData[2*3+1]));
}
{
transposeStore(sharedBase + lane * (N + 1) + 4,vec2(realData[2*4], realData[2*4+1]));
}
{
transposeStore(sharedBase + lane * (N + 1) + 5,vec2(realData[2*5], realData[2*5+1]));
}
{
transposeStore(sharedBase + lane * (N + 1) + 6,vec2(realData[2*6], realData[2*6+1]));
}
{
transposeStore(sharedBase + lane * (N + 1) + 7,vec2(realData[2*7], realData[2*7+1]));
}
{
transposeStore(sharedBase + lane * (N + 1) + 8,vec2(realData[2*8], realData[2*8+1]));
}
}
        TILE_BARRIER();
        if (lane <= N/2) {
            vec2 column[N];
            {
{
column[0] = transposeLoad(sharedBase + 0 * (N + 1) + lane);
}
{
column[1] = transposeLoad(sharedBase + 1 * (N + 1) + lane);
}
{
column[2] = transposeLoad(sharedBase + 2 * (N + 1) + lane);
}
{
column[3] = transposeLoad(sharedBase + 3 * (N + 1) + lane);
}
{
column[4] = transposeLoad(sharedBase + 4 * (N + 1) + lane);
}
{
column[5] = transposeLoad(sharedBase + 5 * (N + 1) + lane);
}
{
column[6] = transposeLoad(sharedBase + 6 * (N + 1) + lane);
}
{
column[7] = transposeLoad(sharedBase + 7 * (N + 1) + lane);
}
{
column[8] = transposeLoad(sharedBase + 8 * (N + 1) + lane);
}
{
column[9] = transposeLoad(sharedBase + 9 * (N + 1) + lane);
}
{
column[10] = transposeLoad(sharedBase + 10 * (N + 1) + lane);
}
{
column[11] = transposeLoad(sharedBase + 11 * (N + 1) + lane);
}
{
column[12] = transposeLoad(sharedBase + 12 * (N + 1) + lane);
}
{
column[13] = transposeLoad(sharedBase + 13 * (N + 1) + lane);
}
{
column[14] = transposeLoad(sharedBase + 14 * (N + 1) + lane);
}
{
column[15] = transposeLoad(sharedBase + 15 * (N + 1) + lane);
}
}
            dft16(column);
            {
{
spectrum[0] = column[0];
}
{
spectrum[1] = column[1];
}
{
spectrum[2] = column[2];
}
{
spectrum[3] = column[3];
}
{
spectrum[4] = column[4];
}
{
spectrum[5] = column[5];
}
{
spectrum[6] = column[6];
}
{
spectrum[7] = column[7];
}
{
spectrum[8] = column[8];
}
{
spectrum[9] = column[9];
}
{
spectrum[10] = column[10];
}
{
spectrum[11] = column[11];
}
{
spectrum[12] = column[12];
}
{
spectrum[13] = column[13];
}
{
spectrum[14] = column[14];
}
{
spectrum[15] = column[15];
}
}
        }
        TILE_BARRIER();
    
    if (validBlock && lane <= N/2) {
        {
{

            int destination = blockId*pc.dstStride + 2*(0*(N/2+1)+lane);
            cachedDestination[destination]=spectrum[0].x;
            cachedDestination[destination+1]=spectrum[0].y;
        
}
{

            int destination = blockId*pc.dstStride + 2*(1*(N/2+1)+lane);
            cachedDestination[destination]=spectrum[1].x;
            cachedDestination[destination+1]=spectrum[1].y;
        
}
{

            int destination = blockId*pc.dstStride + 2*(2*(N/2+1)+lane);
            cachedDestination[destination]=spectrum[2].x;
            cachedDestination[destination+1]=spectrum[2].y;
        
}
{

            int destination = blockId*pc.dstStride + 2*(3*(N/2+1)+lane);
            cachedDestination[destination]=spectrum[3].x;
            cachedDestination[destination+1]=spectrum[3].y;
        
}
{

            int destination = blockId*pc.dstStride + 2*(4*(N/2+1)+lane);
            cachedDestination[destination]=spectrum[4].x;
            cachedDestination[destination+1]=spectrum[4].y;
        
}
{

            int destination = blockId*pc.dstStride + 2*(5*(N/2+1)+lane);
            cachedDestination[destination]=spectrum[5].x;
            cachedDestination[destination+1]=spectrum[5].y;
        
}
{

            int destination = blockId*pc.dstStride + 2*(6*(N/2+1)+lane);
            cachedDestination[destination]=spectrum[6].x;
            cachedDestination[destination+1]=spectrum[6].y;
        
}
{

            int destination = blockId*pc.dstStride + 2*(7*(N/2+1)+lane);
            cachedDestination[destination]=spectrum[7].x;
            cachedDestination[destination+1]=spectrum[7].y;
        
}
{

            int destination = blockId*pc.dstStride + 2*(8*(N/2+1)+lane);
            cachedDestination[destination]=spectrum[8].x;
            cachedDestination[destination+1]=spectrum[8].y;
        
}
{

            int destination = blockId*pc.dstStride + 2*(9*(N/2+1)+lane);
            cachedDestination[destination]=spectrum[9].x;
            cachedDestination[destination+1]=spectrum[9].y;
        
}
{

            int destination = blockId*pc.dstStride + 2*(10*(N/2+1)+lane);
            cachedDestination[destination]=spectrum[10].x;
            cachedDestination[destination+1]=spectrum[10].y;
        
}
{

            int destination = blockId*pc.dstStride + 2*(11*(N/2+1)+lane);
            cachedDestination[destination]=spectrum[11].x;
            cachedDestination[destination+1]=spectrum[11].y;
        
}
{

            int destination = blockId*pc.dstStride + 2*(12*(N/2+1)+lane);
            cachedDestination[destination]=spectrum[12].x;
            cachedDestination[destination+1]=spectrum[12].y;
        
}
{

            int destination = blockId*pc.dstStride + 2*(13*(N/2+1)+lane);
            cachedDestination[destination]=spectrum[13].x;
            cachedDestination[destination+1]=spectrum[13].y;
        
}
{

            int destination = blockId*pc.dstStride + 2*(14*(N/2+1)+lane);
            cachedDestination[destination]=spectrum[14].x;
            cachedDestination[destination+1]=spectrum[14].y;
        
}
{

            int destination = blockId*pc.dstStride + 2*(15*(N/2+1)+lane);
            cachedDestination[destination]=spectrum[15].x;
            cachedDestination[destination+1]=spectrum[15].y;
        
}
}
    }
}
#endif
#if T == 3
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
    vec2 spectrum[N];


        int x = reflectCoordinate(originX + lane, pc.width);
        {
{

            float v = validBlock ? sampleFrame(0, x, reflectCoordinate(originY+0, pc.height)) : 0.0;
#if REAL_SHARED
            transposeStoreReal(sharedBase + 0 * (N + 1) + lane,
                v * analysisWindow((T/2)*N+0,lane));
#else
            transposeStore(sharedBase + 0 * (N + 1) + lane,
                vec2(v * analysisWindow((T/2)*N+0,lane), 0.0));
#endif
        
}
{

            float v = validBlock ? sampleFrame(0, x, reflectCoordinate(originY+1, pc.height)) : 0.0;
#if REAL_SHARED
            transposeStoreReal(sharedBase + 1 * (N + 1) + lane,
                v * analysisWindow((T/2)*N+1,lane));
#else
            transposeStore(sharedBase + 1 * (N + 1) + lane,
                vec2(v * analysisWindow((T/2)*N+1,lane), 0.0));
#endif
        
}
{

            float v = validBlock ? sampleFrame(0, x, reflectCoordinate(originY+2, pc.height)) : 0.0;
#if REAL_SHARED
            transposeStoreReal(sharedBase + 2 * (N + 1) + lane,
                v * analysisWindow((T/2)*N+2,lane));
#else
            transposeStore(sharedBase + 2 * (N + 1) + lane,
                vec2(v * analysisWindow((T/2)*N+2,lane), 0.0));
#endif
        
}
{

            float v = validBlock ? sampleFrame(0, x, reflectCoordinate(originY+3, pc.height)) : 0.0;
#if REAL_SHARED
            transposeStoreReal(sharedBase + 3 * (N + 1) + lane,
                v * analysisWindow((T/2)*N+3,lane));
#else
            transposeStore(sharedBase + 3 * (N + 1) + lane,
                vec2(v * analysisWindow((T/2)*N+3,lane), 0.0));
#endif
        
}
{

            float v = validBlock ? sampleFrame(0, x, reflectCoordinate(originY+4, pc.height)) : 0.0;
#if REAL_SHARED
            transposeStoreReal(sharedBase + 4 * (N + 1) + lane,
                v * analysisWindow((T/2)*N+4,lane));
#else
            transposeStore(sharedBase + 4 * (N + 1) + lane,
                vec2(v * analysisWindow((T/2)*N+4,lane), 0.0));
#endif
        
}
{

            float v = validBlock ? sampleFrame(0, x, reflectCoordinate(originY+5, pc.height)) : 0.0;
#if REAL_SHARED
            transposeStoreReal(sharedBase + 5 * (N + 1) + lane,
                v * analysisWindow((T/2)*N+5,lane));
#else
            transposeStore(sharedBase + 5 * (N + 1) + lane,
                vec2(v * analysisWindow((T/2)*N+5,lane), 0.0));
#endif
        
}
{

            float v = validBlock ? sampleFrame(0, x, reflectCoordinate(originY+6, pc.height)) : 0.0;
#if REAL_SHARED
            transposeStoreReal(sharedBase + 6 * (N + 1) + lane,
                v * analysisWindow((T/2)*N+6,lane));
#else
            transposeStore(sharedBase + 6 * (N + 1) + lane,
                vec2(v * analysisWindow((T/2)*N+6,lane), 0.0));
#endif
        
}
{

            float v = validBlock ? sampleFrame(0, x, reflectCoordinate(originY+7, pc.height)) : 0.0;
#if REAL_SHARED
            transposeStoreReal(sharedBase + 7 * (N + 1) + lane,
                v * analysisWindow((T/2)*N+7,lane));
#else
            transposeStore(sharedBase + 7 * (N + 1) + lane,
                vec2(v * analysisWindow((T/2)*N+7,lane), 0.0));
#endif
        
}
{

            float v = validBlock ? sampleFrame(0, x, reflectCoordinate(originY+8, pc.height)) : 0.0;
#if REAL_SHARED
            transposeStoreReal(sharedBase + 8 * (N + 1) + lane,
                v * analysisWindow((T/2)*N+8,lane));
#else
            transposeStore(sharedBase + 8 * (N + 1) + lane,
                vec2(v * analysisWindow((T/2)*N+8,lane), 0.0));
#endif
        
}
{

            float v = validBlock ? sampleFrame(0, x, reflectCoordinate(originY+9, pc.height)) : 0.0;
#if REAL_SHARED
            transposeStoreReal(sharedBase + 9 * (N + 1) + lane,
                v * analysisWindow((T/2)*N+9,lane));
#else
            transposeStore(sharedBase + 9 * (N + 1) + lane,
                vec2(v * analysisWindow((T/2)*N+9,lane), 0.0));
#endif
        
}
{

            float v = validBlock ? sampleFrame(0, x, reflectCoordinate(originY+10, pc.height)) : 0.0;
#if REAL_SHARED
            transposeStoreReal(sharedBase + 10 * (N + 1) + lane,
                v * analysisWindow((T/2)*N+10,lane));
#else
            transposeStore(sharedBase + 10 * (N + 1) + lane,
                vec2(v * analysisWindow((T/2)*N+10,lane), 0.0));
#endif
        
}
{

            float v = validBlock ? sampleFrame(0, x, reflectCoordinate(originY+11, pc.height)) : 0.0;
#if REAL_SHARED
            transposeStoreReal(sharedBase + 11 * (N + 1) + lane,
                v * analysisWindow((T/2)*N+11,lane));
#else
            transposeStore(sharedBase + 11 * (N + 1) + lane,
                vec2(v * analysisWindow((T/2)*N+11,lane), 0.0));
#endif
        
}
{

            float v = validBlock ? sampleFrame(0, x, reflectCoordinate(originY+12, pc.height)) : 0.0;
#if REAL_SHARED
            transposeStoreReal(sharedBase + 12 * (N + 1) + lane,
                v * analysisWindow((T/2)*N+12,lane));
#else
            transposeStore(sharedBase + 12 * (N + 1) + lane,
                vec2(v * analysisWindow((T/2)*N+12,lane), 0.0));
#endif
        
}
{

            float v = validBlock ? sampleFrame(0, x, reflectCoordinate(originY+13, pc.height)) : 0.0;
#if REAL_SHARED
            transposeStoreReal(sharedBase + 13 * (N + 1) + lane,
                v * analysisWindow((T/2)*N+13,lane));
#else
            transposeStore(sharedBase + 13 * (N + 1) + lane,
                vec2(v * analysisWindow((T/2)*N+13,lane), 0.0));
#endif
        
}
{

            float v = validBlock ? sampleFrame(0, x, reflectCoordinate(originY+14, pc.height)) : 0.0;
#if REAL_SHARED
            transposeStoreReal(sharedBase + 14 * (N + 1) + lane,
                v * analysisWindow((T/2)*N+14,lane));
#else
            transposeStore(sharedBase + 14 * (N + 1) + lane,
                vec2(v * analysisWindow((T/2)*N+14,lane), 0.0));
#endif
        
}
{

            float v = validBlock ? sampleFrame(0, x, reflectCoordinate(originY+15, pc.height)) : 0.0;
#if REAL_SHARED
            transposeStoreReal(sharedBase + 15 * (N + 1) + lane,
                v * analysisWindow((T/2)*N+15,lane));
#else
            transposeStore(sharedBase + 15 * (N + 1) + lane,
                vec2(v * analysisWindow((T/2)*N+15,lane), 0.0));
#endif
        
}
}
        TILE_BARRIER();
        float realData[N + 2];
        {
{
realData[0] = transposeLoadReal(sharedBase + lane * (N + 1) + 0);
}
{
realData[1] = transposeLoadReal(sharedBase + lane * (N + 1) + 1);
}
{
realData[2] = transposeLoadReal(sharedBase + lane * (N + 1) + 2);
}
{
realData[3] = transposeLoadReal(sharedBase + lane * (N + 1) + 3);
}
{
realData[4] = transposeLoadReal(sharedBase + lane * (N + 1) + 4);
}
{
realData[5] = transposeLoadReal(sharedBase + lane * (N + 1) + 5);
}
{
realData[6] = transposeLoadReal(sharedBase + lane * (N + 1) + 6);
}
{
realData[7] = transposeLoadReal(sharedBase + lane * (N + 1) + 7);
}
{
realData[8] = transposeLoadReal(sharedBase + lane * (N + 1) + 8);
}
{
realData[9] = transposeLoadReal(sharedBase + lane * (N + 1) + 9);
}
{
realData[10] = transposeLoadReal(sharedBase + lane * (N + 1) + 10);
}
{
realData[11] = transposeLoadReal(sharedBase + lane * (N + 1) + 11);
}
{
realData[12] = transposeLoadReal(sharedBase + lane * (N + 1) + 12);
}
{
realData[13] = transposeLoadReal(sharedBase + lane * (N + 1) + 13);
}
{
realData[14] = transposeLoadReal(sharedBase + lane * (N + 1) + 14);
}
{
realData[15] = transposeLoadReal(sharedBase + lane * (N + 1) + 15);
}
}
        TILE_BARRIER();
        rdft16(realData);
        {
{
transposeStore(sharedBase + lane * (N + 1) + 0,vec2(realData[2*0], realData[2*0+1]));
}
{
transposeStore(sharedBase + lane * (N + 1) + 1,vec2(realData[2*1], realData[2*1+1]));
}
{
transposeStore(sharedBase + lane * (N + 1) + 2,vec2(realData[2*2], realData[2*2+1]));
}
{
transposeStore(sharedBase + lane * (N + 1) + 3,vec2(realData[2*3], realData[2*3+1]));
}
{
transposeStore(sharedBase + lane * (N + 1) + 4,vec2(realData[2*4], realData[2*4+1]));
}
{
transposeStore(sharedBase + lane * (N + 1) + 5,vec2(realData[2*5], realData[2*5+1]));
}
{
transposeStore(sharedBase + lane * (N + 1) + 6,vec2(realData[2*6], realData[2*6+1]));
}
{
transposeStore(sharedBase + lane * (N + 1) + 7,vec2(realData[2*7], realData[2*7+1]));
}
{
transposeStore(sharedBase + lane * (N + 1) + 8,vec2(realData[2*8], realData[2*8+1]));
}
}
        TILE_BARRIER();
        if (lane <= N/2) {
            vec2 column[N];
            {
{
column[0] = transposeLoad(sharedBase + 0 * (N + 1) + lane);
}
{
column[1] = transposeLoad(sharedBase + 1 * (N + 1) + lane);
}
{
column[2] = transposeLoad(sharedBase + 2 * (N + 1) + lane);
}
{
column[3] = transposeLoad(sharedBase + 3 * (N + 1) + lane);
}
{
column[4] = transposeLoad(sharedBase + 4 * (N + 1) + lane);
}
{
column[5] = transposeLoad(sharedBase + 5 * (N + 1) + lane);
}
{
column[6] = transposeLoad(sharedBase + 6 * (N + 1) + lane);
}
{
column[7] = transposeLoad(sharedBase + 7 * (N + 1) + lane);
}
{
column[8] = transposeLoad(sharedBase + 8 * (N + 1) + lane);
}
{
column[9] = transposeLoad(sharedBase + 9 * (N + 1) + lane);
}
{
column[10] = transposeLoad(sharedBase + 10 * (N + 1) + lane);
}
{
column[11] = transposeLoad(sharedBase + 11 * (N + 1) + lane);
}
{
column[12] = transposeLoad(sharedBase + 12 * (N + 1) + lane);
}
{
column[13] = transposeLoad(sharedBase + 13 * (N + 1) + lane);
}
{
column[14] = transposeLoad(sharedBase + 14 * (N + 1) + lane);
}
{
column[15] = transposeLoad(sharedBase + 15 * (N + 1) + lane);
}
}
            dft16(column);
            {
{
spectrum[0] = column[0];
}
{
spectrum[1] = column[1];
}
{
spectrum[2] = column[2];
}
{
spectrum[3] = column[3];
}
{
spectrum[4] = column[4];
}
{
spectrum[5] = column[5];
}
{
spectrum[6] = column[6];
}
{
spectrum[7] = column[7];
}
{
spectrum[8] = column[8];
}
{
spectrum[9] = column[9];
}
{
spectrum[10] = column[10];
}
{
spectrum[11] = column[11];
}
{
spectrum[12] = column[12];
}
{
spectrum[13] = column[13];
}
{
spectrum[14] = column[14];
}
{
spectrum[15] = column[15];
}
}
        }
        TILE_BARRIER();
    
    if (validBlock && lane <= N/2) {
        {
{

            int destination = blockId*pc.dstStride + 2*(0*(N/2+1)+lane);
            cachedDestination[destination]=spectrum[0].x;
            cachedDestination[destination+1]=spectrum[0].y;
        
}
{

            int destination = blockId*pc.dstStride + 2*(1*(N/2+1)+lane);
            cachedDestination[destination]=spectrum[1].x;
            cachedDestination[destination+1]=spectrum[1].y;
        
}
{

            int destination = blockId*pc.dstStride + 2*(2*(N/2+1)+lane);
            cachedDestination[destination]=spectrum[2].x;
            cachedDestination[destination+1]=spectrum[2].y;
        
}
{

            int destination = blockId*pc.dstStride + 2*(3*(N/2+1)+lane);
            cachedDestination[destination]=spectrum[3].x;
            cachedDestination[destination+1]=spectrum[3].y;
        
}
{

            int destination = blockId*pc.dstStride + 2*(4*(N/2+1)+lane);
            cachedDestination[destination]=spectrum[4].x;
            cachedDestination[destination+1]=spectrum[4].y;
        
}
{

            int destination = blockId*pc.dstStride + 2*(5*(N/2+1)+lane);
            cachedDestination[destination]=spectrum[5].x;
            cachedDestination[destination+1]=spectrum[5].y;
        
}
{

            int destination = blockId*pc.dstStride + 2*(6*(N/2+1)+lane);
            cachedDestination[destination]=spectrum[6].x;
            cachedDestination[destination+1]=spectrum[6].y;
        
}
{

            int destination = blockId*pc.dstStride + 2*(7*(N/2+1)+lane);
            cachedDestination[destination]=spectrum[7].x;
            cachedDestination[destination+1]=spectrum[7].y;
        
}
{

            int destination = blockId*pc.dstStride + 2*(8*(N/2+1)+lane);
            cachedDestination[destination]=spectrum[8].x;
            cachedDestination[destination+1]=spectrum[8].y;
        
}
{

            int destination = blockId*pc.dstStride + 2*(9*(N/2+1)+lane);
            cachedDestination[destination]=spectrum[9].x;
            cachedDestination[destination+1]=spectrum[9].y;
        
}
{

            int destination = blockId*pc.dstStride + 2*(10*(N/2+1)+lane);
            cachedDestination[destination]=spectrum[10].x;
            cachedDestination[destination+1]=spectrum[10].y;
        
}
{

            int destination = blockId*pc.dstStride + 2*(11*(N/2+1)+lane);
            cachedDestination[destination]=spectrum[11].x;
            cachedDestination[destination+1]=spectrum[11].y;
        
}
{

            int destination = blockId*pc.dstStride + 2*(12*(N/2+1)+lane);
            cachedDestination[destination]=spectrum[12].x;
            cachedDestination[destination+1]=spectrum[12].y;
        
}
{

            int destination = blockId*pc.dstStride + 2*(13*(N/2+1)+lane);
            cachedDestination[destination]=spectrum[13].x;
            cachedDestination[destination+1]=spectrum[13].y;
        
}
{

            int destination = blockId*pc.dstStride + 2*(14*(N/2+1)+lane);
            cachedDestination[destination]=spectrum[14].x;
            cachedDestination[destination+1]=spectrum[14].y;
        
}
{

            int destination = blockId*pc.dstStride + 2*(15*(N/2+1)+lane);
            cachedDestination[destination]=spectrum[15].x;
            cachedDestination[destination+1]=spectrum[15].y;
        
}
}
    }
}
#endif
#if T == 5
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
    vec2 spectrum[N];


        int x = reflectCoordinate(originX + lane, pc.width);
        {
{

            float v = validBlock ? sampleFrame(0, x, reflectCoordinate(originY+0, pc.height)) : 0.0;
#if REAL_SHARED
            transposeStoreReal(sharedBase + 0 * (N + 1) + lane,
                v * analysisWindow((T/2)*N+0,lane));
#else
            transposeStore(sharedBase + 0 * (N + 1) + lane,
                vec2(v * analysisWindow((T/2)*N+0,lane), 0.0));
#endif
        
}
{

            float v = validBlock ? sampleFrame(0, x, reflectCoordinate(originY+1, pc.height)) : 0.0;
#if REAL_SHARED
            transposeStoreReal(sharedBase + 1 * (N + 1) + lane,
                v * analysisWindow((T/2)*N+1,lane));
#else
            transposeStore(sharedBase + 1 * (N + 1) + lane,
                vec2(v * analysisWindow((T/2)*N+1,lane), 0.0));
#endif
        
}
{

            float v = validBlock ? sampleFrame(0, x, reflectCoordinate(originY+2, pc.height)) : 0.0;
#if REAL_SHARED
            transposeStoreReal(sharedBase + 2 * (N + 1) + lane,
                v * analysisWindow((T/2)*N+2,lane));
#else
            transposeStore(sharedBase + 2 * (N + 1) + lane,
                vec2(v * analysisWindow((T/2)*N+2,lane), 0.0));
#endif
        
}
{

            float v = validBlock ? sampleFrame(0, x, reflectCoordinate(originY+3, pc.height)) : 0.0;
#if REAL_SHARED
            transposeStoreReal(sharedBase + 3 * (N + 1) + lane,
                v * analysisWindow((T/2)*N+3,lane));
#else
            transposeStore(sharedBase + 3 * (N + 1) + lane,
                vec2(v * analysisWindow((T/2)*N+3,lane), 0.0));
#endif
        
}
{

            float v = validBlock ? sampleFrame(0, x, reflectCoordinate(originY+4, pc.height)) : 0.0;
#if REAL_SHARED
            transposeStoreReal(sharedBase + 4 * (N + 1) + lane,
                v * analysisWindow((T/2)*N+4,lane));
#else
            transposeStore(sharedBase + 4 * (N + 1) + lane,
                vec2(v * analysisWindow((T/2)*N+4,lane), 0.0));
#endif
        
}
{

            float v = validBlock ? sampleFrame(0, x, reflectCoordinate(originY+5, pc.height)) : 0.0;
#if REAL_SHARED
            transposeStoreReal(sharedBase + 5 * (N + 1) + lane,
                v * analysisWindow((T/2)*N+5,lane));
#else
            transposeStore(sharedBase + 5 * (N + 1) + lane,
                vec2(v * analysisWindow((T/2)*N+5,lane), 0.0));
#endif
        
}
{

            float v = validBlock ? sampleFrame(0, x, reflectCoordinate(originY+6, pc.height)) : 0.0;
#if REAL_SHARED
            transposeStoreReal(sharedBase + 6 * (N + 1) + lane,
                v * analysisWindow((T/2)*N+6,lane));
#else
            transposeStore(sharedBase + 6 * (N + 1) + lane,
                vec2(v * analysisWindow((T/2)*N+6,lane), 0.0));
#endif
        
}
{

            float v = validBlock ? sampleFrame(0, x, reflectCoordinate(originY+7, pc.height)) : 0.0;
#if REAL_SHARED
            transposeStoreReal(sharedBase + 7 * (N + 1) + lane,
                v * analysisWindow((T/2)*N+7,lane));
#else
            transposeStore(sharedBase + 7 * (N + 1) + lane,
                vec2(v * analysisWindow((T/2)*N+7,lane), 0.0));
#endif
        
}
{

            float v = validBlock ? sampleFrame(0, x, reflectCoordinate(originY+8, pc.height)) : 0.0;
#if REAL_SHARED
            transposeStoreReal(sharedBase + 8 * (N + 1) + lane,
                v * analysisWindow((T/2)*N+8,lane));
#else
            transposeStore(sharedBase + 8 * (N + 1) + lane,
                vec2(v * analysisWindow((T/2)*N+8,lane), 0.0));
#endif
        
}
{

            float v = validBlock ? sampleFrame(0, x, reflectCoordinate(originY+9, pc.height)) : 0.0;
#if REAL_SHARED
            transposeStoreReal(sharedBase + 9 * (N + 1) + lane,
                v * analysisWindow((T/2)*N+9,lane));
#else
            transposeStore(sharedBase + 9 * (N + 1) + lane,
                vec2(v * analysisWindow((T/2)*N+9,lane), 0.0));
#endif
        
}
{

            float v = validBlock ? sampleFrame(0, x, reflectCoordinate(originY+10, pc.height)) : 0.0;
#if REAL_SHARED
            transposeStoreReal(sharedBase + 10 * (N + 1) + lane,
                v * analysisWindow((T/2)*N+10,lane));
#else
            transposeStore(sharedBase + 10 * (N + 1) + lane,
                vec2(v * analysisWindow((T/2)*N+10,lane), 0.0));
#endif
        
}
{

            float v = validBlock ? sampleFrame(0, x, reflectCoordinate(originY+11, pc.height)) : 0.0;
#if REAL_SHARED
            transposeStoreReal(sharedBase + 11 * (N + 1) + lane,
                v * analysisWindow((T/2)*N+11,lane));
#else
            transposeStore(sharedBase + 11 * (N + 1) + lane,
                vec2(v * analysisWindow((T/2)*N+11,lane), 0.0));
#endif
        
}
{

            float v = validBlock ? sampleFrame(0, x, reflectCoordinate(originY+12, pc.height)) : 0.0;
#if REAL_SHARED
            transposeStoreReal(sharedBase + 12 * (N + 1) + lane,
                v * analysisWindow((T/2)*N+12,lane));
#else
            transposeStore(sharedBase + 12 * (N + 1) + lane,
                vec2(v * analysisWindow((T/2)*N+12,lane), 0.0));
#endif
        
}
{

            float v = validBlock ? sampleFrame(0, x, reflectCoordinate(originY+13, pc.height)) : 0.0;
#if REAL_SHARED
            transposeStoreReal(sharedBase + 13 * (N + 1) + lane,
                v * analysisWindow((T/2)*N+13,lane));
#else
            transposeStore(sharedBase + 13 * (N + 1) + lane,
                vec2(v * analysisWindow((T/2)*N+13,lane), 0.0));
#endif
        
}
{

            float v = validBlock ? sampleFrame(0, x, reflectCoordinate(originY+14, pc.height)) : 0.0;
#if REAL_SHARED
            transposeStoreReal(sharedBase + 14 * (N + 1) + lane,
                v * analysisWindow((T/2)*N+14,lane));
#else
            transposeStore(sharedBase + 14 * (N + 1) + lane,
                vec2(v * analysisWindow((T/2)*N+14,lane), 0.0));
#endif
        
}
{

            float v = validBlock ? sampleFrame(0, x, reflectCoordinate(originY+15, pc.height)) : 0.0;
#if REAL_SHARED
            transposeStoreReal(sharedBase + 15 * (N + 1) + lane,
                v * analysisWindow((T/2)*N+15,lane));
#else
            transposeStore(sharedBase + 15 * (N + 1) + lane,
                vec2(v * analysisWindow((T/2)*N+15,lane), 0.0));
#endif
        
}
}
        TILE_BARRIER();
        float realData[N + 2];
        {
{
realData[0] = transposeLoadReal(sharedBase + lane * (N + 1) + 0);
}
{
realData[1] = transposeLoadReal(sharedBase + lane * (N + 1) + 1);
}
{
realData[2] = transposeLoadReal(sharedBase + lane * (N + 1) + 2);
}
{
realData[3] = transposeLoadReal(sharedBase + lane * (N + 1) + 3);
}
{
realData[4] = transposeLoadReal(sharedBase + lane * (N + 1) + 4);
}
{
realData[5] = transposeLoadReal(sharedBase + lane * (N + 1) + 5);
}
{
realData[6] = transposeLoadReal(sharedBase + lane * (N + 1) + 6);
}
{
realData[7] = transposeLoadReal(sharedBase + lane * (N + 1) + 7);
}
{
realData[8] = transposeLoadReal(sharedBase + lane * (N + 1) + 8);
}
{
realData[9] = transposeLoadReal(sharedBase + lane * (N + 1) + 9);
}
{
realData[10] = transposeLoadReal(sharedBase + lane * (N + 1) + 10);
}
{
realData[11] = transposeLoadReal(sharedBase + lane * (N + 1) + 11);
}
{
realData[12] = transposeLoadReal(sharedBase + lane * (N + 1) + 12);
}
{
realData[13] = transposeLoadReal(sharedBase + lane * (N + 1) + 13);
}
{
realData[14] = transposeLoadReal(sharedBase + lane * (N + 1) + 14);
}
{
realData[15] = transposeLoadReal(sharedBase + lane * (N + 1) + 15);
}
}
        TILE_BARRIER();
        rdft16(realData);
        {
{
transposeStore(sharedBase + lane * (N + 1) + 0,vec2(realData[2*0], realData[2*0+1]));
}
{
transposeStore(sharedBase + lane * (N + 1) + 1,vec2(realData[2*1], realData[2*1+1]));
}
{
transposeStore(sharedBase + lane * (N + 1) + 2,vec2(realData[2*2], realData[2*2+1]));
}
{
transposeStore(sharedBase + lane * (N + 1) + 3,vec2(realData[2*3], realData[2*3+1]));
}
{
transposeStore(sharedBase + lane * (N + 1) + 4,vec2(realData[2*4], realData[2*4+1]));
}
{
transposeStore(sharedBase + lane * (N + 1) + 5,vec2(realData[2*5], realData[2*5+1]));
}
{
transposeStore(sharedBase + lane * (N + 1) + 6,vec2(realData[2*6], realData[2*6+1]));
}
{
transposeStore(sharedBase + lane * (N + 1) + 7,vec2(realData[2*7], realData[2*7+1]));
}
{
transposeStore(sharedBase + lane * (N + 1) + 8,vec2(realData[2*8], realData[2*8+1]));
}
}
        TILE_BARRIER();
        if (lane <= N/2) {
            vec2 column[N];
            {
{
column[0] = transposeLoad(sharedBase + 0 * (N + 1) + lane);
}
{
column[1] = transposeLoad(sharedBase + 1 * (N + 1) + lane);
}
{
column[2] = transposeLoad(sharedBase + 2 * (N + 1) + lane);
}
{
column[3] = transposeLoad(sharedBase + 3 * (N + 1) + lane);
}
{
column[4] = transposeLoad(sharedBase + 4 * (N + 1) + lane);
}
{
column[5] = transposeLoad(sharedBase + 5 * (N + 1) + lane);
}
{
column[6] = transposeLoad(sharedBase + 6 * (N + 1) + lane);
}
{
column[7] = transposeLoad(sharedBase + 7 * (N + 1) + lane);
}
{
column[8] = transposeLoad(sharedBase + 8 * (N + 1) + lane);
}
{
column[9] = transposeLoad(sharedBase + 9 * (N + 1) + lane);
}
{
column[10] = transposeLoad(sharedBase + 10 * (N + 1) + lane);
}
{
column[11] = transposeLoad(sharedBase + 11 * (N + 1) + lane);
}
{
column[12] = transposeLoad(sharedBase + 12 * (N + 1) + lane);
}
{
column[13] = transposeLoad(sharedBase + 13 * (N + 1) + lane);
}
{
column[14] = transposeLoad(sharedBase + 14 * (N + 1) + lane);
}
{
column[15] = transposeLoad(sharedBase + 15 * (N + 1) + lane);
}
}
            dft16(column);
            {
{
spectrum[0] = column[0];
}
{
spectrum[1] = column[1];
}
{
spectrum[2] = column[2];
}
{
spectrum[3] = column[3];
}
{
spectrum[4] = column[4];
}
{
spectrum[5] = column[5];
}
{
spectrum[6] = column[6];
}
{
spectrum[7] = column[7];
}
{
spectrum[8] = column[8];
}
{
spectrum[9] = column[9];
}
{
spectrum[10] = column[10];
}
{
spectrum[11] = column[11];
}
{
spectrum[12] = column[12];
}
{
spectrum[13] = column[13];
}
{
spectrum[14] = column[14];
}
{
spectrum[15] = column[15];
}
}
        }
        TILE_BARRIER();
    
    if (validBlock && lane <= N/2) {
        {
{

            int destination = blockId*pc.dstStride + 2*(0*(N/2+1)+lane);
            cachedDestination[destination]=spectrum[0].x;
            cachedDestination[destination+1]=spectrum[0].y;
        
}
{

            int destination = blockId*pc.dstStride + 2*(1*(N/2+1)+lane);
            cachedDestination[destination]=spectrum[1].x;
            cachedDestination[destination+1]=spectrum[1].y;
        
}
{

            int destination = blockId*pc.dstStride + 2*(2*(N/2+1)+lane);
            cachedDestination[destination]=spectrum[2].x;
            cachedDestination[destination+1]=spectrum[2].y;
        
}
{

            int destination = blockId*pc.dstStride + 2*(3*(N/2+1)+lane);
            cachedDestination[destination]=spectrum[3].x;
            cachedDestination[destination+1]=spectrum[3].y;
        
}
{

            int destination = blockId*pc.dstStride + 2*(4*(N/2+1)+lane);
            cachedDestination[destination]=spectrum[4].x;
            cachedDestination[destination+1]=spectrum[4].y;
        
}
{

            int destination = blockId*pc.dstStride + 2*(5*(N/2+1)+lane);
            cachedDestination[destination]=spectrum[5].x;
            cachedDestination[destination+1]=spectrum[5].y;
        
}
{

            int destination = blockId*pc.dstStride + 2*(6*(N/2+1)+lane);
            cachedDestination[destination]=spectrum[6].x;
            cachedDestination[destination+1]=spectrum[6].y;
        
}
{

            int destination = blockId*pc.dstStride + 2*(7*(N/2+1)+lane);
            cachedDestination[destination]=spectrum[7].x;
            cachedDestination[destination+1]=spectrum[7].y;
        
}
{

            int destination = blockId*pc.dstStride + 2*(8*(N/2+1)+lane);
            cachedDestination[destination]=spectrum[8].x;
            cachedDestination[destination+1]=spectrum[8].y;
        
}
{

            int destination = blockId*pc.dstStride + 2*(9*(N/2+1)+lane);
            cachedDestination[destination]=spectrum[9].x;
            cachedDestination[destination+1]=spectrum[9].y;
        
}
{

            int destination = blockId*pc.dstStride + 2*(10*(N/2+1)+lane);
            cachedDestination[destination]=spectrum[10].x;
            cachedDestination[destination+1]=spectrum[10].y;
        
}
{

            int destination = blockId*pc.dstStride + 2*(11*(N/2+1)+lane);
            cachedDestination[destination]=spectrum[11].x;
            cachedDestination[destination+1]=spectrum[11].y;
        
}
{

            int destination = blockId*pc.dstStride + 2*(12*(N/2+1)+lane);
            cachedDestination[destination]=spectrum[12].x;
            cachedDestination[destination+1]=spectrum[12].y;
        
}
{

            int destination = blockId*pc.dstStride + 2*(13*(N/2+1)+lane);
            cachedDestination[destination]=spectrum[13].x;
            cachedDestination[destination+1]=spectrum[13].y;
        
}
{

            int destination = blockId*pc.dstStride + 2*(14*(N/2+1)+lane);
            cachedDestination[destination]=spectrum[14].x;
            cachedDestination[destination+1]=spectrum[14].y;
        
}
{

            int destination = blockId*pc.dstStride + 2*(15*(N/2+1)+lane);
            cachedDestination[destination]=spectrum[15].x;
            cachedDestination[destination+1]=spectrum[15].y;
        
}
}
    }
}
#endif
#if T == 7
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
    vec2 spectrum[N];


        int x = reflectCoordinate(originX + lane, pc.width);
        {
{

            float v = validBlock ? sampleFrame(0, x, reflectCoordinate(originY+0, pc.height)) : 0.0;
#if REAL_SHARED
            transposeStoreReal(sharedBase + 0 * (N + 1) + lane,
                v * analysisWindow((T/2)*N+0,lane));
#else
            transposeStore(sharedBase + 0 * (N + 1) + lane,
                vec2(v * analysisWindow((T/2)*N+0,lane), 0.0));
#endif
        
}
{

            float v = validBlock ? sampleFrame(0, x, reflectCoordinate(originY+1, pc.height)) : 0.0;
#if REAL_SHARED
            transposeStoreReal(sharedBase + 1 * (N + 1) + lane,
                v * analysisWindow((T/2)*N+1,lane));
#else
            transposeStore(sharedBase + 1 * (N + 1) + lane,
                vec2(v * analysisWindow((T/2)*N+1,lane), 0.0));
#endif
        
}
{

            float v = validBlock ? sampleFrame(0, x, reflectCoordinate(originY+2, pc.height)) : 0.0;
#if REAL_SHARED
            transposeStoreReal(sharedBase + 2 * (N + 1) + lane,
                v * analysisWindow((T/2)*N+2,lane));
#else
            transposeStore(sharedBase + 2 * (N + 1) + lane,
                vec2(v * analysisWindow((T/2)*N+2,lane), 0.0));
#endif
        
}
{

            float v = validBlock ? sampleFrame(0, x, reflectCoordinate(originY+3, pc.height)) : 0.0;
#if REAL_SHARED
            transposeStoreReal(sharedBase + 3 * (N + 1) + lane,
                v * analysisWindow((T/2)*N+3,lane));
#else
            transposeStore(sharedBase + 3 * (N + 1) + lane,
                vec2(v * analysisWindow((T/2)*N+3,lane), 0.0));
#endif
        
}
{

            float v = validBlock ? sampleFrame(0, x, reflectCoordinate(originY+4, pc.height)) : 0.0;
#if REAL_SHARED
            transposeStoreReal(sharedBase + 4 * (N + 1) + lane,
                v * analysisWindow((T/2)*N+4,lane));
#else
            transposeStore(sharedBase + 4 * (N + 1) + lane,
                vec2(v * analysisWindow((T/2)*N+4,lane), 0.0));
#endif
        
}
{

            float v = validBlock ? sampleFrame(0, x, reflectCoordinate(originY+5, pc.height)) : 0.0;
#if REAL_SHARED
            transposeStoreReal(sharedBase + 5 * (N + 1) + lane,
                v * analysisWindow((T/2)*N+5,lane));
#else
            transposeStore(sharedBase + 5 * (N + 1) + lane,
                vec2(v * analysisWindow((T/2)*N+5,lane), 0.0));
#endif
        
}
{

            float v = validBlock ? sampleFrame(0, x, reflectCoordinate(originY+6, pc.height)) : 0.0;
#if REAL_SHARED
            transposeStoreReal(sharedBase + 6 * (N + 1) + lane,
                v * analysisWindow((T/2)*N+6,lane));
#else
            transposeStore(sharedBase + 6 * (N + 1) + lane,
                vec2(v * analysisWindow((T/2)*N+6,lane), 0.0));
#endif
        
}
{

            float v = validBlock ? sampleFrame(0, x, reflectCoordinate(originY+7, pc.height)) : 0.0;
#if REAL_SHARED
            transposeStoreReal(sharedBase + 7 * (N + 1) + lane,
                v * analysisWindow((T/2)*N+7,lane));
#else
            transposeStore(sharedBase + 7 * (N + 1) + lane,
                vec2(v * analysisWindow((T/2)*N+7,lane), 0.0));
#endif
        
}
{

            float v = validBlock ? sampleFrame(0, x, reflectCoordinate(originY+8, pc.height)) : 0.0;
#if REAL_SHARED
            transposeStoreReal(sharedBase + 8 * (N + 1) + lane,
                v * analysisWindow((T/2)*N+8,lane));
#else
            transposeStore(sharedBase + 8 * (N + 1) + lane,
                vec2(v * analysisWindow((T/2)*N+8,lane), 0.0));
#endif
        
}
{

            float v = validBlock ? sampleFrame(0, x, reflectCoordinate(originY+9, pc.height)) : 0.0;
#if REAL_SHARED
            transposeStoreReal(sharedBase + 9 * (N + 1) + lane,
                v * analysisWindow((T/2)*N+9,lane));
#else
            transposeStore(sharedBase + 9 * (N + 1) + lane,
                vec2(v * analysisWindow((T/2)*N+9,lane), 0.0));
#endif
        
}
{

            float v = validBlock ? sampleFrame(0, x, reflectCoordinate(originY+10, pc.height)) : 0.0;
#if REAL_SHARED
            transposeStoreReal(sharedBase + 10 * (N + 1) + lane,
                v * analysisWindow((T/2)*N+10,lane));
#else
            transposeStore(sharedBase + 10 * (N + 1) + lane,
                vec2(v * analysisWindow((T/2)*N+10,lane), 0.0));
#endif
        
}
{

            float v = validBlock ? sampleFrame(0, x, reflectCoordinate(originY+11, pc.height)) : 0.0;
#if REAL_SHARED
            transposeStoreReal(sharedBase + 11 * (N + 1) + lane,
                v * analysisWindow((T/2)*N+11,lane));
#else
            transposeStore(sharedBase + 11 * (N + 1) + lane,
                vec2(v * analysisWindow((T/2)*N+11,lane), 0.0));
#endif
        
}
{

            float v = validBlock ? sampleFrame(0, x, reflectCoordinate(originY+12, pc.height)) : 0.0;
#if REAL_SHARED
            transposeStoreReal(sharedBase + 12 * (N + 1) + lane,
                v * analysisWindow((T/2)*N+12,lane));
#else
            transposeStore(sharedBase + 12 * (N + 1) + lane,
                vec2(v * analysisWindow((T/2)*N+12,lane), 0.0));
#endif
        
}
{

            float v = validBlock ? sampleFrame(0, x, reflectCoordinate(originY+13, pc.height)) : 0.0;
#if REAL_SHARED
            transposeStoreReal(sharedBase + 13 * (N + 1) + lane,
                v * analysisWindow((T/2)*N+13,lane));
#else
            transposeStore(sharedBase + 13 * (N + 1) + lane,
                vec2(v * analysisWindow((T/2)*N+13,lane), 0.0));
#endif
        
}
{

            float v = validBlock ? sampleFrame(0, x, reflectCoordinate(originY+14, pc.height)) : 0.0;
#if REAL_SHARED
            transposeStoreReal(sharedBase + 14 * (N + 1) + lane,
                v * analysisWindow((T/2)*N+14,lane));
#else
            transposeStore(sharedBase + 14 * (N + 1) + lane,
                vec2(v * analysisWindow((T/2)*N+14,lane), 0.0));
#endif
        
}
{

            float v = validBlock ? sampleFrame(0, x, reflectCoordinate(originY+15, pc.height)) : 0.0;
#if REAL_SHARED
            transposeStoreReal(sharedBase + 15 * (N + 1) + lane,
                v * analysisWindow((T/2)*N+15,lane));
#else
            transposeStore(sharedBase + 15 * (N + 1) + lane,
                vec2(v * analysisWindow((T/2)*N+15,lane), 0.0));
#endif
        
}
}
        TILE_BARRIER();
        float realData[N + 2];
        {
{
realData[0] = transposeLoadReal(sharedBase + lane * (N + 1) + 0);
}
{
realData[1] = transposeLoadReal(sharedBase + lane * (N + 1) + 1);
}
{
realData[2] = transposeLoadReal(sharedBase + lane * (N + 1) + 2);
}
{
realData[3] = transposeLoadReal(sharedBase + lane * (N + 1) + 3);
}
{
realData[4] = transposeLoadReal(sharedBase + lane * (N + 1) + 4);
}
{
realData[5] = transposeLoadReal(sharedBase + lane * (N + 1) + 5);
}
{
realData[6] = transposeLoadReal(sharedBase + lane * (N + 1) + 6);
}
{
realData[7] = transposeLoadReal(sharedBase + lane * (N + 1) + 7);
}
{
realData[8] = transposeLoadReal(sharedBase + lane * (N + 1) + 8);
}
{
realData[9] = transposeLoadReal(sharedBase + lane * (N + 1) + 9);
}
{
realData[10] = transposeLoadReal(sharedBase + lane * (N + 1) + 10);
}
{
realData[11] = transposeLoadReal(sharedBase + lane * (N + 1) + 11);
}
{
realData[12] = transposeLoadReal(sharedBase + lane * (N + 1) + 12);
}
{
realData[13] = transposeLoadReal(sharedBase + lane * (N + 1) + 13);
}
{
realData[14] = transposeLoadReal(sharedBase + lane * (N + 1) + 14);
}
{
realData[15] = transposeLoadReal(sharedBase + lane * (N + 1) + 15);
}
}
        TILE_BARRIER();
        rdft16(realData);
        {
{
transposeStore(sharedBase + lane * (N + 1) + 0,vec2(realData[2*0], realData[2*0+1]));
}
{
transposeStore(sharedBase + lane * (N + 1) + 1,vec2(realData[2*1], realData[2*1+1]));
}
{
transposeStore(sharedBase + lane * (N + 1) + 2,vec2(realData[2*2], realData[2*2+1]));
}
{
transposeStore(sharedBase + lane * (N + 1) + 3,vec2(realData[2*3], realData[2*3+1]));
}
{
transposeStore(sharedBase + lane * (N + 1) + 4,vec2(realData[2*4], realData[2*4+1]));
}
{
transposeStore(sharedBase + lane * (N + 1) + 5,vec2(realData[2*5], realData[2*5+1]));
}
{
transposeStore(sharedBase + lane * (N + 1) + 6,vec2(realData[2*6], realData[2*6+1]));
}
{
transposeStore(sharedBase + lane * (N + 1) + 7,vec2(realData[2*7], realData[2*7+1]));
}
{
transposeStore(sharedBase + lane * (N + 1) + 8,vec2(realData[2*8], realData[2*8+1]));
}
}
        TILE_BARRIER();
        if (lane <= N/2) {
            vec2 column[N];
            {
{
column[0] = transposeLoad(sharedBase + 0 * (N + 1) + lane);
}
{
column[1] = transposeLoad(sharedBase + 1 * (N + 1) + lane);
}
{
column[2] = transposeLoad(sharedBase + 2 * (N + 1) + lane);
}
{
column[3] = transposeLoad(sharedBase + 3 * (N + 1) + lane);
}
{
column[4] = transposeLoad(sharedBase + 4 * (N + 1) + lane);
}
{
column[5] = transposeLoad(sharedBase + 5 * (N + 1) + lane);
}
{
column[6] = transposeLoad(sharedBase + 6 * (N + 1) + lane);
}
{
column[7] = transposeLoad(sharedBase + 7 * (N + 1) + lane);
}
{
column[8] = transposeLoad(sharedBase + 8 * (N + 1) + lane);
}
{
column[9] = transposeLoad(sharedBase + 9 * (N + 1) + lane);
}
{
column[10] = transposeLoad(sharedBase + 10 * (N + 1) + lane);
}
{
column[11] = transposeLoad(sharedBase + 11 * (N + 1) + lane);
}
{
column[12] = transposeLoad(sharedBase + 12 * (N + 1) + lane);
}
{
column[13] = transposeLoad(sharedBase + 13 * (N + 1) + lane);
}
{
column[14] = transposeLoad(sharedBase + 14 * (N + 1) + lane);
}
{
column[15] = transposeLoad(sharedBase + 15 * (N + 1) + lane);
}
}
            dft16(column);
            {
{
spectrum[0] = column[0];
}
{
spectrum[1] = column[1];
}
{
spectrum[2] = column[2];
}
{
spectrum[3] = column[3];
}
{
spectrum[4] = column[4];
}
{
spectrum[5] = column[5];
}
{
spectrum[6] = column[6];
}
{
spectrum[7] = column[7];
}
{
spectrum[8] = column[8];
}
{
spectrum[9] = column[9];
}
{
spectrum[10] = column[10];
}
{
spectrum[11] = column[11];
}
{
spectrum[12] = column[12];
}
{
spectrum[13] = column[13];
}
{
spectrum[14] = column[14];
}
{
spectrum[15] = column[15];
}
}
        }
        TILE_BARRIER();
    
    if (validBlock && lane <= N/2) {
        {
{

            int destination = blockId*pc.dstStride + 2*(0*(N/2+1)+lane);
            cachedDestination[destination]=spectrum[0].x;
            cachedDestination[destination+1]=spectrum[0].y;
        
}
{

            int destination = blockId*pc.dstStride + 2*(1*(N/2+1)+lane);
            cachedDestination[destination]=spectrum[1].x;
            cachedDestination[destination+1]=spectrum[1].y;
        
}
{

            int destination = blockId*pc.dstStride + 2*(2*(N/2+1)+lane);
            cachedDestination[destination]=spectrum[2].x;
            cachedDestination[destination+1]=spectrum[2].y;
        
}
{

            int destination = blockId*pc.dstStride + 2*(3*(N/2+1)+lane);
            cachedDestination[destination]=spectrum[3].x;
            cachedDestination[destination+1]=spectrum[3].y;
        
}
{

            int destination = blockId*pc.dstStride + 2*(4*(N/2+1)+lane);
            cachedDestination[destination]=spectrum[4].x;
            cachedDestination[destination+1]=spectrum[4].y;
        
}
{

            int destination = blockId*pc.dstStride + 2*(5*(N/2+1)+lane);
            cachedDestination[destination]=spectrum[5].x;
            cachedDestination[destination+1]=spectrum[5].y;
        
}
{

            int destination = blockId*pc.dstStride + 2*(6*(N/2+1)+lane);
            cachedDestination[destination]=spectrum[6].x;
            cachedDestination[destination+1]=spectrum[6].y;
        
}
{

            int destination = blockId*pc.dstStride + 2*(7*(N/2+1)+lane);
            cachedDestination[destination]=spectrum[7].x;
            cachedDestination[destination+1]=spectrum[7].y;
        
}
{

            int destination = blockId*pc.dstStride + 2*(8*(N/2+1)+lane);
            cachedDestination[destination]=spectrum[8].x;
            cachedDestination[destination+1]=spectrum[8].y;
        
}
{

            int destination = blockId*pc.dstStride + 2*(9*(N/2+1)+lane);
            cachedDestination[destination]=spectrum[9].x;
            cachedDestination[destination+1]=spectrum[9].y;
        
}
{

            int destination = blockId*pc.dstStride + 2*(10*(N/2+1)+lane);
            cachedDestination[destination]=spectrum[10].x;
            cachedDestination[destination+1]=spectrum[10].y;
        
}
{

            int destination = blockId*pc.dstStride + 2*(11*(N/2+1)+lane);
            cachedDestination[destination]=spectrum[11].x;
            cachedDestination[destination+1]=spectrum[11].y;
        
}
{

            int destination = blockId*pc.dstStride + 2*(12*(N/2+1)+lane);
            cachedDestination[destination]=spectrum[12].x;
            cachedDestination[destination+1]=spectrum[12].y;
        
}
{

            int destination = blockId*pc.dstStride + 2*(13*(N/2+1)+lane);
            cachedDestination[destination]=spectrum[13].x;
            cachedDestination[destination+1]=spectrum[13].y;
        
}
{

            int destination = blockId*pc.dstStride + 2*(14*(N/2+1)+lane);
            cachedDestination[destination]=spectrum[14].x;
            cachedDestination[destination+1]=spectrum[14].y;
        
}
{

            int destination = blockId*pc.dstStride + 2*(15*(N/2+1)+lane);
            cachedDestination[destination]=spectrum[15].x;
            cachedDestination[destination+1]=spectrum[15].y;
        
}
}
    }
}
#endif
#else
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
    vec2 spectrum[N];


        int x = reflectCoordinate(originX + lane, pc.width);
        UNROLL_LOOP for (int y=0; y<N; ++y) {
            float v = validBlock ? sampleFrame(0, x, reflectCoordinate(originY+y, pc.height)) : 0.0;
#if REAL_SHARED
            transposeStoreReal(sharedBase + y * (N + 1) + lane,
                v * analysisWindow((T/2)*N+y,lane));
#else
            transposeStore(sharedBase + y * (N + 1) + lane,
                vec2(v * analysisWindow((T/2)*N+y,lane), 0.0));
#endif
        }
        TILE_BARRIER();
        float realData[N + 2];
        UNROLL_LOOP for (int x=0; x<N; ++x)
            realData[x] = transposeLoadReal(sharedBase + lane * (N + 1) + x);
        TILE_BARRIER();
        rdft16(realData);
        UNROLL_LOOP for (int x=0; x<=N/2; ++x)
            transposeStore(sharedBase + lane * (N + 1) + x,vec2(realData[2*x], realData[2*x+1]));
        TILE_BARRIER();
        if (lane <= N/2) {
            vec2 column[N];
            UNROLL_LOOP for (int y=0; y<N; ++y)
                column[y] = transposeLoad(sharedBase + y * (N + 1) + lane);
            dft16(column);
            UNROLL_LOOP for (int y=0; y<N; ++y) spectrum[y] = column[y];
        }
        TILE_BARRIER();
    
    if (validBlock && lane <= N/2) {
        UNROLL_LOOP for (int y=0; y<N; ++y) {
            int destination = blockId*pc.dstStride + 2*(y*(N/2+1)+lane);
            cachedDestination[destination]=spectrum[y].x;
            cachedDestination[destination+1]=spectrum[y].y;
        }
    }
}
#endif
)glsl";
inline const char* finish = R"glsl(

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
void rdft16(inout float data[18]) {

    DK(KP923879532, +0.923879532511286756128183189396788286822416626);
    DK(KP707106781, +0.707106781186547524400844362104849039284835938);
    DK(KP414213562, +0.414213562373095048801688724209698078569671875);

    float T3;
    float T6;
    float T7;
    float T49;
    float T37;
    float T10;
    float T13;
    float T14;
    float T50;
    float T40;
    float T22;
    float T55;
    float T34;
    float T44;
    float T29;
    float T54;
    float T35;
    float T47;
    float T38;
    float T39;
    float T53;
    float T56;
    {
        float T1;
        float T2;
        float T4;
        float T5;
        T1 = data[0];
        T2 = data[8];
        T3 = T1 + T2;
        T4 = data[4];
        T5 = data[12];
        T6 = T4 + T5;
        T7 = T3 + T6;
        T49 = T4 - T5;
        T37 = T1 - T2;
    }
    {
        float T8;
        float T9;
        float T11;
        float T12;
        T8 = data[2];
        T9 = data[10];
        T10 = T8 + T9;
        T38 = T8 - T9;
        T11 = data[14];
        T12 = data[6];
        T13 = T11 + T12;
        T39 = T11 - T12;
    }
    T14 = T10 + T13;
    T50 = T39 - T38;
    T40 = T38 + T39;
    {
        float T18;
        float T42;
        float T21;
        float T43;
        {
            float T16;
            float T17;
            float T19;
            float T20;
            T16 = data[1];
            T17 = data[9];
            T18 = T16 + T17;
            T42 = T16 - T17;
            T19 = data[5];
            T20 = data[13];
            T21 = T19 + T20;
            T43 = T19 - T20;
        }
        T22 = T18 - T21;
        T55 = FMA(KP414213562, T42, T43);
        T34 = T18 + T21;
        T44 = FNMS(KP414213562, T43, T42);
    }
    {
        float T25;
        float T45;
        float T28;
        float T46;
        {
            float T23;
            float T24;
            float T26;
            float T27;
            T23 = data[15];
            T24 = data[7];
            T25 = T23 + T24;
            T45 = T23 - T24;
            T26 = data[3];
            T27 = data[11];
            T28 = T26 + T27;
            T46 = T27 - T26;
        }
        T29 = T25 - T28;
        T54 = FMA(KP414213562, T45, T46);
        T35 = T25 + T28;
        T47 = FNMS(KP414213562, T46, T45);
    }
    data[8] = T7 - T14;
    data[9] = T35 - T34;
    {
        float T15;
        float T30;
        float T31;
        float T32;
        T15 = T3 - T6;
        T30 = T22 + T29;
        data[12] = FNMS(KP707106781, T30, T15);
        data[4] = FMA(KP707106781, T30, T15);
        T31 = T13 - T10;
        T32 = T29 - T22;
        data[5] = FMA(KP707106781, T32, T31);
        data[13] = FMS(KP707106781, T32, T31);
    }
    {
        float T33;
        float T36;
        float T41;
        float T48;
        T33 = T7 + T14;
        T36 = T34 + T35;
        data[16] = T33 - T36;
        data[0] = T33 + T36;
        T41 = FMA(KP707106781, T40, T37);
        T48 = T44 + T47;
        data[14] = FNMS(KP923879532, T48, T41);
        data[2] = FMA(KP923879532, T48, T41);
    }
    T53 = FNMS(KP707106781, T50, T49);
    T56 = T54 - T55;
    data[3] = FMS(KP923879532, T56, T53);
    data[15] = FMA(KP923879532, T56, T53);
    {
        float T57;
        float T58;
        float T51;
        float T52;
        T57 = FNMS(KP707106781, T40, T37);
        T58 = T55 + T54;
        data[10] = FNMS(KP923879532, T58, T57);
        data[6] = FMA(KP923879532, T58, T57);
        T51 = FMA(KP707106781, T50, T49);
        T52 = T47 - T44;
        data[7] = FMA(KP923879532, T52, T51);
        data[11] = FMS(KP923879532, T52, T51);
    }
    data[1] = 0.0f;
    data[17] = 0.0f;
}
void dft16(inout vec2 data[16]) {

    DK(KP923879532, +0.923879532511286756128183189396788286822416626);
    DK(KP414213562, +0.414213562373095048801688724209698078569671875);
    DK(KP707106781, +0.707106781186547524400844362104849039284835938);

    float T7;
    float T115;
    float T129;
    float T38;
    float T49;
    float T95;
    float T105;
    float T83;
    float T29;
    float T126;
    float T141;
    float T73;
    float T78;
    float T102;
    float T123;
    float T101;
    float T14;
    float T116;
    float T130;
    float T45;
    float T52;
    float T84;
    float T85;
    float T55;
    float T22;
    float T121;
    float T140;
    float T62;
    float T67;
    float T99;
    float T118;
    float T98;
    {
        float T3;
        float T47;
        float T34;
        float T82;
        float T6;
        float T81;
        float T37;
        float T48;
        {
            float T1;
            float T2;
            float T32;
            float T33;
            T1 = data[0].x;
            T2 = data[8].x;
            T3 = T1 + T2;
            T47 = T1 - T2;
            T32 = data[0].y;
            T33 = data[8].y;
            T34 = T32 + T33;
            T82 = T32 - T33;
        }
        {
            float T4;
            float T5;
            float T35;
            float T36;
            T4 = data[4].x;
            T5 = data[12].x;
            T6 = T4 + T5;
            T81 = T4 - T5;
            T35 = data[4].y;
            T36 = data[12].y;
            T37 = T35 + T36;
            T48 = T35 - T36;
        }
        T7 = T3 + T6;
        T115 = T3 - T6;
        T129 = T34 - T37;
        T38 = T34 + T37;
        T49 = T47 - T48;
        T95 = T47 + T48;
        T105 = T82 - T81;
        T83 = T81 + T82;
    }
    {
        float T25;
        float T74;
        float T72;
        float T124;
        float T28;
        float T69;
        float T77;
        float T125;
        {
            float T23;
            float T24;
            float T70;
            float T71;
            T23 = data[15].x;
            T24 = data[7].x;
            T25 = T23 + T24;
            T74 = T23 - T24;
            T70 = data[15].y;
            T71 = data[7].y;
            T72 = T70 - T71;
            T124 = T70 + T71;
        }
        {
            float T26;
            float T27;
            float T75;
            float T76;
            T26 = data[3].x;
            T27 = data[11].x;
            T28 = T26 + T27;
            T69 = T26 - T27;
            T75 = data[3].y;
            T76 = data[11].y;
            T77 = T75 - T76;
            T125 = T75 + T76;
        }
        T29 = T25 + T28;
        T126 = T124 - T125;
        T141 = T124 + T125;
        T73 = T69 + T72;
        T78 = T74 - T77;
        T102 = T72 - T69;
        T123 = T25 - T28;
        T101 = T74 + T77;
    }
    {
        float T10;
        float T51;
        float T41;
        float T50;
        float T13;
        float T53;
        float T44;
        float T54;
        {
            float T8;
            float T9;
            float T39;
            float T40;
            T8 = data[2].x;
            T9 = data[10].x;
            T10 = T8 + T9;
            T51 = T8 - T9;
            T39 = data[2].y;
            T40 = data[10].y;
            T41 = T39 + T40;
            T50 = T39 - T40;
        }
        {
            float T11;
            float T12;
            float T42;
            float T43;
            T11 = data[14].x;
            T12 = data[6].x;
            T13 = T11 + T12;
            T53 = T11 - T12;
            T42 = data[14].y;
            T43 = data[6].y;
            T44 = T42 + T43;
            T54 = T42 - T43;
        }
        T14 = T10 + T13;
        T116 = T41 - T44;
        T130 = T13 - T10;
        T45 = T41 + T44;
        T52 = T50 - T51;
        T84 = T53 - T54;
        T85 = T51 + T50;
        T55 = T53 + T54;
    }
    {
        float T18;
        float T63;
        float T61;
        float T119;
        float T21;
        float T58;
        float T66;
        float T120;
        {
            float T16;
            float T17;
            float T59;
            float T60;
            T16 = data[1].x;
            T17 = data[9].x;
            T18 = T16 + T17;
            T63 = T16 - T17;
            T59 = data[1].y;
            T60 = data[9].y;
            T61 = T59 - T60;
            T119 = T59 + T60;
        }
        {
            float T19;
            float T20;
            float T64;
            float T65;
            T19 = data[5].x;
            T20 = data[13].x;
            T21 = T19 + T20;
            T58 = T19 - T20;
            T64 = data[5].y;
            T65 = data[13].y;
            T66 = T64 - T65;
            T120 = T64 + T65;
        }
        T22 = T18 + T21;
        T121 = T119 - T120;
        T140 = T119 + T120;
        T62 = T58 + T61;
        T67 = T63 - T66;
        T99 = T61 - T58;
        T118 = T18 - T21;
        T98 = T63 + T66;
    }
    {
        float T15;
        float T30;
        float T143;
        float T144;
        T15 = T7 + T14;
        T30 = T22 + T29;
        data[8].x = T15 - T30;
        data[0].x = T15 + T30;
        T143 = T38 + T45;
        T144 = T140 + T141;
        data[8].y = T143 - T144;
        data[0].y = T143 + T144;
    }
    {
        float T31;
        float T46;
        float T139;
        float T142;
        T31 = T29 - T22;
        T46 = T38 - T45;
        data[4].y = T31 + T46;
        data[12].y = T46 - T31;
        T139 = T7 - T14;
        T142 = T140 - T141;
        data[12].x = T139 - T142;
        data[4].x = T139 + T142;
    }
    {
        float T117;
        float T131;
        float T128;
        float T132;
        float T122;
        float T127;
        T117 = T115 + T116;
        T131 = T129 - T130;
        T122 = T118 + T121;
        T127 = T123 - T126;
        T128 = T122 + T127;
        T132 = T127 - T122;
        data[10].x = FNMS(KP707106781, T128, T117);
        data[6].y = FMA(KP707106781, T132, T131);
        data[2].x = FMA(KP707106781, T128, T117);
        data[14].y = FNMS(KP707106781, T132, T131);
    }
    {
        float T133;
        float T137;
        float T136;
        float T138;
        float T134;
        float T135;
        T133 = T115 - T116;
        T137 = T130 + T129;
        T134 = T121 - T118;
        T135 = T123 + T126;
        T136 = T134 - T135;
        T138 = T134 + T135;
        data[14].x = FNMS(KP707106781, T136, T133);
        data[2].y = FMA(KP707106781, T138, T137);
        data[6].x = FMA(KP707106781, T136, T133);
        data[10].y = FNMS(KP707106781, T138, T137);
    }
    {
        float T57;
        float T93;
        float T87;
        float T89;
        float T80;
        float T88;
        float T92;
        float T94;
        float T56;
        float T86;
        T56 = T52 - T55;
        T57 = FMA(KP707106781, T56, T49);
        T93 = FNMS(KP707106781, T56, T49);
        T86 = T84 - T85;
        T87 = FNMS(KP707106781, T86, T83);
        T89 = FMA(KP707106781, T86, T83);
        {
            float T68;
            float T79;
            float T90;
            float T91;
            T68 = FMA(KP414213562, T67, T62);
            T79 = FNMS(KP414213562, T78, T73);
            T80 = T68 - T79;
            T88 = T68 + T79;
            T90 = FMA(KP414213562, T73, T78);
            T91 = FNMS(KP414213562, T62, T67);
            T92 = T90 - T91;
            T94 = T91 + T90;
        }
        data[11].x = FNMS(KP923879532, T80, T57);
        data[11].y = FNMS(KP923879532, T92, T89);
        data[3].x = FMA(KP923879532, T80, T57);
        data[3].y = FMA(KP923879532, T92, T89);
        data[7].y = FNMS(KP923879532, T88, T87);
        data[7].x = FNMS(KP923879532, T94, T93);
        data[15].y = FMA(KP923879532, T88, T87);
        data[15].x = FMA(KP923879532, T94, T93);
    }
    {
        float T97;
        float T109;
        float T107;
        float T113;
        float T104;
        float T108;
        float T112;
        float T114;
        float T96;
        float T106;
        T96 = T85 + T84;
        T97 = FMA(KP707106781, T96, T95);
        T109 = FNMS(KP707106781, T96, T95);
        T106 = T52 + T55;
        T107 = FNMS(KP707106781, T106, T105);
        T113 = FMA(KP707106781, T106, T105);
        {
            float T100;
            float T103;
            float T110;
            float T111;
            T100 = FMA(KP414213562, T99, T98);
            T103 = FNMS(KP414213562, T102, T101);
            T104 = T100 + T103;
            T108 = T103 - T100;
            T110 = FNMS(KP414213562, T98, T99);
            T111 = FMA(KP414213562, T101, T102);
            T112 = T110 - T111;
            T114 = T110 + T111;
        }
        data[9].x = FNMS(KP923879532, T104, T97);
        data[9].y = FNMS(KP923879532, T114, T113);
        data[1].x = FMA(KP923879532, T104, T97);
        data[1].y = FMA(KP923879532, T114, T113);
        data[13].y = FNMS(KP923879532, T108, T107);
        data[13].x = FNMS(KP923879532, T112, T109);
        data[5].y = FMA(KP923879532, T108, T107);
        data[5].x = FMA(KP923879532, T112, T109);
    }
}
void idft16(inout vec2 data[16]) {

    DK(KP923879532, +0.923879532511286756128183189396788286822416626);
    DK(KP414213562, +0.414213562373095048801688724209698078569671875);
    DK(KP707106781, +0.707106781186547524400844362104849039284835938);

    float T7;
    float T115;
    float T129;
    float T38;
    float T49;
    float T95;
    float T105;
    float T83;
    float T29;
    float T126;
    float T141;
    float T73;
    float T78;
    float T99;
    float T123;
    float T98;
    float T14;
    float T116;
    float T130;
    float T45;
    float T52;
    float T84;
    float T85;
    float T55;
    float T22;
    float T121;
    float T140;
    float T62;
    float T67;
    float T102;
    float T118;
    float T101;
    {
        float T3;
        float T81;
        float T34;
        float T48;
        float T6;
        float T47;
        float T37;
        float T82;
        {
            float T1;
            float T2;
            float T32;
            float T33;
            T1 = data[0].x;
            T2 = data[8].x;
            T3 = T1 + T2;
            T81 = T1 - T2;
            T32 = data[0].y;
            T33 = data[8].y;
            T34 = T32 + T33;
            T48 = T32 - T33;
        }
        {
            float T4;
            float T5;
            float T35;
            float T36;
            T4 = data[4].x;
            T5 = data[12].x;
            T6 = T4 + T5;
            T47 = T4 - T5;
            T35 = data[4].y;
            T36 = data[12].y;
            T37 = T35 + T36;
            T82 = T35 - T36;
        }
        T7 = T3 + T6;
        T115 = T34 - T37;
        T129 = T3 - T6;
        T38 = T34 + T37;
        T49 = T47 + T48;
        T95 = T81 + T82;
        T105 = T48 - T47;
        T83 = T81 - T82;
    }
    {
        float T25;
        float T74;
        float T72;
        float T124;
        float T28;
        float T69;
        float T77;
        float T125;
        {
            float T23;
            float T24;
            float T70;
            float T71;
            T23 = data[15].x;
            T24 = data[7].x;
            T25 = T23 + T24;
            T74 = T23 - T24;
            T70 = data[15].y;
            T71 = data[7].y;
            T72 = T70 - T71;
            T124 = T70 + T71;
        }
        {
            float T26;
            float T27;
            float T75;
            float T76;
            T26 = data[3].x;
            T27 = data[11].x;
            T28 = T26 + T27;
            T69 = T26 - T27;
            T75 = data[3].y;
            T76 = data[11].y;
            T77 = T75 - T76;
            T125 = T75 + T76;
        }
        T29 = T25 + T28;
        T126 = T124 - T125;
        T141 = T124 + T125;
        T73 = T69 + T72;
        T78 = T74 - T77;
        T99 = T74 + T77;
        T123 = T25 - T28;
        T98 = T72 - T69;
    }
    {
        float T10;
        float T50;
        float T41;
        float T51;
        float T13;
        float T54;
        float T44;
        float T53;
        {
            float T8;
            float T9;
            float T39;
            float T40;
            T8 = data[2].x;
            T9 = data[10].x;
            T10 = T8 + T9;
            T50 = T8 - T9;
            T39 = data[2].y;
            T40 = data[10].y;
            T41 = T39 + T40;
            T51 = T39 - T40;
        }
        {
            float T11;
            float T12;
            float T42;
            float T43;
            T11 = data[14].x;
            T12 = data[6].x;
            T13 = T11 + T12;
            T54 = T11 - T12;
            T42 = data[14].y;
            T43 = data[6].y;
            T44 = T42 + T43;
            T53 = T42 - T43;
        }
        T14 = T10 + T13;
        T116 = T10 - T13;
        T130 = T44 - T41;
        T45 = T41 + T44;
        T52 = T50 + T51;
        T84 = T50 - T51;
        T85 = T54 + T53;
        T55 = T53 - T54;
    }
    {
        float T18;
        float T63;
        float T61;
        float T119;
        float T21;
        float T58;
        float T66;
        float T120;
        {
            float T16;
            float T17;
            float T59;
            float T60;
            T16 = data[1].x;
            T17 = data[9].x;
            T18 = T16 + T17;
            T63 = T16 - T17;
            T59 = data[1].y;
            T60 = data[9].y;
            T61 = T59 - T60;
            T119 = T59 + T60;
        }
        {
            float T19;
            float T20;
            float T64;
            float T65;
            T19 = data[5].x;
            T20 = data[13].x;
            T21 = T19 + T20;
            T58 = T19 - T20;
            T64 = data[5].y;
            T65 = data[13].y;
            T66 = T64 - T65;
            T120 = T64 + T65;
        }
        T22 = T18 + T21;
        T121 = T119 - T120;
        T140 = T119 + T120;
        T62 = T58 + T61;
        T67 = T63 - T66;
        T102 = T63 + T66;
        T118 = T18 - T21;
        T101 = T61 - T58;
    }
    
    {
        float T15;
        float T30;
        float T139;
        float T142;
        T15 = T7 + T14;
        T30 = T22 + T29;
        data[8].x = T15 - T30;
        data[0].x = T15 + T30;
        T139 = T38 + T45;
        T142 = T140 + T141;
        data[8].y = T139 - T142;
        data[0].y = T139 + T142;
    }
    {
        float T31;
        float T46;
        float T143;
        float T144;
        T31 = T22 - T29;
        T46 = T38 - T45;
        data[4].y = T31 + T46;
        data[12].y = T46 - T31;
        T143 = T7 - T14;
        T144 = T141 - T140;
        data[12].x = T143 - T144;
        data[4].x = T143 + T144;
    }
    {
        float T117;
        float T131;
        float T128;
        float T132;
        float T122;
        float T127;
        T117 = T115 - T116;
        T131 = T129 + T130;
        T122 = T118 - T121;
        T127 = T123 + T126;
        T128 = T122 - T127;
        T132 = T122 + T127;
        data[14].y = FNMS(KP707106781, T128, T117);
        data[2].x = FMA(KP707106781, T132, T131);
        data[6].y = FMA(KP707106781, T128, T117);
        data[10].x = FNMS(KP707106781, T132, T131);
    }
    {
        float T133;
        float T137;
        float T136;
        float T138;
        float T134;
        float T135;
        T133 = T116 + T115;
        T137 = T129 - T130;
        T134 = T118 + T121;
        T135 = T126 - T123;
        T136 = T134 + T135;
        T138 = T135 - T134;
        data[10].y = FNMS(KP707106781, T136, T133);
        data[6].x = FMA(KP707106781, T138, T137);
        data[2].y = FMA(KP707106781, T136, T133);
        data[14].x = FNMS(KP707106781, T138, T137);
    }
    {
        float T57;
        float T89;
        float T87;
        float T93;
        float T80;
        float T88;
        float T92;
        float T94;
        float T56;
        float T86;
        T56 = T52 + T55;
        T57 = FMA(KP707106781, T56, T49);
        T89 = FNMS(KP707106781, T56, T49);
        T86 = T84 + T85;
        T87 = FNMS(KP707106781, T86, T83);
        T93 = FMA(KP707106781, T86, T83);
        {
            float T68;
            float T79;
            float T90;
            float T91;
            T68 = FMA(KP414213562, T67, T62);
            T79 = FNMS(KP414213562, T78, T73);
            T80 = T68 + T79;
            T88 = T79 - T68;
            T90 = FNMS(KP414213562, T62, T67);
            T91 = FMA(KP414213562, T73, T78);
            T92 = T90 - T91;
            T94 = T90 + T91;
        }
        data[9].y = FNMS(KP923879532, T80, T57);
        data[9].x = FNMS(KP923879532, T94, T93);
        data[1].y = FMA(KP923879532, T80, T57);
        data[1].x = FMA(KP923879532, T94, T93);
        data[13].x = FNMS(KP923879532, T88, T87);
        data[13].y = FNMS(KP923879532, T92, T89);
        data[5].x = FMA(KP923879532, T88, T87);
        data[5].y = FMA(KP923879532, T92, T89);
    }
    {
        float T97;
        float T113;
        float T107;
        float T109;
        float T104;
        float T108;
        float T112;
        float T114;
        float T96;
        float T106;
        T96 = T55 - T52;
        T97 = FMA(KP707106781, T96, T95);
        T113 = FNMS(KP707106781, T96, T95);
        T106 = T84 - T85;
        T107 = FNMS(KP707106781, T106, T105);
        T109 = FMA(KP707106781, T106, T105);
        {
            float T100;
            float T103;
            float T110;
            float T111;
            T100 = FMA(KP414213562, T99, T98);
            T103 = FNMS(KP414213562, T102, T101);
            T104 = T100 - T103;
            T108 = T103 + T100;
            T110 = FMA(KP414213562, T101, T102);
            T111 = FNMS(KP414213562, T98, T99);
            T112 = T110 - T111;
            T114 = T110 + T111;
        }
        data[11].x = FNMS(KP923879532, T104, T97);
        data[11].y = FNMS(KP923879532, T112, T109);
        data[3].x = FMA(KP923879532, T104, T97);
        data[3].y = FMA(KP923879532, T112, T109);
        data[7].y = FNMS(KP923879532, T108, T107);
        data[7].x = FNMS(KP923879532, T114, T113);
        data[15].y = FMA(KP923879532, T108, T107);
        data[15].x = FMA(KP923879532, T114, T113);
    }
}
void irdft16(inout float data[18]) {

    DK(KP1_847759065, +1.847759065022573512256366378793576573644833252);
    DK(KP414213562, +0.414213562373095048801688724209698078569671875);
    DK(KP1_414213562, +1.414213562373095048801688724209698078569671875);
    DK(KP2_000000000, +2.000000000000000000000000000000000000000000000);

    float T5;
    float T47;
    float T19;
    float T39;
    float T8;
    float T48;
    float T24;
    float T40;
    float T12;
    float T51;
    float T15;
    float T52;
    float T30;
    float T35;
    float T53;
    float T50;
    float T43;
    float T42;
    {
        float T4;
        float T18;
        float T3;
        float T17;
        float T1;
        float T2;
        T4 = data[8];
        T18 = data[9];
        T1 = data[0];
        T2 = data[16];
        T3 = T1 + T2;
        T17 = T1 - T2;
        T5 = FMA(KP2_000000000, T4, T3);
        T47 = FNMS(KP2_000000000, T4, T3);
        T19 = FMA(KP2_000000000, T18, T17);
        T39 = FNMS(KP2_000000000, T18, T17);
    }
    {
        float T6;
        float T7;
        float T20;
        float T21;
        float T22;
        float T23;
        T6 = data[4];
        T7 = data[12];
        T20 = T6 - T7;
        T21 = data[5];
        T22 = data[13];
        T23 = T21 + T22;
        T8 = T6 + T7;
        T48 = T21 - T22;
        T24 = T20 + T23;
        T40 = T23 - T20;
    }
    {
        float T26;
        float T33;
        float T34;
        float T29;
        {
            float T10;
            float T11;
            float T31;
            float T32;
            T10 = data[2];
            T11 = data[14];
            T12 = T10 + T11;
            T26 = T10 - T11;
            T31 = data[3];
            T32 = data[15];
            T33 = T31 + T32;
            T51 = T31 - T32;
        }
        {
            float T13;
            float T14;
            float T27;
            float T28;
            T13 = data[10];
            T14 = data[6];
            T15 = T13 + T14;
            T34 = T13 - T14;
            T27 = data[11];
            T28 = data[7];
            T29 = T27 + T28;
            T52 = T27 - T28;
        }
        T30 = T26 + T29;
        T35 = T33 - T34;
        T53 = T51 - T52;
        T50 = T12 - T15;
        T43 = T34 + T33;
        T42 = T26 - T29;
    }
    {
        float T9;
        float T16;
        float T55;
        float T56;
        T9 = FMA(KP2_000000000, T8, T5);
        T16 = T12 + T15;
        data[8] = FNMS(KP2_000000000, T16, T9);
        data[0] = FMA(KP2_000000000, T16, T9);
        T55 = FNMS(KP2_000000000, T48, T47);
        T56 = T53 - T50;
        data[14] = FNMS(KP1_414213562, T56, T55);
        data[6] = FMA(KP1_414213562, T56, T55);
    }
    {
        float T57;
        float T58;
        float T25;
        float T36;
        T57 = FNMS(KP2_000000000, T8, T5);
        T58 = T52 + T51;
        data[12] = FNMS(KP2_000000000, T58, T57);
        data[4] = FMA(KP2_000000000, T58, T57);
        T25 = FMA(KP1_414213562, T24, T19);
        T36 = FMA(KP414213562, T35, T30);
        data[9] = FNMS(KP1_847759065, T36, T25);
        data[1] = FMA(KP1_847759065, T36, T25);
    }
    {
        float T37;
        float T38;
        float T45;
        float T46;
        T37 = FNMS(KP1_414213562, T24, T19);
        T38 = FNMS(KP414213562, T30, T35);
        data[13] = FNMS(KP1_847759065, T38, T37);
        data[5] = FMA(KP1_847759065, T38, T37);
        T45 = FMA(KP1_414213562, T40, T39);
        T46 = FMA(KP414213562, T42, T43);
        data[11] = FNMS(KP1_847759065, T46, T45);
        data[3] = FMA(KP1_847759065, T46, T45);
    }
    {
        float T49;
        float T54;
        float T41;
        float T44;
        T49 = FMA(KP2_000000000, T48, T47);
        T54 = T50 + T53;
        data[10] = FNMS(KP1_414213562, T54, T49);
        data[2] = FMA(KP1_414213562, T54, T49);
        T41 = FNMS(KP1_414213562, T40, T39);
        T44 = FNMS(KP414213562, T43, T42);
        data[7] = FNMS(KP1_847759065, T44, T41);
        data[15] = FMA(KP1_847759065, T44, T41);
    }
}
#if T == 1
void dftTemporal(inout vec2 data[1]) {
}
#endif
#if T == 3
void dftTemporal(inout vec2 data[3]) {

    DK(KP866025403, +0.866025403784438646763723170752936183471402627);
    DK(KP500000000, +0.500000000000000000000000000000000000000000000);

    float T1;
    float T9;
    float T4;
    float T12;
    float T8;
    float T10;
    float T5;
    float T11;
    T1 = data[0].x;
    T9 = data[0].y;
    {
        float T2;
        float T3;
        float T6;
        float T7;
        T2 = data[1].x;
        T3 = data[2].x;
        T4 = T2 + T3;
        T12 = T3 - T2;
        T6 = data[1].y;
        T7 = data[2].y;
        T8 = T6 - T7;
        T10 = T6 + T7;
    }
    data[0].x = T1 + T4;
    data[0].y = T9 + T10;
    T5 = FNMS(KP500000000, T4, T1);
    data[2].x = FNMS(KP866025403, T8, T5);
    data[1].x = FMA(KP866025403, T8, T5);
    T11 = FNMS(KP500000000, T10, T9);
    data[1].y = FMA(KP866025403, T12, T11);
    data[2].y = FNMS(KP866025403, T12, T11);
}
#endif
#if T == 5
void dftTemporal(inout vec2 data[5]) {

    DK(KP951056516, +0.951056516295153572116439333379382143405698634);
    DK(KP559016994, +0.559016994374947424102293417182819058860154590);
    DK(KP250000000, +0.250000000000000000000000000000000000000000000);
    DK(KP618033988, +0.618033988749894848204586834365638117720309180);

    float T1;
    float T21;
    float T8;
    float T29;
    float T10;
    float T28;
    float T14;
    float T26;
    float T17;
    float T24;
    T1 = data[0].x;
    T21 = data[0].y;
    {
        float T2;
        float T3;
        float T4;
        float T5;
        float T6;
        float T7;
        T2 = data[1].x;
        T3 = data[4].x;
        T4 = T2 + T3;
        T5 = data[2].x;
        T6 = data[3].x;
        T7 = T5 + T6;
        T8 = T4 + T7;
        T29 = T5 - T6;
        T10 = T4 - T7;
        T28 = T2 - T3;
    }
    {
        float T12;
        float T13;
        float T22;
        float T15;
        float T16;
        float T23;
        T12 = data[1].y;
        T13 = data[4].y;
        T22 = T12 + T13;
        T15 = data[2].y;
        T16 = data[3].y;
        T23 = T15 + T16;
        T14 = T12 - T13;
        T26 = T22 - T23;
        T17 = T15 - T16;
        T24 = T22 + T23;
    }
    data[0].x = T1 + T8;
    data[0].y = T21 + T24;
    {
        float T18;
        float T20;
        float T11;
        float T19;
        float T9;
        T18 = FMA(KP618033988, T17, T14);
        T20 = FNMS(KP618033988, T14, T17);
        T9 = FNMS(KP250000000, T8, T1);
        T11 = FMA(KP559016994, T10, T9);
        T19 = FNMS(KP559016994, T10, T9);
        data[4].x = FNMS(KP951056516, T18, T11);
        data[3].x = FMA(KP951056516, T20, T19);
        data[1].x = FMA(KP951056516, T18, T11);
        data[2].x = FNMS(KP951056516, T20, T19);
    }
    {
        float T30;
        float T32;
        float T27;
        float T31;
        float T25;
        T30 = FMA(KP618033988, T29, T28);
        T32 = FNMS(KP618033988, T28, T29);
        T25 = FNMS(KP250000000, T24, T21);
        T27 = FMA(KP559016994, T26, T25);
        T31 = FNMS(KP559016994, T26, T25);
        data[1].y = FNMS(KP951056516, T30, T27);
        data[3].y = FNMS(KP951056516, T32, T31);
        data[4].y = FMA(KP951056516, T30, T27);
        data[2].y = FMA(KP951056516, T32, T31);
    }
}
#endif
#if T == 7
void dftTemporal(inout vec2 data[7]) {

    DK(KP974927912, +0.974927912181823607018131682993931217232785801);
    DK(KP900968867, +0.900968867902419126236102319507445051165919162);
    DK(KP692021471, +0.692021471630095869627814897002069140197260599);
    DK(KP801937735, +0.801937735804838252472204639014890102331838324);
    DK(KP554958132, +0.554958132087371191422194871006410481067288862);
    DK(KP356895867, +0.356895867892209443894399510021300583399127187);

    float T1;
    float T35;
    float T4;
    float T44;
    float T10;
    float T42;
    float T7;
    float T43;
    float T11;
    float T25;
    float T55;
    float T50;
    float T45;
    float T30;
    float T16;
    float T37;
    float T22;
    float T38;
    float T19;
    float T36;
    float T23;
    float T28;
    float T52;
    float T47;
    float T39;
    float T33;
    T1 = data[0].x;
    T35 = data[0].y;
    {
        float T2;
        float T3;
        float T14;
        float T15;
        T2 = data[1].x;
        T3 = data[6].x;
        T4 = T2 + T3;
        T44 = T3 - T2;
        {
            float T8;
            float T9;
            float T5;
            float T6;
            T8 = data[3].x;
            T9 = data[4].x;
            T10 = T8 + T9;
            T42 = T9 - T8;
            T5 = data[2].x;
            T6 = data[5].x;
            T7 = T5 + T6;
            T43 = T6 - T5;
        }
        T11 = FNMS(KP356895867, T7, T4);
        T25 = FNMS(KP356895867, T4, T10);
        T55 = FMA(KP554958132, T42, T44);
        T50 = FMA(KP554958132, T43, T42);
        T45 = FNMS(KP554958132, T44, T43);
        T30 = FNMS(KP356895867, T10, T7);
        T14 = data[2].y;
        T15 = data[5].y;
        T16 = T14 - T15;
        T37 = T14 + T15;
        {
            float T20;
            float T21;
            float T17;
            float T18;
            T20 = data[3].y;
            T21 = data[4].y;
            T22 = T20 - T21;
            T38 = T20 + T21;
            T17 = data[1].y;
            T18 = data[6].y;
            T19 = T17 - T18;
            T36 = T17 + T18;
        }
        T23 = FMA(KP554958132, T22, T19);
        T28 = FMA(KP554958132, T16, T22);
        T52 = FNMS(KP356895867, T37, T36);
        T47 = FNMS(KP356895867, T36, T38);
        T39 = FNMS(KP356895867, T38, T37);
        T33 = FNMS(KP554958132, T19, T16);
    }
    data[0].x = T1 + T4 + T7 + T10;
    data[0].y = T35 + T36 + T37 + T38;
    {
        float T24;
        float T13;
        float T12;
        float T56;
        float T54;
        float T53;
        T24 = FMA(KP801937735, T23, T16);
        T12 = FNMS(KP692021471, T11, T10);
        T13 = FNMS(KP900968867, T12, T1);
        data[6].x = FNMS(KP974927912, T24, T13);
        data[1].x = FMA(KP974927912, T24, T13);
        T56 = FMA(KP801937735, T55, T43);
        T53 = FNMS(KP692021471, T52, T38);
        T54 = FNMS(KP900968867, T53, T35);
        data[1].y = FMA(KP974927912, T56, T54);
        data[6].y = FNMS(KP974927912, T56, T54);
    }
    {
        float T29;
        float T27;
        float T26;
        float T51;
        float T49;
        float T48;
        T29 = FNMS(KP801937735, T28, T19);
        T26 = FNMS(KP692021471, T25, T7);
        T27 = FNMS(KP900968867, T26, T1);
        data[5].x = FNMS(KP974927912, T29, T27);
        data[2].x = FMA(KP974927912, T29, T27);
        T51 = FNMS(KP801937735, T50, T44);
        T48 = FNMS(KP692021471, T47, T37);
        T49 = FNMS(KP900968867, T48, T35);
        data[2].y = FMA(KP974927912, T51, T49);
        data[5].y = FNMS(KP974927912, T51, T49);
    }
    {
        float T34;
        float T32;
        float T31;
        float T46;
        float T41;
        float T40;
        T34 = FNMS(KP801937735, T33, T22);
        T31 = FNMS(KP692021471, T30, T4);
        T32 = FNMS(KP900968867, T31, T1);
        data[4].x = FNMS(KP974927912, T34, T32);
        data[3].x = FMA(KP974927912, T34, T32);
        T46 = FNMS(KP801937735, T45, T42);
        T40 = FNMS(KP692021471, T39, T36);
        T41 = FNMS(KP900968867, T40, T35);
        data[3].y = FMA(KP974927912, T46, T41);
        data[4].y = FNMS(KP974927912, T46, T41);
    }
}
#endif
#if WARP_TILE
layout(local_size_x=32*TILES, local_size_y=1, local_size_z=1) in;
#else
layout(local_size_x=N*TILES, local_size_y=1, local_size_z=1) in;
#endif
layout(set=0, binding=0, std430) readonly buffer Frame0 { float src0[]; };
layout(set=0, binding=1, std430) readonly buffer Frame1 { float src1[]; };
layout(set=0, binding=2, std430) readonly buffer Frame2 { float src2[]; };
layout(set=0, binding=3, std430) readonly buffer Frame3 { float src3[]; };
layout(set=0, binding=4, std430) readonly buffer Frame4 { float src4[]; };
layout(set=0, binding=5, std430) readonly buffer Frame5 { float src5[]; };
layout(set=0, binding=6, std430) readonly buffer Frame6 { float src6[]; };

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
#if STATIC_MAIN
#if T == 1
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
    {
{
result += vec2(value[0].x * phase[0].x - value[0].y * phase[0].y,
                       value[0].x * phase[0].y + value[0].y * phase[0].x);
}
}
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


    if (lane <= N/2) {
        {
{

            {
{

                spectrum[0*N+0] = validBlock ? readCachedSpectrum(0,blockId,0,lane)*temporalFactors[0] : vec2(0.0);
            
}
{

                spectrum[0*N+1] = validBlock ? readCachedSpectrum(0,blockId,1,lane)*temporalFactors[0] : vec2(0.0);
            
}
{

                spectrum[0*N+2] = validBlock ? readCachedSpectrum(0,blockId,2,lane)*temporalFactors[0] : vec2(0.0);
            
}
{

                spectrum[0*N+3] = validBlock ? readCachedSpectrum(0,blockId,3,lane)*temporalFactors[0] : vec2(0.0);
            
}
{

                spectrum[0*N+4] = validBlock ? readCachedSpectrum(0,blockId,4,lane)*temporalFactors[0] : vec2(0.0);
            
}
{

                spectrum[0*N+5] = validBlock ? readCachedSpectrum(0,blockId,5,lane)*temporalFactors[0] : vec2(0.0);
            
}
{

                spectrum[0*N+6] = validBlock ? readCachedSpectrum(0,blockId,6,lane)*temporalFactors[0] : vec2(0.0);
            
}
{

                spectrum[0*N+7] = validBlock ? readCachedSpectrum(0,blockId,7,lane)*temporalFactors[0] : vec2(0.0);
            
}
{

                spectrum[0*N+8] = validBlock ? readCachedSpectrum(0,blockId,8,lane)*temporalFactors[0] : vec2(0.0);
            
}
{

                spectrum[0*N+9] = validBlock ? readCachedSpectrum(0,blockId,9,lane)*temporalFactors[0] : vec2(0.0);
            
}
{

                spectrum[0*N+10] = validBlock ? readCachedSpectrum(0,blockId,10,lane)*temporalFactors[0] : vec2(0.0);
            
}
{

                spectrum[0*N+11] = validBlock ? readCachedSpectrum(0,blockId,11,lane)*temporalFactors[0] : vec2(0.0);
            
}
{

                spectrum[0*N+12] = validBlock ? readCachedSpectrum(0,blockId,12,lane)*temporalFactors[0] : vec2(0.0);
            
}
{

                spectrum[0*N+13] = validBlock ? readCachedSpectrum(0,blockId,13,lane)*temporalFactors[0] : vec2(0.0);
            
}
{

                spectrum[0*N+14] = validBlock ? readCachedSpectrum(0,blockId,14,lane)*temporalFactors[0] : vec2(0.0);
            
}
{

                spectrum[0*N+15] = validBlock ? readCachedSpectrum(0,blockId,15,lane)*temporalFactors[0] : vec2(0.0);
            
}
}
        
}
}
    }
    if (lane <= N/2) {
        {
{

            vec2 temporal[T];
            {
{
temporal[0] = spectrum[0*N+0];
}
}
            dftTemporal(temporal);
            {
{
spectrum[0*N+0] = temporal[0];
}
}
        
}
{

            vec2 temporal[T];
            {
{
temporal[0] = spectrum[0*N+1];
}
}
            dftTemporal(temporal);
            {
{
spectrum[0*N+1] = temporal[0];
}
}
        
}
{

            vec2 temporal[T];
            {
{
temporal[0] = spectrum[0*N+2];
}
}
            dftTemporal(temporal);
            {
{
spectrum[0*N+2] = temporal[0];
}
}
        
}
{

            vec2 temporal[T];
            {
{
temporal[0] = spectrum[0*N+3];
}
}
            dftTemporal(temporal);
            {
{
spectrum[0*N+3] = temporal[0];
}
}
        
}
{

            vec2 temporal[T];
            {
{
temporal[0] = spectrum[0*N+4];
}
}
            dftTemporal(temporal);
            {
{
spectrum[0*N+4] = temporal[0];
}
}
        
}
{

            vec2 temporal[T];
            {
{
temporal[0] = spectrum[0*N+5];
}
}
            dftTemporal(temporal);
            {
{
spectrum[0*N+5] = temporal[0];
}
}
        
}
{

            vec2 temporal[T];
            {
{
temporal[0] = spectrum[0*N+6];
}
}
            dftTemporal(temporal);
            {
{
spectrum[0*N+6] = temporal[0];
}
}
        
}
{

            vec2 temporal[T];
            {
{
temporal[0] = spectrum[0*N+7];
}
}
            dftTemporal(temporal);
            {
{
spectrum[0*N+7] = temporal[0];
}
}
        
}
{

            vec2 temporal[T];
            {
{
temporal[0] = spectrum[0*N+8];
}
}
            dftTemporal(temporal);
            {
{
spectrum[0*N+8] = temporal[0];
}
}
        
}
{

            vec2 temporal[T];
            {
{
temporal[0] = spectrum[0*N+9];
}
}
            dftTemporal(temporal);
            {
{
spectrum[0*N+9] = temporal[0];
}
}
        
}
{

            vec2 temporal[T];
            {
{
temporal[0] = spectrum[0*N+10];
}
}
            dftTemporal(temporal);
            {
{
spectrum[0*N+10] = temporal[0];
}
}
        
}
{

            vec2 temporal[T];
            {
{
temporal[0] = spectrum[0*N+11];
}
}
            dftTemporal(temporal);
            {
{
spectrum[0*N+11] = temporal[0];
}
}
        
}
{

            vec2 temporal[T];
            {
{
temporal[0] = spectrum[0*N+12];
}
}
            dftTemporal(temporal);
            {
{
spectrum[0*N+12] = temporal[0];
}
}
        
}
{

            vec2 temporal[T];
            {
{
temporal[0] = spectrum[0*N+13];
}
}
            dftTemporal(temporal);
            {
{
spectrum[0*N+13] = temporal[0];
}
}
        
}
{

            vec2 temporal[T];
            {
{
temporal[0] = spectrum[0*N+14];
}
}
            dftTemporal(temporal);
            {
{
spectrum[0*N+14] = temporal[0];
}
}
        
}
{

            vec2 temporal[T];
            {
{
temporal[0] = spectrum[0*N+15];
}
}
            dftTemporal(temporal);
            {
{
spectrum[0*N+15] = temporal[0];
}
}
        
}
}
    }
#if ZMEAN
    if (lane == 0) blockMean[tile] = spectrum[0].x / windowSpectrum(0,0).x;
    TILE_BARRIER();
#endif
    vec2 column[N];
    if (lane <= N/2) {
        {
{

            vec2 temporal[T];
            {
{

                int frequency = (0 * N + 0) * (N / 2 + 1) + lane;
                vec2 v = spectrum[0*N+0];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(0*N+0,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[0] = v;
            
}
}
            column[0] = inverseTemporalCenter(temporal);
        
}
{

            vec2 temporal[T];
            {
{

                int frequency = (0 * N + 1) * (N / 2 + 1) + lane;
                vec2 v = spectrum[0*N+1];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(0*N+1,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[0] = v;
            
}
}
            column[1] = inverseTemporalCenter(temporal);
        
}
{

            vec2 temporal[T];
            {
{

                int frequency = (0 * N + 2) * (N / 2 + 1) + lane;
                vec2 v = spectrum[0*N+2];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(0*N+2,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[0] = v;
            
}
}
            column[2] = inverseTemporalCenter(temporal);
        
}
{

            vec2 temporal[T];
            {
{

                int frequency = (0 * N + 3) * (N / 2 + 1) + lane;
                vec2 v = spectrum[0*N+3];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(0*N+3,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[0] = v;
            
}
}
            column[3] = inverseTemporalCenter(temporal);
        
}
{

            vec2 temporal[T];
            {
{

                int frequency = (0 * N + 4) * (N / 2 + 1) + lane;
                vec2 v = spectrum[0*N+4];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(0*N+4,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[0] = v;
            
}
}
            column[4] = inverseTemporalCenter(temporal);
        
}
{

            vec2 temporal[T];
            {
{

                int frequency = (0 * N + 5) * (N / 2 + 1) + lane;
                vec2 v = spectrum[0*N+5];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(0*N+5,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[0] = v;
            
}
}
            column[5] = inverseTemporalCenter(temporal);
        
}
{

            vec2 temporal[T];
            {
{

                int frequency = (0 * N + 6) * (N / 2 + 1) + lane;
                vec2 v = spectrum[0*N+6];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(0*N+6,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[0] = v;
            
}
}
            column[6] = inverseTemporalCenter(temporal);
        
}
{

            vec2 temporal[T];
            {
{

                int frequency = (0 * N + 7) * (N / 2 + 1) + lane;
                vec2 v = spectrum[0*N+7];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(0*N+7,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[0] = v;
            
}
}
            column[7] = inverseTemporalCenter(temporal);
        
}
{

            vec2 temporal[T];
            {
{

                int frequency = (0 * N + 8) * (N / 2 + 1) + lane;
                vec2 v = spectrum[0*N+8];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(0*N+8,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[0] = v;
            
}
}
            column[8] = inverseTemporalCenter(temporal);
        
}
{

            vec2 temporal[T];
            {
{

                int frequency = (0 * N + 9) * (N / 2 + 1) + lane;
                vec2 v = spectrum[0*N+9];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(0*N+9,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[0] = v;
            
}
}
            column[9] = inverseTemporalCenter(temporal);
        
}
{

            vec2 temporal[T];
            {
{

                int frequency = (0 * N + 10) * (N / 2 + 1) + lane;
                vec2 v = spectrum[0*N+10];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(0*N+10,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[0] = v;
            
}
}
            column[10] = inverseTemporalCenter(temporal);
        
}
{

            vec2 temporal[T];
            {
{

                int frequency = (0 * N + 11) * (N / 2 + 1) + lane;
                vec2 v = spectrum[0*N+11];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(0*N+11,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[0] = v;
            
}
}
            column[11] = inverseTemporalCenter(temporal);
        
}
{

            vec2 temporal[T];
            {
{

                int frequency = (0 * N + 12) * (N / 2 + 1) + lane;
                vec2 v = spectrum[0*N+12];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(0*N+12,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[0] = v;
            
}
}
            column[12] = inverseTemporalCenter(temporal);
        
}
{

            vec2 temporal[T];
            {
{

                int frequency = (0 * N + 13) * (N / 2 + 1) + lane;
                vec2 v = spectrum[0*N+13];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(0*N+13,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[0] = v;
            
}
}
            column[13] = inverseTemporalCenter(temporal);
        
}
{

            vec2 temporal[T];
            {
{

                int frequency = (0 * N + 14) * (N / 2 + 1) + lane;
                vec2 v = spectrum[0*N+14];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(0*N+14,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[0] = v;
            
}
}
            column[14] = inverseTemporalCenter(temporal);
        
}
{

            vec2 temporal[T];
            {
{

                int frequency = (0 * N + 15) * (N / 2 + 1) + lane;
                vec2 v = spectrum[0*N+15];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(0*N+15,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[0] = v;
            
}
}
            column[15] = inverseTemporalCenter(temporal);
        
}
}
        idft16(column);
        {
{
transposeStore(sharedBase + 0 * (N + 1) + lane,column[0]);
}
{
transposeStore(sharedBase + 1 * (N + 1) + lane,column[1]);
}
{
transposeStore(sharedBase + 2 * (N + 1) + lane,column[2]);
}
{
transposeStore(sharedBase + 3 * (N + 1) + lane,column[3]);
}
{
transposeStore(sharedBase + 4 * (N + 1) + lane,column[4]);
}
{
transposeStore(sharedBase + 5 * (N + 1) + lane,column[5]);
}
{
transposeStore(sharedBase + 6 * (N + 1) + lane,column[6]);
}
{
transposeStore(sharedBase + 7 * (N + 1) + lane,column[7]);
}
{
transposeStore(sharedBase + 8 * (N + 1) + lane,column[8]);
}
{
transposeStore(sharedBase + 9 * (N + 1) + lane,column[9]);
}
{
transposeStore(sharedBase + 10 * (N + 1) + lane,column[10]);
}
{
transposeStore(sharedBase + 11 * (N + 1) + lane,column[11]);
}
{
transposeStore(sharedBase + 12 * (N + 1) + lane,column[12]);
}
{
transposeStore(sharedBase + 13 * (N + 1) + lane,column[13]);
}
{
transposeStore(sharedBase + 14 * (N + 1) + lane,column[14]);
}
{
transposeStore(sharedBase + 15 * (N + 1) + lane,column[15]);
}
}
    }
    TILE_BARRIER();
    float realData[N + 2];
    {
{

        vec2 v = transposeLoad(sharedBase + lane * (N + 1) + 0);
        realData[2*0] = v.x;
        realData[2*0+1] = v.y;
    
}
{

        vec2 v = transposeLoad(sharedBase + lane * (N + 1) + 1);
        realData[2*1] = v.x;
        realData[2*1+1] = v.y;
    
}
{

        vec2 v = transposeLoad(sharedBase + lane * (N + 1) + 2);
        realData[2*2] = v.x;
        realData[2*2+1] = v.y;
    
}
{

        vec2 v = transposeLoad(sharedBase + lane * (N + 1) + 3);
        realData[2*3] = v.x;
        realData[2*3+1] = v.y;
    
}
{

        vec2 v = transposeLoad(sharedBase + lane * (N + 1) + 4);
        realData[2*4] = v.x;
        realData[2*4+1] = v.y;
    
}
{

        vec2 v = transposeLoad(sharedBase + lane * (N + 1) + 5);
        realData[2*5] = v.x;
        realData[2*5+1] = v.y;
    
}
{

        vec2 v = transposeLoad(sharedBase + lane * (N + 1) + 6);
        realData[2*6] = v.x;
        realData[2*6+1] = v.y;
    
}
{

        vec2 v = transposeLoad(sharedBase + lane * (N + 1) + 7);
        realData[2*7] = v.x;
        realData[2*7+1] = v.y;
    
}
{

        vec2 v = transposeLoad(sharedBase + lane * (N + 1) + 8);
        realData[2*8] = v.x;
        realData[2*8+1] = v.y;
    
}
}
    TILE_BARRIER();
    irdft16(realData);
    // FFTW's generated r2cb codelet uses the reverse real ordering.
    {
{
transposeStoreReal(sharedBase + 0 * (N + 1) + lane,realData[0 == 0 ? 0 : N-0]);
}
{
transposeStoreReal(sharedBase + 1 * (N + 1) + lane,realData[1 == 0 ? 0 : N-1]);
}
{
transposeStoreReal(sharedBase + 2 * (N + 1) + lane,realData[2 == 0 ? 0 : N-2]);
}
{
transposeStoreReal(sharedBase + 3 * (N + 1) + lane,realData[3 == 0 ? 0 : N-3]);
}
{
transposeStoreReal(sharedBase + 4 * (N + 1) + lane,realData[4 == 0 ? 0 : N-4]);
}
{
transposeStoreReal(sharedBase + 5 * (N + 1) + lane,realData[5 == 0 ? 0 : N-5]);
}
{
transposeStoreReal(sharedBase + 6 * (N + 1) + lane,realData[6 == 0 ? 0 : N-6]);
}
{
transposeStoreReal(sharedBase + 7 * (N + 1) + lane,realData[7 == 0 ? 0 : N-7]);
}
{
transposeStoreReal(sharedBase + 8 * (N + 1) + lane,realData[8 == 0 ? 0 : N-8]);
}
{
transposeStoreReal(sharedBase + 9 * (N + 1) + lane,realData[9 == 0 ? 0 : N-9]);
}
{
transposeStoreReal(sharedBase + 10 * (N + 1) + lane,realData[10 == 0 ? 0 : N-10]);
}
{
transposeStoreReal(sharedBase + 11 * (N + 1) + lane,realData[11 == 0 ? 0 : N-11]);
}
{
transposeStoreReal(sharedBase + 12 * (N + 1) + lane,realData[12 == 0 ? 0 : N-12]);
}
{
transposeStoreReal(sharedBase + 13 * (N + 1) + lane,realData[13 == 0 ? 0 : N-13]);
}
{
transposeStoreReal(sharedBase + 14 * (N + 1) + lane,realData[14 == 0 ? 0 : N-14]);
}
{
transposeStoreReal(sharedBase + 15 * (N + 1) + lane,realData[15 == 0 ? 0 : N-15]);
}
}
    TILE_BARRIER();
    if (validBlock)
        {
{
block[blockId*N*N + 0*N + lane] = transposeLoadReal(sharedBase + lane*(N+1)+0);
}
{
block[blockId*N*N + 1*N + lane] = transposeLoadReal(sharedBase + lane*(N+1)+1);
}
{
block[blockId*N*N + 2*N + lane] = transposeLoadReal(sharedBase + lane*(N+1)+2);
}
{
block[blockId*N*N + 3*N + lane] = transposeLoadReal(sharedBase + lane*(N+1)+3);
}
{
block[blockId*N*N + 4*N + lane] = transposeLoadReal(sharedBase + lane*(N+1)+4);
}
{
block[blockId*N*N + 5*N + lane] = transposeLoadReal(sharedBase + lane*(N+1)+5);
}
{
block[blockId*N*N + 6*N + lane] = transposeLoadReal(sharedBase + lane*(N+1)+6);
}
{
block[blockId*N*N + 7*N + lane] = transposeLoadReal(sharedBase + lane*(N+1)+7);
}
{
block[blockId*N*N + 8*N + lane] = transposeLoadReal(sharedBase + lane*(N+1)+8);
}
{
block[blockId*N*N + 9*N + lane] = transposeLoadReal(sharedBase + lane*(N+1)+9);
}
{
block[blockId*N*N + 10*N + lane] = transposeLoadReal(sharedBase + lane*(N+1)+10);
}
{
block[blockId*N*N + 11*N + lane] = transposeLoadReal(sharedBase + lane*(N+1)+11);
}
{
block[blockId*N*N + 12*N + lane] = transposeLoadReal(sharedBase + lane*(N+1)+12);
}
{
block[blockId*N*N + 13*N + lane] = transposeLoadReal(sharedBase + lane*(N+1)+13);
}
{
block[blockId*N*N + 14*N + lane] = transposeLoadReal(sharedBase + lane*(N+1)+14);
}
{
block[blockId*N*N + 15*N + lane] = transposeLoadReal(sharedBase + lane*(N+1)+15);
}
}
}
#endif
#if T == 3
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
    {
{
result += vec2(value[0].x * phase[0].x - value[0].y * phase[0].y,
                       value[0].x * phase[0].y + value[0].y * phase[0].x);
}
{
result += vec2(value[1].x * phase[1].x - value[1].y * phase[1].y,
                       value[1].x * phase[1].y + value[1].y * phase[1].x);
}
{
result += vec2(value[2].x * phase[2].x - value[2].y * phase[2].y,
                       value[2].x * phase[2].y + value[2].y * phase[2].x);
}
}
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


    if (lane <= N/2) {
        {
{

            {
{

                spectrum[0*N+0] = validBlock ? readCachedSpectrum(0,blockId,0,lane)*temporalFactors[0] : vec2(0.0);
            
}
{

                spectrum[0*N+1] = validBlock ? readCachedSpectrum(0,blockId,1,lane)*temporalFactors[0] : vec2(0.0);
            
}
{

                spectrum[0*N+2] = validBlock ? readCachedSpectrum(0,blockId,2,lane)*temporalFactors[0] : vec2(0.0);
            
}
{

                spectrum[0*N+3] = validBlock ? readCachedSpectrum(0,blockId,3,lane)*temporalFactors[0] : vec2(0.0);
            
}
{

                spectrum[0*N+4] = validBlock ? readCachedSpectrum(0,blockId,4,lane)*temporalFactors[0] : vec2(0.0);
            
}
{

                spectrum[0*N+5] = validBlock ? readCachedSpectrum(0,blockId,5,lane)*temporalFactors[0] : vec2(0.0);
            
}
{

                spectrum[0*N+6] = validBlock ? readCachedSpectrum(0,blockId,6,lane)*temporalFactors[0] : vec2(0.0);
            
}
{

                spectrum[0*N+7] = validBlock ? readCachedSpectrum(0,blockId,7,lane)*temporalFactors[0] : vec2(0.0);
            
}
{

                spectrum[0*N+8] = validBlock ? readCachedSpectrum(0,blockId,8,lane)*temporalFactors[0] : vec2(0.0);
            
}
{

                spectrum[0*N+9] = validBlock ? readCachedSpectrum(0,blockId,9,lane)*temporalFactors[0] : vec2(0.0);
            
}
{

                spectrum[0*N+10] = validBlock ? readCachedSpectrum(0,blockId,10,lane)*temporalFactors[0] : vec2(0.0);
            
}
{

                spectrum[0*N+11] = validBlock ? readCachedSpectrum(0,blockId,11,lane)*temporalFactors[0] : vec2(0.0);
            
}
{

                spectrum[0*N+12] = validBlock ? readCachedSpectrum(0,blockId,12,lane)*temporalFactors[0] : vec2(0.0);
            
}
{

                spectrum[0*N+13] = validBlock ? readCachedSpectrum(0,blockId,13,lane)*temporalFactors[0] : vec2(0.0);
            
}
{

                spectrum[0*N+14] = validBlock ? readCachedSpectrum(0,blockId,14,lane)*temporalFactors[0] : vec2(0.0);
            
}
{

                spectrum[0*N+15] = validBlock ? readCachedSpectrum(0,blockId,15,lane)*temporalFactors[0] : vec2(0.0);
            
}
}
        
}
{

            {
{

                spectrum[1*N+0] = validBlock ? readCachedSpectrum(1,blockId,0,lane)*temporalFactors[1] : vec2(0.0);
            
}
{

                spectrum[1*N+1] = validBlock ? readCachedSpectrum(1,blockId,1,lane)*temporalFactors[1] : vec2(0.0);
            
}
{

                spectrum[1*N+2] = validBlock ? readCachedSpectrum(1,blockId,2,lane)*temporalFactors[1] : vec2(0.0);
            
}
{

                spectrum[1*N+3] = validBlock ? readCachedSpectrum(1,blockId,3,lane)*temporalFactors[1] : vec2(0.0);
            
}
{

                spectrum[1*N+4] = validBlock ? readCachedSpectrum(1,blockId,4,lane)*temporalFactors[1] : vec2(0.0);
            
}
{

                spectrum[1*N+5] = validBlock ? readCachedSpectrum(1,blockId,5,lane)*temporalFactors[1] : vec2(0.0);
            
}
{

                spectrum[1*N+6] = validBlock ? readCachedSpectrum(1,blockId,6,lane)*temporalFactors[1] : vec2(0.0);
            
}
{

                spectrum[1*N+7] = validBlock ? readCachedSpectrum(1,blockId,7,lane)*temporalFactors[1] : vec2(0.0);
            
}
{

                spectrum[1*N+8] = validBlock ? readCachedSpectrum(1,blockId,8,lane)*temporalFactors[1] : vec2(0.0);
            
}
{

                spectrum[1*N+9] = validBlock ? readCachedSpectrum(1,blockId,9,lane)*temporalFactors[1] : vec2(0.0);
            
}
{

                spectrum[1*N+10] = validBlock ? readCachedSpectrum(1,blockId,10,lane)*temporalFactors[1] : vec2(0.0);
            
}
{

                spectrum[1*N+11] = validBlock ? readCachedSpectrum(1,blockId,11,lane)*temporalFactors[1] : vec2(0.0);
            
}
{

                spectrum[1*N+12] = validBlock ? readCachedSpectrum(1,blockId,12,lane)*temporalFactors[1] : vec2(0.0);
            
}
{

                spectrum[1*N+13] = validBlock ? readCachedSpectrum(1,blockId,13,lane)*temporalFactors[1] : vec2(0.0);
            
}
{

                spectrum[1*N+14] = validBlock ? readCachedSpectrum(1,blockId,14,lane)*temporalFactors[1] : vec2(0.0);
            
}
{

                spectrum[1*N+15] = validBlock ? readCachedSpectrum(1,blockId,15,lane)*temporalFactors[1] : vec2(0.0);
            
}
}
        
}
{

            {
{

                spectrum[2*N+0] = validBlock ? readCachedSpectrum(2,blockId,0,lane)*temporalFactors[2] : vec2(0.0);
            
}
{

                spectrum[2*N+1] = validBlock ? readCachedSpectrum(2,blockId,1,lane)*temporalFactors[2] : vec2(0.0);
            
}
{

                spectrum[2*N+2] = validBlock ? readCachedSpectrum(2,blockId,2,lane)*temporalFactors[2] : vec2(0.0);
            
}
{

                spectrum[2*N+3] = validBlock ? readCachedSpectrum(2,blockId,3,lane)*temporalFactors[2] : vec2(0.0);
            
}
{

                spectrum[2*N+4] = validBlock ? readCachedSpectrum(2,blockId,4,lane)*temporalFactors[2] : vec2(0.0);
            
}
{

                spectrum[2*N+5] = validBlock ? readCachedSpectrum(2,blockId,5,lane)*temporalFactors[2] : vec2(0.0);
            
}
{

                spectrum[2*N+6] = validBlock ? readCachedSpectrum(2,blockId,6,lane)*temporalFactors[2] : vec2(0.0);
            
}
{

                spectrum[2*N+7] = validBlock ? readCachedSpectrum(2,blockId,7,lane)*temporalFactors[2] : vec2(0.0);
            
}
{

                spectrum[2*N+8] = validBlock ? readCachedSpectrum(2,blockId,8,lane)*temporalFactors[2] : vec2(0.0);
            
}
{

                spectrum[2*N+9] = validBlock ? readCachedSpectrum(2,blockId,9,lane)*temporalFactors[2] : vec2(0.0);
            
}
{

                spectrum[2*N+10] = validBlock ? readCachedSpectrum(2,blockId,10,lane)*temporalFactors[2] : vec2(0.0);
            
}
{

                spectrum[2*N+11] = validBlock ? readCachedSpectrum(2,blockId,11,lane)*temporalFactors[2] : vec2(0.0);
            
}
{

                spectrum[2*N+12] = validBlock ? readCachedSpectrum(2,blockId,12,lane)*temporalFactors[2] : vec2(0.0);
            
}
{

                spectrum[2*N+13] = validBlock ? readCachedSpectrum(2,blockId,13,lane)*temporalFactors[2] : vec2(0.0);
            
}
{

                spectrum[2*N+14] = validBlock ? readCachedSpectrum(2,blockId,14,lane)*temporalFactors[2] : vec2(0.0);
            
}
{

                spectrum[2*N+15] = validBlock ? readCachedSpectrum(2,blockId,15,lane)*temporalFactors[2] : vec2(0.0);
            
}
}
        
}
}
    }
    if (lane <= N/2) {
        {
{

            vec2 temporal[T];
            {
{
temporal[0] = spectrum[0*N+0];
}
{
temporal[1] = spectrum[1*N+0];
}
{
temporal[2] = spectrum[2*N+0];
}
}
            dftTemporal(temporal);
            {
{
spectrum[0*N+0] = temporal[0];
}
{
spectrum[1*N+0] = temporal[1];
}
{
spectrum[2*N+0] = temporal[2];
}
}
        
}
{

            vec2 temporal[T];
            {
{
temporal[0] = spectrum[0*N+1];
}
{
temporal[1] = spectrum[1*N+1];
}
{
temporal[2] = spectrum[2*N+1];
}
}
            dftTemporal(temporal);
            {
{
spectrum[0*N+1] = temporal[0];
}
{
spectrum[1*N+1] = temporal[1];
}
{
spectrum[2*N+1] = temporal[2];
}
}
        
}
{

            vec2 temporal[T];
            {
{
temporal[0] = spectrum[0*N+2];
}
{
temporal[1] = spectrum[1*N+2];
}
{
temporal[2] = spectrum[2*N+2];
}
}
            dftTemporal(temporal);
            {
{
spectrum[0*N+2] = temporal[0];
}
{
spectrum[1*N+2] = temporal[1];
}
{
spectrum[2*N+2] = temporal[2];
}
}
        
}
{

            vec2 temporal[T];
            {
{
temporal[0] = spectrum[0*N+3];
}
{
temporal[1] = spectrum[1*N+3];
}
{
temporal[2] = spectrum[2*N+3];
}
}
            dftTemporal(temporal);
            {
{
spectrum[0*N+3] = temporal[0];
}
{
spectrum[1*N+3] = temporal[1];
}
{
spectrum[2*N+3] = temporal[2];
}
}
        
}
{

            vec2 temporal[T];
            {
{
temporal[0] = spectrum[0*N+4];
}
{
temporal[1] = spectrum[1*N+4];
}
{
temporal[2] = spectrum[2*N+4];
}
}
            dftTemporal(temporal);
            {
{
spectrum[0*N+4] = temporal[0];
}
{
spectrum[1*N+4] = temporal[1];
}
{
spectrum[2*N+4] = temporal[2];
}
}
        
}
{

            vec2 temporal[T];
            {
{
temporal[0] = spectrum[0*N+5];
}
{
temporal[1] = spectrum[1*N+5];
}
{
temporal[2] = spectrum[2*N+5];
}
}
            dftTemporal(temporal);
            {
{
spectrum[0*N+5] = temporal[0];
}
{
spectrum[1*N+5] = temporal[1];
}
{
spectrum[2*N+5] = temporal[2];
}
}
        
}
{

            vec2 temporal[T];
            {
{
temporal[0] = spectrum[0*N+6];
}
{
temporal[1] = spectrum[1*N+6];
}
{
temporal[2] = spectrum[2*N+6];
}
}
            dftTemporal(temporal);
            {
{
spectrum[0*N+6] = temporal[0];
}
{
spectrum[1*N+6] = temporal[1];
}
{
spectrum[2*N+6] = temporal[2];
}
}
        
}
{

            vec2 temporal[T];
            {
{
temporal[0] = spectrum[0*N+7];
}
{
temporal[1] = spectrum[1*N+7];
}
{
temporal[2] = spectrum[2*N+7];
}
}
            dftTemporal(temporal);
            {
{
spectrum[0*N+7] = temporal[0];
}
{
spectrum[1*N+7] = temporal[1];
}
{
spectrum[2*N+7] = temporal[2];
}
}
        
}
{

            vec2 temporal[T];
            {
{
temporal[0] = spectrum[0*N+8];
}
{
temporal[1] = spectrum[1*N+8];
}
{
temporal[2] = spectrum[2*N+8];
}
}
            dftTemporal(temporal);
            {
{
spectrum[0*N+8] = temporal[0];
}
{
spectrum[1*N+8] = temporal[1];
}
{
spectrum[2*N+8] = temporal[2];
}
}
        
}
{

            vec2 temporal[T];
            {
{
temporal[0] = spectrum[0*N+9];
}
{
temporal[1] = spectrum[1*N+9];
}
{
temporal[2] = spectrum[2*N+9];
}
}
            dftTemporal(temporal);
            {
{
spectrum[0*N+9] = temporal[0];
}
{
spectrum[1*N+9] = temporal[1];
}
{
spectrum[2*N+9] = temporal[2];
}
}
        
}
{

            vec2 temporal[T];
            {
{
temporal[0] = spectrum[0*N+10];
}
{
temporal[1] = spectrum[1*N+10];
}
{
temporal[2] = spectrum[2*N+10];
}
}
            dftTemporal(temporal);
            {
{
spectrum[0*N+10] = temporal[0];
}
{
spectrum[1*N+10] = temporal[1];
}
{
spectrum[2*N+10] = temporal[2];
}
}
        
}
{

            vec2 temporal[T];
            {
{
temporal[0] = spectrum[0*N+11];
}
{
temporal[1] = spectrum[1*N+11];
}
{
temporal[2] = spectrum[2*N+11];
}
}
            dftTemporal(temporal);
            {
{
spectrum[0*N+11] = temporal[0];
}
{
spectrum[1*N+11] = temporal[1];
}
{
spectrum[2*N+11] = temporal[2];
}
}
        
}
{

            vec2 temporal[T];
            {
{
temporal[0] = spectrum[0*N+12];
}
{
temporal[1] = spectrum[1*N+12];
}
{
temporal[2] = spectrum[2*N+12];
}
}
            dftTemporal(temporal);
            {
{
spectrum[0*N+12] = temporal[0];
}
{
spectrum[1*N+12] = temporal[1];
}
{
spectrum[2*N+12] = temporal[2];
}
}
        
}
{

            vec2 temporal[T];
            {
{
temporal[0] = spectrum[0*N+13];
}
{
temporal[1] = spectrum[1*N+13];
}
{
temporal[2] = spectrum[2*N+13];
}
}
            dftTemporal(temporal);
            {
{
spectrum[0*N+13] = temporal[0];
}
{
spectrum[1*N+13] = temporal[1];
}
{
spectrum[2*N+13] = temporal[2];
}
}
        
}
{

            vec2 temporal[T];
            {
{
temporal[0] = spectrum[0*N+14];
}
{
temporal[1] = spectrum[1*N+14];
}
{
temporal[2] = spectrum[2*N+14];
}
}
            dftTemporal(temporal);
            {
{
spectrum[0*N+14] = temporal[0];
}
{
spectrum[1*N+14] = temporal[1];
}
{
spectrum[2*N+14] = temporal[2];
}
}
        
}
{

            vec2 temporal[T];
            {
{
temporal[0] = spectrum[0*N+15];
}
{
temporal[1] = spectrum[1*N+15];
}
{
temporal[2] = spectrum[2*N+15];
}
}
            dftTemporal(temporal);
            {
{
spectrum[0*N+15] = temporal[0];
}
{
spectrum[1*N+15] = temporal[1];
}
{
spectrum[2*N+15] = temporal[2];
}
}
        
}
}
    }
#if ZMEAN
    if (lane == 0) blockMean[tile] = spectrum[0].x / windowSpectrum(0,0).x;
    TILE_BARRIER();
#endif
    vec2 column[N];
    if (lane <= N/2) {
        {
{

            vec2 temporal[T];
            {
{

                int frequency = (0 * N + 0) * (N / 2 + 1) + lane;
                vec2 v = spectrum[0*N+0];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(0*N+0,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[0] = v;
            
}
{

                int frequency = (1 * N + 0) * (N / 2 + 1) + lane;
                vec2 v = spectrum[1*N+0];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(1*N+0,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[1] = v;
            
}
{

                int frequency = (2 * N + 0) * (N / 2 + 1) + lane;
                vec2 v = spectrum[2*N+0];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(2*N+0,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[2] = v;
            
}
}
            column[0] = inverseTemporalCenter(temporal);
        
}
{

            vec2 temporal[T];
            {
{

                int frequency = (0 * N + 1) * (N / 2 + 1) + lane;
                vec2 v = spectrum[0*N+1];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(0*N+1,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[0] = v;
            
}
{

                int frequency = (1 * N + 1) * (N / 2 + 1) + lane;
                vec2 v = spectrum[1*N+1];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(1*N+1,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[1] = v;
            
}
{

                int frequency = (2 * N + 1) * (N / 2 + 1) + lane;
                vec2 v = spectrum[2*N+1];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(2*N+1,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[2] = v;
            
}
}
            column[1] = inverseTemporalCenter(temporal);
        
}
{

            vec2 temporal[T];
            {
{

                int frequency = (0 * N + 2) * (N / 2 + 1) + lane;
                vec2 v = spectrum[0*N+2];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(0*N+2,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[0] = v;
            
}
{

                int frequency = (1 * N + 2) * (N / 2 + 1) + lane;
                vec2 v = spectrum[1*N+2];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(1*N+2,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[1] = v;
            
}
{

                int frequency = (2 * N + 2) * (N / 2 + 1) + lane;
                vec2 v = spectrum[2*N+2];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(2*N+2,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[2] = v;
            
}
}
            column[2] = inverseTemporalCenter(temporal);
        
}
{

            vec2 temporal[T];
            {
{

                int frequency = (0 * N + 3) * (N / 2 + 1) + lane;
                vec2 v = spectrum[0*N+3];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(0*N+3,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[0] = v;
            
}
{

                int frequency = (1 * N + 3) * (N / 2 + 1) + lane;
                vec2 v = spectrum[1*N+3];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(1*N+3,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[1] = v;
            
}
{

                int frequency = (2 * N + 3) * (N / 2 + 1) + lane;
                vec2 v = spectrum[2*N+3];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(2*N+3,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[2] = v;
            
}
}
            column[3] = inverseTemporalCenter(temporal);
        
}
{

            vec2 temporal[T];
            {
{

                int frequency = (0 * N + 4) * (N / 2 + 1) + lane;
                vec2 v = spectrum[0*N+4];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(0*N+4,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[0] = v;
            
}
{

                int frequency = (1 * N + 4) * (N / 2 + 1) + lane;
                vec2 v = spectrum[1*N+4];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(1*N+4,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[1] = v;
            
}
{

                int frequency = (2 * N + 4) * (N / 2 + 1) + lane;
                vec2 v = spectrum[2*N+4];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(2*N+4,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[2] = v;
            
}
}
            column[4] = inverseTemporalCenter(temporal);
        
}
{

            vec2 temporal[T];
            {
{

                int frequency = (0 * N + 5) * (N / 2 + 1) + lane;
                vec2 v = spectrum[0*N+5];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(0*N+5,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[0] = v;
            
}
{

                int frequency = (1 * N + 5) * (N / 2 + 1) + lane;
                vec2 v = spectrum[1*N+5];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(1*N+5,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[1] = v;
            
}
{

                int frequency = (2 * N + 5) * (N / 2 + 1) + lane;
                vec2 v = spectrum[2*N+5];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(2*N+5,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[2] = v;
            
}
}
            column[5] = inverseTemporalCenter(temporal);
        
}
{

            vec2 temporal[T];
            {
{

                int frequency = (0 * N + 6) * (N / 2 + 1) + lane;
                vec2 v = spectrum[0*N+6];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(0*N+6,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[0] = v;
            
}
{

                int frequency = (1 * N + 6) * (N / 2 + 1) + lane;
                vec2 v = spectrum[1*N+6];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(1*N+6,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[1] = v;
            
}
{

                int frequency = (2 * N + 6) * (N / 2 + 1) + lane;
                vec2 v = spectrum[2*N+6];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(2*N+6,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[2] = v;
            
}
}
            column[6] = inverseTemporalCenter(temporal);
        
}
{

            vec2 temporal[T];
            {
{

                int frequency = (0 * N + 7) * (N / 2 + 1) + lane;
                vec2 v = spectrum[0*N+7];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(0*N+7,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[0] = v;
            
}
{

                int frequency = (1 * N + 7) * (N / 2 + 1) + lane;
                vec2 v = spectrum[1*N+7];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(1*N+7,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[1] = v;
            
}
{

                int frequency = (2 * N + 7) * (N / 2 + 1) + lane;
                vec2 v = spectrum[2*N+7];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(2*N+7,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[2] = v;
            
}
}
            column[7] = inverseTemporalCenter(temporal);
        
}
{

            vec2 temporal[T];
            {
{

                int frequency = (0 * N + 8) * (N / 2 + 1) + lane;
                vec2 v = spectrum[0*N+8];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(0*N+8,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[0] = v;
            
}
{

                int frequency = (1 * N + 8) * (N / 2 + 1) + lane;
                vec2 v = spectrum[1*N+8];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(1*N+8,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[1] = v;
            
}
{

                int frequency = (2 * N + 8) * (N / 2 + 1) + lane;
                vec2 v = spectrum[2*N+8];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(2*N+8,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[2] = v;
            
}
}
            column[8] = inverseTemporalCenter(temporal);
        
}
{

            vec2 temporal[T];
            {
{

                int frequency = (0 * N + 9) * (N / 2 + 1) + lane;
                vec2 v = spectrum[0*N+9];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(0*N+9,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[0] = v;
            
}
{

                int frequency = (1 * N + 9) * (N / 2 + 1) + lane;
                vec2 v = spectrum[1*N+9];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(1*N+9,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[1] = v;
            
}
{

                int frequency = (2 * N + 9) * (N / 2 + 1) + lane;
                vec2 v = spectrum[2*N+9];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(2*N+9,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[2] = v;
            
}
}
            column[9] = inverseTemporalCenter(temporal);
        
}
{

            vec2 temporal[T];
            {
{

                int frequency = (0 * N + 10) * (N / 2 + 1) + lane;
                vec2 v = spectrum[0*N+10];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(0*N+10,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[0] = v;
            
}
{

                int frequency = (1 * N + 10) * (N / 2 + 1) + lane;
                vec2 v = spectrum[1*N+10];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(1*N+10,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[1] = v;
            
}
{

                int frequency = (2 * N + 10) * (N / 2 + 1) + lane;
                vec2 v = spectrum[2*N+10];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(2*N+10,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[2] = v;
            
}
}
            column[10] = inverseTemporalCenter(temporal);
        
}
{

            vec2 temporal[T];
            {
{

                int frequency = (0 * N + 11) * (N / 2 + 1) + lane;
                vec2 v = spectrum[0*N+11];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(0*N+11,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[0] = v;
            
}
{

                int frequency = (1 * N + 11) * (N / 2 + 1) + lane;
                vec2 v = spectrum[1*N+11];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(1*N+11,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[1] = v;
            
}
{

                int frequency = (2 * N + 11) * (N / 2 + 1) + lane;
                vec2 v = spectrum[2*N+11];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(2*N+11,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[2] = v;
            
}
}
            column[11] = inverseTemporalCenter(temporal);
        
}
{

            vec2 temporal[T];
            {
{

                int frequency = (0 * N + 12) * (N / 2 + 1) + lane;
                vec2 v = spectrum[0*N+12];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(0*N+12,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[0] = v;
            
}
{

                int frequency = (1 * N + 12) * (N / 2 + 1) + lane;
                vec2 v = spectrum[1*N+12];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(1*N+12,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[1] = v;
            
}
{

                int frequency = (2 * N + 12) * (N / 2 + 1) + lane;
                vec2 v = spectrum[2*N+12];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(2*N+12,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[2] = v;
            
}
}
            column[12] = inverseTemporalCenter(temporal);
        
}
{

            vec2 temporal[T];
            {
{

                int frequency = (0 * N + 13) * (N / 2 + 1) + lane;
                vec2 v = spectrum[0*N+13];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(0*N+13,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[0] = v;
            
}
{

                int frequency = (1 * N + 13) * (N / 2 + 1) + lane;
                vec2 v = spectrum[1*N+13];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(1*N+13,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[1] = v;
            
}
{

                int frequency = (2 * N + 13) * (N / 2 + 1) + lane;
                vec2 v = spectrum[2*N+13];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(2*N+13,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[2] = v;
            
}
}
            column[13] = inverseTemporalCenter(temporal);
        
}
{

            vec2 temporal[T];
            {
{

                int frequency = (0 * N + 14) * (N / 2 + 1) + lane;
                vec2 v = spectrum[0*N+14];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(0*N+14,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[0] = v;
            
}
{

                int frequency = (1 * N + 14) * (N / 2 + 1) + lane;
                vec2 v = spectrum[1*N+14];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(1*N+14,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[1] = v;
            
}
{

                int frequency = (2 * N + 14) * (N / 2 + 1) + lane;
                vec2 v = spectrum[2*N+14];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(2*N+14,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[2] = v;
            
}
}
            column[14] = inverseTemporalCenter(temporal);
        
}
{

            vec2 temporal[T];
            {
{

                int frequency = (0 * N + 15) * (N / 2 + 1) + lane;
                vec2 v = spectrum[0*N+15];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(0*N+15,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[0] = v;
            
}
{

                int frequency = (1 * N + 15) * (N / 2 + 1) + lane;
                vec2 v = spectrum[1*N+15];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(1*N+15,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[1] = v;
            
}
{

                int frequency = (2 * N + 15) * (N / 2 + 1) + lane;
                vec2 v = spectrum[2*N+15];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(2*N+15,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[2] = v;
            
}
}
            column[15] = inverseTemporalCenter(temporal);
        
}
}
        idft16(column);
        {
{
transposeStore(sharedBase + 0 * (N + 1) + lane,column[0]);
}
{
transposeStore(sharedBase + 1 * (N + 1) + lane,column[1]);
}
{
transposeStore(sharedBase + 2 * (N + 1) + lane,column[2]);
}
{
transposeStore(sharedBase + 3 * (N + 1) + lane,column[3]);
}
{
transposeStore(sharedBase + 4 * (N + 1) + lane,column[4]);
}
{
transposeStore(sharedBase + 5 * (N + 1) + lane,column[5]);
}
{
transposeStore(sharedBase + 6 * (N + 1) + lane,column[6]);
}
{
transposeStore(sharedBase + 7 * (N + 1) + lane,column[7]);
}
{
transposeStore(sharedBase + 8 * (N + 1) + lane,column[8]);
}
{
transposeStore(sharedBase + 9 * (N + 1) + lane,column[9]);
}
{
transposeStore(sharedBase + 10 * (N + 1) + lane,column[10]);
}
{
transposeStore(sharedBase + 11 * (N + 1) + lane,column[11]);
}
{
transposeStore(sharedBase + 12 * (N + 1) + lane,column[12]);
}
{
transposeStore(sharedBase + 13 * (N + 1) + lane,column[13]);
}
{
transposeStore(sharedBase + 14 * (N + 1) + lane,column[14]);
}
{
transposeStore(sharedBase + 15 * (N + 1) + lane,column[15]);
}
}
    }
    TILE_BARRIER();
    float realData[N + 2];
    {
{

        vec2 v = transposeLoad(sharedBase + lane * (N + 1) + 0);
        realData[2*0] = v.x;
        realData[2*0+1] = v.y;
    
}
{

        vec2 v = transposeLoad(sharedBase + lane * (N + 1) + 1);
        realData[2*1] = v.x;
        realData[2*1+1] = v.y;
    
}
{

        vec2 v = transposeLoad(sharedBase + lane * (N + 1) + 2);
        realData[2*2] = v.x;
        realData[2*2+1] = v.y;
    
}
{

        vec2 v = transposeLoad(sharedBase + lane * (N + 1) + 3);
        realData[2*3] = v.x;
        realData[2*3+1] = v.y;
    
}
{

        vec2 v = transposeLoad(sharedBase + lane * (N + 1) + 4);
        realData[2*4] = v.x;
        realData[2*4+1] = v.y;
    
}
{

        vec2 v = transposeLoad(sharedBase + lane * (N + 1) + 5);
        realData[2*5] = v.x;
        realData[2*5+1] = v.y;
    
}
{

        vec2 v = transposeLoad(sharedBase + lane * (N + 1) + 6);
        realData[2*6] = v.x;
        realData[2*6+1] = v.y;
    
}
{

        vec2 v = transposeLoad(sharedBase + lane * (N + 1) + 7);
        realData[2*7] = v.x;
        realData[2*7+1] = v.y;
    
}
{

        vec2 v = transposeLoad(sharedBase + lane * (N + 1) + 8);
        realData[2*8] = v.x;
        realData[2*8+1] = v.y;
    
}
}
    TILE_BARRIER();
    irdft16(realData);
    // FFTW's generated r2cb codelet uses the reverse real ordering.
    {
{
transposeStoreReal(sharedBase + 0 * (N + 1) + lane,realData[0 == 0 ? 0 : N-0]);
}
{
transposeStoreReal(sharedBase + 1 * (N + 1) + lane,realData[1 == 0 ? 0 : N-1]);
}
{
transposeStoreReal(sharedBase + 2 * (N + 1) + lane,realData[2 == 0 ? 0 : N-2]);
}
{
transposeStoreReal(sharedBase + 3 * (N + 1) + lane,realData[3 == 0 ? 0 : N-3]);
}
{
transposeStoreReal(sharedBase + 4 * (N + 1) + lane,realData[4 == 0 ? 0 : N-4]);
}
{
transposeStoreReal(sharedBase + 5 * (N + 1) + lane,realData[5 == 0 ? 0 : N-5]);
}
{
transposeStoreReal(sharedBase + 6 * (N + 1) + lane,realData[6 == 0 ? 0 : N-6]);
}
{
transposeStoreReal(sharedBase + 7 * (N + 1) + lane,realData[7 == 0 ? 0 : N-7]);
}
{
transposeStoreReal(sharedBase + 8 * (N + 1) + lane,realData[8 == 0 ? 0 : N-8]);
}
{
transposeStoreReal(sharedBase + 9 * (N + 1) + lane,realData[9 == 0 ? 0 : N-9]);
}
{
transposeStoreReal(sharedBase + 10 * (N + 1) + lane,realData[10 == 0 ? 0 : N-10]);
}
{
transposeStoreReal(sharedBase + 11 * (N + 1) + lane,realData[11 == 0 ? 0 : N-11]);
}
{
transposeStoreReal(sharedBase + 12 * (N + 1) + lane,realData[12 == 0 ? 0 : N-12]);
}
{
transposeStoreReal(sharedBase + 13 * (N + 1) + lane,realData[13 == 0 ? 0 : N-13]);
}
{
transposeStoreReal(sharedBase + 14 * (N + 1) + lane,realData[14 == 0 ? 0 : N-14]);
}
{
transposeStoreReal(sharedBase + 15 * (N + 1) + lane,realData[15 == 0 ? 0 : N-15]);
}
}
    TILE_BARRIER();
    if (validBlock)
        {
{
block[blockId*N*N + 0*N + lane] = transposeLoadReal(sharedBase + lane*(N+1)+0);
}
{
block[blockId*N*N + 1*N + lane] = transposeLoadReal(sharedBase + lane*(N+1)+1);
}
{
block[blockId*N*N + 2*N + lane] = transposeLoadReal(sharedBase + lane*(N+1)+2);
}
{
block[blockId*N*N + 3*N + lane] = transposeLoadReal(sharedBase + lane*(N+1)+3);
}
{
block[blockId*N*N + 4*N + lane] = transposeLoadReal(sharedBase + lane*(N+1)+4);
}
{
block[blockId*N*N + 5*N + lane] = transposeLoadReal(sharedBase + lane*(N+1)+5);
}
{
block[blockId*N*N + 6*N + lane] = transposeLoadReal(sharedBase + lane*(N+1)+6);
}
{
block[blockId*N*N + 7*N + lane] = transposeLoadReal(sharedBase + lane*(N+1)+7);
}
{
block[blockId*N*N + 8*N + lane] = transposeLoadReal(sharedBase + lane*(N+1)+8);
}
{
block[blockId*N*N + 9*N + lane] = transposeLoadReal(sharedBase + lane*(N+1)+9);
}
{
block[blockId*N*N + 10*N + lane] = transposeLoadReal(sharedBase + lane*(N+1)+10);
}
{
block[blockId*N*N + 11*N + lane] = transposeLoadReal(sharedBase + lane*(N+1)+11);
}
{
block[blockId*N*N + 12*N + lane] = transposeLoadReal(sharedBase + lane*(N+1)+12);
}
{
block[blockId*N*N + 13*N + lane] = transposeLoadReal(sharedBase + lane*(N+1)+13);
}
{
block[blockId*N*N + 14*N + lane] = transposeLoadReal(sharedBase + lane*(N+1)+14);
}
{
block[blockId*N*N + 15*N + lane] = transposeLoadReal(sharedBase + lane*(N+1)+15);
}
}
}
#endif
#if T == 5
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
    {
{
result += vec2(value[0].x * phase[0].x - value[0].y * phase[0].y,
                       value[0].x * phase[0].y + value[0].y * phase[0].x);
}
{
result += vec2(value[1].x * phase[1].x - value[1].y * phase[1].y,
                       value[1].x * phase[1].y + value[1].y * phase[1].x);
}
{
result += vec2(value[2].x * phase[2].x - value[2].y * phase[2].y,
                       value[2].x * phase[2].y + value[2].y * phase[2].x);
}
{
result += vec2(value[3].x * phase[3].x - value[3].y * phase[3].y,
                       value[3].x * phase[3].y + value[3].y * phase[3].x);
}
{
result += vec2(value[4].x * phase[4].x - value[4].y * phase[4].y,
                       value[4].x * phase[4].y + value[4].y * phase[4].x);
}
}
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


    if (lane <= N/2) {
        {
{

            {
{

                spectrum[0*N+0] = validBlock ? readCachedSpectrum(0,blockId,0,lane)*temporalFactors[0] : vec2(0.0);
            
}
{

                spectrum[0*N+1] = validBlock ? readCachedSpectrum(0,blockId,1,lane)*temporalFactors[0] : vec2(0.0);
            
}
{

                spectrum[0*N+2] = validBlock ? readCachedSpectrum(0,blockId,2,lane)*temporalFactors[0] : vec2(0.0);
            
}
{

                spectrum[0*N+3] = validBlock ? readCachedSpectrum(0,blockId,3,lane)*temporalFactors[0] : vec2(0.0);
            
}
{

                spectrum[0*N+4] = validBlock ? readCachedSpectrum(0,blockId,4,lane)*temporalFactors[0] : vec2(0.0);
            
}
{

                spectrum[0*N+5] = validBlock ? readCachedSpectrum(0,blockId,5,lane)*temporalFactors[0] : vec2(0.0);
            
}
{

                spectrum[0*N+6] = validBlock ? readCachedSpectrum(0,blockId,6,lane)*temporalFactors[0] : vec2(0.0);
            
}
{

                spectrum[0*N+7] = validBlock ? readCachedSpectrum(0,blockId,7,lane)*temporalFactors[0] : vec2(0.0);
            
}
{

                spectrum[0*N+8] = validBlock ? readCachedSpectrum(0,blockId,8,lane)*temporalFactors[0] : vec2(0.0);
            
}
{

                spectrum[0*N+9] = validBlock ? readCachedSpectrum(0,blockId,9,lane)*temporalFactors[0] : vec2(0.0);
            
}
{

                spectrum[0*N+10] = validBlock ? readCachedSpectrum(0,blockId,10,lane)*temporalFactors[0] : vec2(0.0);
            
}
{

                spectrum[0*N+11] = validBlock ? readCachedSpectrum(0,blockId,11,lane)*temporalFactors[0] : vec2(0.0);
            
}
{

                spectrum[0*N+12] = validBlock ? readCachedSpectrum(0,blockId,12,lane)*temporalFactors[0] : vec2(0.0);
            
}
{

                spectrum[0*N+13] = validBlock ? readCachedSpectrum(0,blockId,13,lane)*temporalFactors[0] : vec2(0.0);
            
}
{

                spectrum[0*N+14] = validBlock ? readCachedSpectrum(0,blockId,14,lane)*temporalFactors[0] : vec2(0.0);
            
}
{

                spectrum[0*N+15] = validBlock ? readCachedSpectrum(0,blockId,15,lane)*temporalFactors[0] : vec2(0.0);
            
}
}
        
}
{

            {
{

                spectrum[1*N+0] = validBlock ? readCachedSpectrum(1,blockId,0,lane)*temporalFactors[1] : vec2(0.0);
            
}
{

                spectrum[1*N+1] = validBlock ? readCachedSpectrum(1,blockId,1,lane)*temporalFactors[1] : vec2(0.0);
            
}
{

                spectrum[1*N+2] = validBlock ? readCachedSpectrum(1,blockId,2,lane)*temporalFactors[1] : vec2(0.0);
            
}
{

                spectrum[1*N+3] = validBlock ? readCachedSpectrum(1,blockId,3,lane)*temporalFactors[1] : vec2(0.0);
            
}
{

                spectrum[1*N+4] = validBlock ? readCachedSpectrum(1,blockId,4,lane)*temporalFactors[1] : vec2(0.0);
            
}
{

                spectrum[1*N+5] = validBlock ? readCachedSpectrum(1,blockId,5,lane)*temporalFactors[1] : vec2(0.0);
            
}
{

                spectrum[1*N+6] = validBlock ? readCachedSpectrum(1,blockId,6,lane)*temporalFactors[1] : vec2(0.0);
            
}
{

                spectrum[1*N+7] = validBlock ? readCachedSpectrum(1,blockId,7,lane)*temporalFactors[1] : vec2(0.0);
            
}
{

                spectrum[1*N+8] = validBlock ? readCachedSpectrum(1,blockId,8,lane)*temporalFactors[1] : vec2(0.0);
            
}
{

                spectrum[1*N+9] = validBlock ? readCachedSpectrum(1,blockId,9,lane)*temporalFactors[1] : vec2(0.0);
            
}
{

                spectrum[1*N+10] = validBlock ? readCachedSpectrum(1,blockId,10,lane)*temporalFactors[1] : vec2(0.0);
            
}
{

                spectrum[1*N+11] = validBlock ? readCachedSpectrum(1,blockId,11,lane)*temporalFactors[1] : vec2(0.0);
            
}
{

                spectrum[1*N+12] = validBlock ? readCachedSpectrum(1,blockId,12,lane)*temporalFactors[1] : vec2(0.0);
            
}
{

                spectrum[1*N+13] = validBlock ? readCachedSpectrum(1,blockId,13,lane)*temporalFactors[1] : vec2(0.0);
            
}
{

                spectrum[1*N+14] = validBlock ? readCachedSpectrum(1,blockId,14,lane)*temporalFactors[1] : vec2(0.0);
            
}
{

                spectrum[1*N+15] = validBlock ? readCachedSpectrum(1,blockId,15,lane)*temporalFactors[1] : vec2(0.0);
            
}
}
        
}
{

            {
{

                spectrum[2*N+0] = validBlock ? readCachedSpectrum(2,blockId,0,lane)*temporalFactors[2] : vec2(0.0);
            
}
{

                spectrum[2*N+1] = validBlock ? readCachedSpectrum(2,blockId,1,lane)*temporalFactors[2] : vec2(0.0);
            
}
{

                spectrum[2*N+2] = validBlock ? readCachedSpectrum(2,blockId,2,lane)*temporalFactors[2] : vec2(0.0);
            
}
{

                spectrum[2*N+3] = validBlock ? readCachedSpectrum(2,blockId,3,lane)*temporalFactors[2] : vec2(0.0);
            
}
{

                spectrum[2*N+4] = validBlock ? readCachedSpectrum(2,blockId,4,lane)*temporalFactors[2] : vec2(0.0);
            
}
{

                spectrum[2*N+5] = validBlock ? readCachedSpectrum(2,blockId,5,lane)*temporalFactors[2] : vec2(0.0);
            
}
{

                spectrum[2*N+6] = validBlock ? readCachedSpectrum(2,blockId,6,lane)*temporalFactors[2] : vec2(0.0);
            
}
{

                spectrum[2*N+7] = validBlock ? readCachedSpectrum(2,blockId,7,lane)*temporalFactors[2] : vec2(0.0);
            
}
{

                spectrum[2*N+8] = validBlock ? readCachedSpectrum(2,blockId,8,lane)*temporalFactors[2] : vec2(0.0);
            
}
{

                spectrum[2*N+9] = validBlock ? readCachedSpectrum(2,blockId,9,lane)*temporalFactors[2] : vec2(0.0);
            
}
{

                spectrum[2*N+10] = validBlock ? readCachedSpectrum(2,blockId,10,lane)*temporalFactors[2] : vec2(0.0);
            
}
{

                spectrum[2*N+11] = validBlock ? readCachedSpectrum(2,blockId,11,lane)*temporalFactors[2] : vec2(0.0);
            
}
{

                spectrum[2*N+12] = validBlock ? readCachedSpectrum(2,blockId,12,lane)*temporalFactors[2] : vec2(0.0);
            
}
{

                spectrum[2*N+13] = validBlock ? readCachedSpectrum(2,blockId,13,lane)*temporalFactors[2] : vec2(0.0);
            
}
{

                spectrum[2*N+14] = validBlock ? readCachedSpectrum(2,blockId,14,lane)*temporalFactors[2] : vec2(0.0);
            
}
{

                spectrum[2*N+15] = validBlock ? readCachedSpectrum(2,blockId,15,lane)*temporalFactors[2] : vec2(0.0);
            
}
}
        
}
{

            {
{

                spectrum[3*N+0] = validBlock ? readCachedSpectrum(3,blockId,0,lane)*temporalFactors[3] : vec2(0.0);
            
}
{

                spectrum[3*N+1] = validBlock ? readCachedSpectrum(3,blockId,1,lane)*temporalFactors[3] : vec2(0.0);
            
}
{

                spectrum[3*N+2] = validBlock ? readCachedSpectrum(3,blockId,2,lane)*temporalFactors[3] : vec2(0.0);
            
}
{

                spectrum[3*N+3] = validBlock ? readCachedSpectrum(3,blockId,3,lane)*temporalFactors[3] : vec2(0.0);
            
}
{

                spectrum[3*N+4] = validBlock ? readCachedSpectrum(3,blockId,4,lane)*temporalFactors[3] : vec2(0.0);
            
}
{

                spectrum[3*N+5] = validBlock ? readCachedSpectrum(3,blockId,5,lane)*temporalFactors[3] : vec2(0.0);
            
}
{

                spectrum[3*N+6] = validBlock ? readCachedSpectrum(3,blockId,6,lane)*temporalFactors[3] : vec2(0.0);
            
}
{

                spectrum[3*N+7] = validBlock ? readCachedSpectrum(3,blockId,7,lane)*temporalFactors[3] : vec2(0.0);
            
}
{

                spectrum[3*N+8] = validBlock ? readCachedSpectrum(3,blockId,8,lane)*temporalFactors[3] : vec2(0.0);
            
}
{

                spectrum[3*N+9] = validBlock ? readCachedSpectrum(3,blockId,9,lane)*temporalFactors[3] : vec2(0.0);
            
}
{

                spectrum[3*N+10] = validBlock ? readCachedSpectrum(3,blockId,10,lane)*temporalFactors[3] : vec2(0.0);
            
}
{

                spectrum[3*N+11] = validBlock ? readCachedSpectrum(3,blockId,11,lane)*temporalFactors[3] : vec2(0.0);
            
}
{

                spectrum[3*N+12] = validBlock ? readCachedSpectrum(3,blockId,12,lane)*temporalFactors[3] : vec2(0.0);
            
}
{

                spectrum[3*N+13] = validBlock ? readCachedSpectrum(3,blockId,13,lane)*temporalFactors[3] : vec2(0.0);
            
}
{

                spectrum[3*N+14] = validBlock ? readCachedSpectrum(3,blockId,14,lane)*temporalFactors[3] : vec2(0.0);
            
}
{

                spectrum[3*N+15] = validBlock ? readCachedSpectrum(3,blockId,15,lane)*temporalFactors[3] : vec2(0.0);
            
}
}
        
}
{

            {
{

                spectrum[4*N+0] = validBlock ? readCachedSpectrum(4,blockId,0,lane)*temporalFactors[4] : vec2(0.0);
            
}
{

                spectrum[4*N+1] = validBlock ? readCachedSpectrum(4,blockId,1,lane)*temporalFactors[4] : vec2(0.0);
            
}
{

                spectrum[4*N+2] = validBlock ? readCachedSpectrum(4,blockId,2,lane)*temporalFactors[4] : vec2(0.0);
            
}
{

                spectrum[4*N+3] = validBlock ? readCachedSpectrum(4,blockId,3,lane)*temporalFactors[4] : vec2(0.0);
            
}
{

                spectrum[4*N+4] = validBlock ? readCachedSpectrum(4,blockId,4,lane)*temporalFactors[4] : vec2(0.0);
            
}
{

                spectrum[4*N+5] = validBlock ? readCachedSpectrum(4,blockId,5,lane)*temporalFactors[4] : vec2(0.0);
            
}
{

                spectrum[4*N+6] = validBlock ? readCachedSpectrum(4,blockId,6,lane)*temporalFactors[4] : vec2(0.0);
            
}
{

                spectrum[4*N+7] = validBlock ? readCachedSpectrum(4,blockId,7,lane)*temporalFactors[4] : vec2(0.0);
            
}
{

                spectrum[4*N+8] = validBlock ? readCachedSpectrum(4,blockId,8,lane)*temporalFactors[4] : vec2(0.0);
            
}
{

                spectrum[4*N+9] = validBlock ? readCachedSpectrum(4,blockId,9,lane)*temporalFactors[4] : vec2(0.0);
            
}
{

                spectrum[4*N+10] = validBlock ? readCachedSpectrum(4,blockId,10,lane)*temporalFactors[4] : vec2(0.0);
            
}
{

                spectrum[4*N+11] = validBlock ? readCachedSpectrum(4,blockId,11,lane)*temporalFactors[4] : vec2(0.0);
            
}
{

                spectrum[4*N+12] = validBlock ? readCachedSpectrum(4,blockId,12,lane)*temporalFactors[4] : vec2(0.0);
            
}
{

                spectrum[4*N+13] = validBlock ? readCachedSpectrum(4,blockId,13,lane)*temporalFactors[4] : vec2(0.0);
            
}
{

                spectrum[4*N+14] = validBlock ? readCachedSpectrum(4,blockId,14,lane)*temporalFactors[4] : vec2(0.0);
            
}
{

                spectrum[4*N+15] = validBlock ? readCachedSpectrum(4,blockId,15,lane)*temporalFactors[4] : vec2(0.0);
            
}
}
        
}
}
    }
    if (lane <= N/2) {
        {
{

            vec2 temporal[T];
            {
{
temporal[0] = spectrum[0*N+0];
}
{
temporal[1] = spectrum[1*N+0];
}
{
temporal[2] = spectrum[2*N+0];
}
{
temporal[3] = spectrum[3*N+0];
}
{
temporal[4] = spectrum[4*N+0];
}
}
            dftTemporal(temporal);
            {
{
spectrum[0*N+0] = temporal[0];
}
{
spectrum[1*N+0] = temporal[1];
}
{
spectrum[2*N+0] = temporal[2];
}
{
spectrum[3*N+0] = temporal[3];
}
{
spectrum[4*N+0] = temporal[4];
}
}
        
}
{

            vec2 temporal[T];
            {
{
temporal[0] = spectrum[0*N+1];
}
{
temporal[1] = spectrum[1*N+1];
}
{
temporal[2] = spectrum[2*N+1];
}
{
temporal[3] = spectrum[3*N+1];
}
{
temporal[4] = spectrum[4*N+1];
}
}
            dftTemporal(temporal);
            {
{
spectrum[0*N+1] = temporal[0];
}
{
spectrum[1*N+1] = temporal[1];
}
{
spectrum[2*N+1] = temporal[2];
}
{
spectrum[3*N+1] = temporal[3];
}
{
spectrum[4*N+1] = temporal[4];
}
}
        
}
{

            vec2 temporal[T];
            {
{
temporal[0] = spectrum[0*N+2];
}
{
temporal[1] = spectrum[1*N+2];
}
{
temporal[2] = spectrum[2*N+2];
}
{
temporal[3] = spectrum[3*N+2];
}
{
temporal[4] = spectrum[4*N+2];
}
}
            dftTemporal(temporal);
            {
{
spectrum[0*N+2] = temporal[0];
}
{
spectrum[1*N+2] = temporal[1];
}
{
spectrum[2*N+2] = temporal[2];
}
{
spectrum[3*N+2] = temporal[3];
}
{
spectrum[4*N+2] = temporal[4];
}
}
        
}
{

            vec2 temporal[T];
            {
{
temporal[0] = spectrum[0*N+3];
}
{
temporal[1] = spectrum[1*N+3];
}
{
temporal[2] = spectrum[2*N+3];
}
{
temporal[3] = spectrum[3*N+3];
}
{
temporal[4] = spectrum[4*N+3];
}
}
            dftTemporal(temporal);
            {
{
spectrum[0*N+3] = temporal[0];
}
{
spectrum[1*N+3] = temporal[1];
}
{
spectrum[2*N+3] = temporal[2];
}
{
spectrum[3*N+3] = temporal[3];
}
{
spectrum[4*N+3] = temporal[4];
}
}
        
}
{

            vec2 temporal[T];
            {
{
temporal[0] = spectrum[0*N+4];
}
{
temporal[1] = spectrum[1*N+4];
}
{
temporal[2] = spectrum[2*N+4];
}
{
temporal[3] = spectrum[3*N+4];
}
{
temporal[4] = spectrum[4*N+4];
}
}
            dftTemporal(temporal);
            {
{
spectrum[0*N+4] = temporal[0];
}
{
spectrum[1*N+4] = temporal[1];
}
{
spectrum[2*N+4] = temporal[2];
}
{
spectrum[3*N+4] = temporal[3];
}
{
spectrum[4*N+4] = temporal[4];
}
}
        
}
{

            vec2 temporal[T];
            {
{
temporal[0] = spectrum[0*N+5];
}
{
temporal[1] = spectrum[1*N+5];
}
{
temporal[2] = spectrum[2*N+5];
}
{
temporal[3] = spectrum[3*N+5];
}
{
temporal[4] = spectrum[4*N+5];
}
}
            dftTemporal(temporal);
            {
{
spectrum[0*N+5] = temporal[0];
}
{
spectrum[1*N+5] = temporal[1];
}
{
spectrum[2*N+5] = temporal[2];
}
{
spectrum[3*N+5] = temporal[3];
}
{
spectrum[4*N+5] = temporal[4];
}
}
        
}
{

            vec2 temporal[T];
            {
{
temporal[0] = spectrum[0*N+6];
}
{
temporal[1] = spectrum[1*N+6];
}
{
temporal[2] = spectrum[2*N+6];
}
{
temporal[3] = spectrum[3*N+6];
}
{
temporal[4] = spectrum[4*N+6];
}
}
            dftTemporal(temporal);
            {
{
spectrum[0*N+6] = temporal[0];
}
{
spectrum[1*N+6] = temporal[1];
}
{
spectrum[2*N+6] = temporal[2];
}
{
spectrum[3*N+6] = temporal[3];
}
{
spectrum[4*N+6] = temporal[4];
}
}
        
}
{

            vec2 temporal[T];
            {
{
temporal[0] = spectrum[0*N+7];
}
{
temporal[1] = spectrum[1*N+7];
}
{
temporal[2] = spectrum[2*N+7];
}
{
temporal[3] = spectrum[3*N+7];
}
{
temporal[4] = spectrum[4*N+7];
}
}
            dftTemporal(temporal);
            {
{
spectrum[0*N+7] = temporal[0];
}
{
spectrum[1*N+7] = temporal[1];
}
{
spectrum[2*N+7] = temporal[2];
}
{
spectrum[3*N+7] = temporal[3];
}
{
spectrum[4*N+7] = temporal[4];
}
}
        
}
{

            vec2 temporal[T];
            {
{
temporal[0] = spectrum[0*N+8];
}
{
temporal[1] = spectrum[1*N+8];
}
{
temporal[2] = spectrum[2*N+8];
}
{
temporal[3] = spectrum[3*N+8];
}
{
temporal[4] = spectrum[4*N+8];
}
}
            dftTemporal(temporal);
            {
{
spectrum[0*N+8] = temporal[0];
}
{
spectrum[1*N+8] = temporal[1];
}
{
spectrum[2*N+8] = temporal[2];
}
{
spectrum[3*N+8] = temporal[3];
}
{
spectrum[4*N+8] = temporal[4];
}
}
        
}
{

            vec2 temporal[T];
            {
{
temporal[0] = spectrum[0*N+9];
}
{
temporal[1] = spectrum[1*N+9];
}
{
temporal[2] = spectrum[2*N+9];
}
{
temporal[3] = spectrum[3*N+9];
}
{
temporal[4] = spectrum[4*N+9];
}
}
            dftTemporal(temporal);
            {
{
spectrum[0*N+9] = temporal[0];
}
{
spectrum[1*N+9] = temporal[1];
}
{
spectrum[2*N+9] = temporal[2];
}
{
spectrum[3*N+9] = temporal[3];
}
{
spectrum[4*N+9] = temporal[4];
}
}
        
}
{

            vec2 temporal[T];
            {
{
temporal[0] = spectrum[0*N+10];
}
{
temporal[1] = spectrum[1*N+10];
}
{
temporal[2] = spectrum[2*N+10];
}
{
temporal[3] = spectrum[3*N+10];
}
{
temporal[4] = spectrum[4*N+10];
}
}
            dftTemporal(temporal);
            {
{
spectrum[0*N+10] = temporal[0];
}
{
spectrum[1*N+10] = temporal[1];
}
{
spectrum[2*N+10] = temporal[2];
}
{
spectrum[3*N+10] = temporal[3];
}
{
spectrum[4*N+10] = temporal[4];
}
}
        
}
{

            vec2 temporal[T];
            {
{
temporal[0] = spectrum[0*N+11];
}
{
temporal[1] = spectrum[1*N+11];
}
{
temporal[2] = spectrum[2*N+11];
}
{
temporal[3] = spectrum[3*N+11];
}
{
temporal[4] = spectrum[4*N+11];
}
}
            dftTemporal(temporal);
            {
{
spectrum[0*N+11] = temporal[0];
}
{
spectrum[1*N+11] = temporal[1];
}
{
spectrum[2*N+11] = temporal[2];
}
{
spectrum[3*N+11] = temporal[3];
}
{
spectrum[4*N+11] = temporal[4];
}
}
        
}
{

            vec2 temporal[T];
            {
{
temporal[0] = spectrum[0*N+12];
}
{
temporal[1] = spectrum[1*N+12];
}
{
temporal[2] = spectrum[2*N+12];
}
{
temporal[3] = spectrum[3*N+12];
}
{
temporal[4] = spectrum[4*N+12];
}
}
            dftTemporal(temporal);
            {
{
spectrum[0*N+12] = temporal[0];
}
{
spectrum[1*N+12] = temporal[1];
}
{
spectrum[2*N+12] = temporal[2];
}
{
spectrum[3*N+12] = temporal[3];
}
{
spectrum[4*N+12] = temporal[4];
}
}
        
}
{

            vec2 temporal[T];
            {
{
temporal[0] = spectrum[0*N+13];
}
{
temporal[1] = spectrum[1*N+13];
}
{
temporal[2] = spectrum[2*N+13];
}
{
temporal[3] = spectrum[3*N+13];
}
{
temporal[4] = spectrum[4*N+13];
}
}
            dftTemporal(temporal);
            {
{
spectrum[0*N+13] = temporal[0];
}
{
spectrum[1*N+13] = temporal[1];
}
{
spectrum[2*N+13] = temporal[2];
}
{
spectrum[3*N+13] = temporal[3];
}
{
spectrum[4*N+13] = temporal[4];
}
}
        
}
{

            vec2 temporal[T];
            {
{
temporal[0] = spectrum[0*N+14];
}
{
temporal[1] = spectrum[1*N+14];
}
{
temporal[2] = spectrum[2*N+14];
}
{
temporal[3] = spectrum[3*N+14];
}
{
temporal[4] = spectrum[4*N+14];
}
}
            dftTemporal(temporal);
            {
{
spectrum[0*N+14] = temporal[0];
}
{
spectrum[1*N+14] = temporal[1];
}
{
spectrum[2*N+14] = temporal[2];
}
{
spectrum[3*N+14] = temporal[3];
}
{
spectrum[4*N+14] = temporal[4];
}
}
        
}
{

            vec2 temporal[T];
            {
{
temporal[0] = spectrum[0*N+15];
}
{
temporal[1] = spectrum[1*N+15];
}
{
temporal[2] = spectrum[2*N+15];
}
{
temporal[3] = spectrum[3*N+15];
}
{
temporal[4] = spectrum[4*N+15];
}
}
            dftTemporal(temporal);
            {
{
spectrum[0*N+15] = temporal[0];
}
{
spectrum[1*N+15] = temporal[1];
}
{
spectrum[2*N+15] = temporal[2];
}
{
spectrum[3*N+15] = temporal[3];
}
{
spectrum[4*N+15] = temporal[4];
}
}
        
}
}
    }
#if ZMEAN
    if (lane == 0) blockMean[tile] = spectrum[0].x / windowSpectrum(0,0).x;
    TILE_BARRIER();
#endif
    vec2 column[N];
    if (lane <= N/2) {
        {
{

            vec2 temporal[T];
            {
{

                int frequency = (0 * N + 0) * (N / 2 + 1) + lane;
                vec2 v = spectrum[0*N+0];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(0*N+0,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[0] = v;
            
}
{

                int frequency = (1 * N + 0) * (N / 2 + 1) + lane;
                vec2 v = spectrum[1*N+0];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(1*N+0,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[1] = v;
            
}
{

                int frequency = (2 * N + 0) * (N / 2 + 1) + lane;
                vec2 v = spectrum[2*N+0];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(2*N+0,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[2] = v;
            
}
{

                int frequency = (3 * N + 0) * (N / 2 + 1) + lane;
                vec2 v = spectrum[3*N+0];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(3*N+0,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[3] = v;
            
}
{

                int frequency = (4 * N + 0) * (N / 2 + 1) + lane;
                vec2 v = spectrum[4*N+0];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(4*N+0,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[4] = v;
            
}
}
            column[0] = inverseTemporalCenter(temporal);
        
}
{

            vec2 temporal[T];
            {
{

                int frequency = (0 * N + 1) * (N / 2 + 1) + lane;
                vec2 v = spectrum[0*N+1];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(0*N+1,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[0] = v;
            
}
{

                int frequency = (1 * N + 1) * (N / 2 + 1) + lane;
                vec2 v = spectrum[1*N+1];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(1*N+1,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[1] = v;
            
}
{

                int frequency = (2 * N + 1) * (N / 2 + 1) + lane;
                vec2 v = spectrum[2*N+1];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(2*N+1,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[2] = v;
            
}
{

                int frequency = (3 * N + 1) * (N / 2 + 1) + lane;
                vec2 v = spectrum[3*N+1];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(3*N+1,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[3] = v;
            
}
{

                int frequency = (4 * N + 1) * (N / 2 + 1) + lane;
                vec2 v = spectrum[4*N+1];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(4*N+1,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[4] = v;
            
}
}
            column[1] = inverseTemporalCenter(temporal);
        
}
{

            vec2 temporal[T];
            {
{

                int frequency = (0 * N + 2) * (N / 2 + 1) + lane;
                vec2 v = spectrum[0*N+2];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(0*N+2,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[0] = v;
            
}
{

                int frequency = (1 * N + 2) * (N / 2 + 1) + lane;
                vec2 v = spectrum[1*N+2];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(1*N+2,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[1] = v;
            
}
{

                int frequency = (2 * N + 2) * (N / 2 + 1) + lane;
                vec2 v = spectrum[2*N+2];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(2*N+2,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[2] = v;
            
}
{

                int frequency = (3 * N + 2) * (N / 2 + 1) + lane;
                vec2 v = spectrum[3*N+2];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(3*N+2,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[3] = v;
            
}
{

                int frequency = (4 * N + 2) * (N / 2 + 1) + lane;
                vec2 v = spectrum[4*N+2];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(4*N+2,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[4] = v;
            
}
}
            column[2] = inverseTemporalCenter(temporal);
        
}
{

            vec2 temporal[T];
            {
{

                int frequency = (0 * N + 3) * (N / 2 + 1) + lane;
                vec2 v = spectrum[0*N+3];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(0*N+3,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[0] = v;
            
}
{

                int frequency = (1 * N + 3) * (N / 2 + 1) + lane;
                vec2 v = spectrum[1*N+3];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(1*N+3,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[1] = v;
            
}
{

                int frequency = (2 * N + 3) * (N / 2 + 1) + lane;
                vec2 v = spectrum[2*N+3];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(2*N+3,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[2] = v;
            
}
{

                int frequency = (3 * N + 3) * (N / 2 + 1) + lane;
                vec2 v = spectrum[3*N+3];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(3*N+3,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[3] = v;
            
}
{

                int frequency = (4 * N + 3) * (N / 2 + 1) + lane;
                vec2 v = spectrum[4*N+3];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(4*N+3,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[4] = v;
            
}
}
            column[3] = inverseTemporalCenter(temporal);
        
}
{

            vec2 temporal[T];
            {
{

                int frequency = (0 * N + 4) * (N / 2 + 1) + lane;
                vec2 v = spectrum[0*N+4];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(0*N+4,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[0] = v;
            
}
{

                int frequency = (1 * N + 4) * (N / 2 + 1) + lane;
                vec2 v = spectrum[1*N+4];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(1*N+4,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[1] = v;
            
}
{

                int frequency = (2 * N + 4) * (N / 2 + 1) + lane;
                vec2 v = spectrum[2*N+4];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(2*N+4,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[2] = v;
            
}
{

                int frequency = (3 * N + 4) * (N / 2 + 1) + lane;
                vec2 v = spectrum[3*N+4];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(3*N+4,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[3] = v;
            
}
{

                int frequency = (4 * N + 4) * (N / 2 + 1) + lane;
                vec2 v = spectrum[4*N+4];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(4*N+4,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[4] = v;
            
}
}
            column[4] = inverseTemporalCenter(temporal);
        
}
{

            vec2 temporal[T];
            {
{

                int frequency = (0 * N + 5) * (N / 2 + 1) + lane;
                vec2 v = spectrum[0*N+5];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(0*N+5,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[0] = v;
            
}
{

                int frequency = (1 * N + 5) * (N / 2 + 1) + lane;
                vec2 v = spectrum[1*N+5];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(1*N+5,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[1] = v;
            
}
{

                int frequency = (2 * N + 5) * (N / 2 + 1) + lane;
                vec2 v = spectrum[2*N+5];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(2*N+5,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[2] = v;
            
}
{

                int frequency = (3 * N + 5) * (N / 2 + 1) + lane;
                vec2 v = spectrum[3*N+5];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(3*N+5,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[3] = v;
            
}
{

                int frequency = (4 * N + 5) * (N / 2 + 1) + lane;
                vec2 v = spectrum[4*N+5];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(4*N+5,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[4] = v;
            
}
}
            column[5] = inverseTemporalCenter(temporal);
        
}
{

            vec2 temporal[T];
            {
{

                int frequency = (0 * N + 6) * (N / 2 + 1) + lane;
                vec2 v = spectrum[0*N+6];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(0*N+6,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[0] = v;
            
}
{

                int frequency = (1 * N + 6) * (N / 2 + 1) + lane;
                vec2 v = spectrum[1*N+6];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(1*N+6,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[1] = v;
            
}
{

                int frequency = (2 * N + 6) * (N / 2 + 1) + lane;
                vec2 v = spectrum[2*N+6];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(2*N+6,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[2] = v;
            
}
{

                int frequency = (3 * N + 6) * (N / 2 + 1) + lane;
                vec2 v = spectrum[3*N+6];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(3*N+6,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[3] = v;
            
}
{

                int frequency = (4 * N + 6) * (N / 2 + 1) + lane;
                vec2 v = spectrum[4*N+6];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(4*N+6,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[4] = v;
            
}
}
            column[6] = inverseTemporalCenter(temporal);
        
}
{

            vec2 temporal[T];
            {
{

                int frequency = (0 * N + 7) * (N / 2 + 1) + lane;
                vec2 v = spectrum[0*N+7];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(0*N+7,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[0] = v;
            
}
{

                int frequency = (1 * N + 7) * (N / 2 + 1) + lane;
                vec2 v = spectrum[1*N+7];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(1*N+7,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[1] = v;
            
}
{

                int frequency = (2 * N + 7) * (N / 2 + 1) + lane;
                vec2 v = spectrum[2*N+7];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(2*N+7,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[2] = v;
            
}
{

                int frequency = (3 * N + 7) * (N / 2 + 1) + lane;
                vec2 v = spectrum[3*N+7];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(3*N+7,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[3] = v;
            
}
{

                int frequency = (4 * N + 7) * (N / 2 + 1) + lane;
                vec2 v = spectrum[4*N+7];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(4*N+7,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[4] = v;
            
}
}
            column[7] = inverseTemporalCenter(temporal);
        
}
{

            vec2 temporal[T];
            {
{

                int frequency = (0 * N + 8) * (N / 2 + 1) + lane;
                vec2 v = spectrum[0*N+8];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(0*N+8,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[0] = v;
            
}
{

                int frequency = (1 * N + 8) * (N / 2 + 1) + lane;
                vec2 v = spectrum[1*N+8];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(1*N+8,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[1] = v;
            
}
{

                int frequency = (2 * N + 8) * (N / 2 + 1) + lane;
                vec2 v = spectrum[2*N+8];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(2*N+8,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[2] = v;
            
}
{

                int frequency = (3 * N + 8) * (N / 2 + 1) + lane;
                vec2 v = spectrum[3*N+8];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(3*N+8,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[3] = v;
            
}
{

                int frequency = (4 * N + 8) * (N / 2 + 1) + lane;
                vec2 v = spectrum[4*N+8];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(4*N+8,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[4] = v;
            
}
}
            column[8] = inverseTemporalCenter(temporal);
        
}
{

            vec2 temporal[T];
            {
{

                int frequency = (0 * N + 9) * (N / 2 + 1) + lane;
                vec2 v = spectrum[0*N+9];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(0*N+9,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[0] = v;
            
}
{

                int frequency = (1 * N + 9) * (N / 2 + 1) + lane;
                vec2 v = spectrum[1*N+9];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(1*N+9,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[1] = v;
            
}
{

                int frequency = (2 * N + 9) * (N / 2 + 1) + lane;
                vec2 v = spectrum[2*N+9];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(2*N+9,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[2] = v;
            
}
{

                int frequency = (3 * N + 9) * (N / 2 + 1) + lane;
                vec2 v = spectrum[3*N+9];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(3*N+9,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[3] = v;
            
}
{

                int frequency = (4 * N + 9) * (N / 2 + 1) + lane;
                vec2 v = spectrum[4*N+9];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(4*N+9,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[4] = v;
            
}
}
            column[9] = inverseTemporalCenter(temporal);
        
}
{

            vec2 temporal[T];
            {
{

                int frequency = (0 * N + 10) * (N / 2 + 1) + lane;
                vec2 v = spectrum[0*N+10];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(0*N+10,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[0] = v;
            
}
{

                int frequency = (1 * N + 10) * (N / 2 + 1) + lane;
                vec2 v = spectrum[1*N+10];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(1*N+10,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[1] = v;
            
}
{

                int frequency = (2 * N + 10) * (N / 2 + 1) + lane;
                vec2 v = spectrum[2*N+10];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(2*N+10,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[2] = v;
            
}
{

                int frequency = (3 * N + 10) * (N / 2 + 1) + lane;
                vec2 v = spectrum[3*N+10];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(3*N+10,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[3] = v;
            
}
{

                int frequency = (4 * N + 10) * (N / 2 + 1) + lane;
                vec2 v = spectrum[4*N+10];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(4*N+10,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[4] = v;
            
}
}
            column[10] = inverseTemporalCenter(temporal);
        
}
{

            vec2 temporal[T];
            {
{

                int frequency = (0 * N + 11) * (N / 2 + 1) + lane;
                vec2 v = spectrum[0*N+11];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(0*N+11,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[0] = v;
            
}
{

                int frequency = (1 * N + 11) * (N / 2 + 1) + lane;
                vec2 v = spectrum[1*N+11];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(1*N+11,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[1] = v;
            
}
{

                int frequency = (2 * N + 11) * (N / 2 + 1) + lane;
                vec2 v = spectrum[2*N+11];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(2*N+11,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[2] = v;
            
}
{

                int frequency = (3 * N + 11) * (N / 2 + 1) + lane;
                vec2 v = spectrum[3*N+11];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(3*N+11,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[3] = v;
            
}
{

                int frequency = (4 * N + 11) * (N / 2 + 1) + lane;
                vec2 v = spectrum[4*N+11];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(4*N+11,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[4] = v;
            
}
}
            column[11] = inverseTemporalCenter(temporal);
        
}
{

            vec2 temporal[T];
            {
{

                int frequency = (0 * N + 12) * (N / 2 + 1) + lane;
                vec2 v = spectrum[0*N+12];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(0*N+12,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[0] = v;
            
}
{

                int frequency = (1 * N + 12) * (N / 2 + 1) + lane;
                vec2 v = spectrum[1*N+12];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(1*N+12,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[1] = v;
            
}
{

                int frequency = (2 * N + 12) * (N / 2 + 1) + lane;
                vec2 v = spectrum[2*N+12];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(2*N+12,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[2] = v;
            
}
{

                int frequency = (3 * N + 12) * (N / 2 + 1) + lane;
                vec2 v = spectrum[3*N+12];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(3*N+12,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[3] = v;
            
}
{

                int frequency = (4 * N + 12) * (N / 2 + 1) + lane;
                vec2 v = spectrum[4*N+12];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(4*N+12,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[4] = v;
            
}
}
            column[12] = inverseTemporalCenter(temporal);
        
}
{

            vec2 temporal[T];
            {
{

                int frequency = (0 * N + 13) * (N / 2 + 1) + lane;
                vec2 v = spectrum[0*N+13];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(0*N+13,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[0] = v;
            
}
{

                int frequency = (1 * N + 13) * (N / 2 + 1) + lane;
                vec2 v = spectrum[1*N+13];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(1*N+13,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[1] = v;
            
}
{

                int frequency = (2 * N + 13) * (N / 2 + 1) + lane;
                vec2 v = spectrum[2*N+13];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(2*N+13,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[2] = v;
            
}
{

                int frequency = (3 * N + 13) * (N / 2 + 1) + lane;
                vec2 v = spectrum[3*N+13];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(3*N+13,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[3] = v;
            
}
{

                int frequency = (4 * N + 13) * (N / 2 + 1) + lane;
                vec2 v = spectrum[4*N+13];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(4*N+13,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[4] = v;
            
}
}
            column[13] = inverseTemporalCenter(temporal);
        
}
{

            vec2 temporal[T];
            {
{

                int frequency = (0 * N + 14) * (N / 2 + 1) + lane;
                vec2 v = spectrum[0*N+14];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(0*N+14,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[0] = v;
            
}
{

                int frequency = (1 * N + 14) * (N / 2 + 1) + lane;
                vec2 v = spectrum[1*N+14];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(1*N+14,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[1] = v;
            
}
{

                int frequency = (2 * N + 14) * (N / 2 + 1) + lane;
                vec2 v = spectrum[2*N+14];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(2*N+14,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[2] = v;
            
}
{

                int frequency = (3 * N + 14) * (N / 2 + 1) + lane;
                vec2 v = spectrum[3*N+14];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(3*N+14,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[3] = v;
            
}
{

                int frequency = (4 * N + 14) * (N / 2 + 1) + lane;
                vec2 v = spectrum[4*N+14];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(4*N+14,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[4] = v;
            
}
}
            column[14] = inverseTemporalCenter(temporal);
        
}
{

            vec2 temporal[T];
            {
{

                int frequency = (0 * N + 15) * (N / 2 + 1) + lane;
                vec2 v = spectrum[0*N+15];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(0*N+15,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[0] = v;
            
}
{

                int frequency = (1 * N + 15) * (N / 2 + 1) + lane;
                vec2 v = spectrum[1*N+15];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(1*N+15,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[1] = v;
            
}
{

                int frequency = (2 * N + 15) * (N / 2 + 1) + lane;
                vec2 v = spectrum[2*N+15];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(2*N+15,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[2] = v;
            
}
{

                int frequency = (3 * N + 15) * (N / 2 + 1) + lane;
                vec2 v = spectrum[3*N+15];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(3*N+15,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[3] = v;
            
}
{

                int frequency = (4 * N + 15) * (N / 2 + 1) + lane;
                vec2 v = spectrum[4*N+15];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(4*N+15,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[4] = v;
            
}
}
            column[15] = inverseTemporalCenter(temporal);
        
}
}
        idft16(column);
        {
{
transposeStore(sharedBase + 0 * (N + 1) + lane,column[0]);
}
{
transposeStore(sharedBase + 1 * (N + 1) + lane,column[1]);
}
{
transposeStore(sharedBase + 2 * (N + 1) + lane,column[2]);
}
{
transposeStore(sharedBase + 3 * (N + 1) + lane,column[3]);
}
{
transposeStore(sharedBase + 4 * (N + 1) + lane,column[4]);
}
{
transposeStore(sharedBase + 5 * (N + 1) + lane,column[5]);
}
{
transposeStore(sharedBase + 6 * (N + 1) + lane,column[6]);
}
{
transposeStore(sharedBase + 7 * (N + 1) + lane,column[7]);
}
{
transposeStore(sharedBase + 8 * (N + 1) + lane,column[8]);
}
{
transposeStore(sharedBase + 9 * (N + 1) + lane,column[9]);
}
{
transposeStore(sharedBase + 10 * (N + 1) + lane,column[10]);
}
{
transposeStore(sharedBase + 11 * (N + 1) + lane,column[11]);
}
{
transposeStore(sharedBase + 12 * (N + 1) + lane,column[12]);
}
{
transposeStore(sharedBase + 13 * (N + 1) + lane,column[13]);
}
{
transposeStore(sharedBase + 14 * (N + 1) + lane,column[14]);
}
{
transposeStore(sharedBase + 15 * (N + 1) + lane,column[15]);
}
}
    }
    TILE_BARRIER();
    float realData[N + 2];
    {
{

        vec2 v = transposeLoad(sharedBase + lane * (N + 1) + 0);
        realData[2*0] = v.x;
        realData[2*0+1] = v.y;
    
}
{

        vec2 v = transposeLoad(sharedBase + lane * (N + 1) + 1);
        realData[2*1] = v.x;
        realData[2*1+1] = v.y;
    
}
{

        vec2 v = transposeLoad(sharedBase + lane * (N + 1) + 2);
        realData[2*2] = v.x;
        realData[2*2+1] = v.y;
    
}
{

        vec2 v = transposeLoad(sharedBase + lane * (N + 1) + 3);
        realData[2*3] = v.x;
        realData[2*3+1] = v.y;
    
}
{

        vec2 v = transposeLoad(sharedBase + lane * (N + 1) + 4);
        realData[2*4] = v.x;
        realData[2*4+1] = v.y;
    
}
{

        vec2 v = transposeLoad(sharedBase + lane * (N + 1) + 5);
        realData[2*5] = v.x;
        realData[2*5+1] = v.y;
    
}
{

        vec2 v = transposeLoad(sharedBase + lane * (N + 1) + 6);
        realData[2*6] = v.x;
        realData[2*6+1] = v.y;
    
}
{

        vec2 v = transposeLoad(sharedBase + lane * (N + 1) + 7);
        realData[2*7] = v.x;
        realData[2*7+1] = v.y;
    
}
{

        vec2 v = transposeLoad(sharedBase + lane * (N + 1) + 8);
        realData[2*8] = v.x;
        realData[2*8+1] = v.y;
    
}
}
    TILE_BARRIER();
    irdft16(realData);
    // FFTW's generated r2cb codelet uses the reverse real ordering.
    {
{
transposeStoreReal(sharedBase + 0 * (N + 1) + lane,realData[0 == 0 ? 0 : N-0]);
}
{
transposeStoreReal(sharedBase + 1 * (N + 1) + lane,realData[1 == 0 ? 0 : N-1]);
}
{
transposeStoreReal(sharedBase + 2 * (N + 1) + lane,realData[2 == 0 ? 0 : N-2]);
}
{
transposeStoreReal(sharedBase + 3 * (N + 1) + lane,realData[3 == 0 ? 0 : N-3]);
}
{
transposeStoreReal(sharedBase + 4 * (N + 1) + lane,realData[4 == 0 ? 0 : N-4]);
}
{
transposeStoreReal(sharedBase + 5 * (N + 1) + lane,realData[5 == 0 ? 0 : N-5]);
}
{
transposeStoreReal(sharedBase + 6 * (N + 1) + lane,realData[6 == 0 ? 0 : N-6]);
}
{
transposeStoreReal(sharedBase + 7 * (N + 1) + lane,realData[7 == 0 ? 0 : N-7]);
}
{
transposeStoreReal(sharedBase + 8 * (N + 1) + lane,realData[8 == 0 ? 0 : N-8]);
}
{
transposeStoreReal(sharedBase + 9 * (N + 1) + lane,realData[9 == 0 ? 0 : N-9]);
}
{
transposeStoreReal(sharedBase + 10 * (N + 1) + lane,realData[10 == 0 ? 0 : N-10]);
}
{
transposeStoreReal(sharedBase + 11 * (N + 1) + lane,realData[11 == 0 ? 0 : N-11]);
}
{
transposeStoreReal(sharedBase + 12 * (N + 1) + lane,realData[12 == 0 ? 0 : N-12]);
}
{
transposeStoreReal(sharedBase + 13 * (N + 1) + lane,realData[13 == 0 ? 0 : N-13]);
}
{
transposeStoreReal(sharedBase + 14 * (N + 1) + lane,realData[14 == 0 ? 0 : N-14]);
}
{
transposeStoreReal(sharedBase + 15 * (N + 1) + lane,realData[15 == 0 ? 0 : N-15]);
}
}
    TILE_BARRIER();
    if (validBlock)
        {
{
block[blockId*N*N + 0*N + lane] = transposeLoadReal(sharedBase + lane*(N+1)+0);
}
{
block[blockId*N*N + 1*N + lane] = transposeLoadReal(sharedBase + lane*(N+1)+1);
}
{
block[blockId*N*N + 2*N + lane] = transposeLoadReal(sharedBase + lane*(N+1)+2);
}
{
block[blockId*N*N + 3*N + lane] = transposeLoadReal(sharedBase + lane*(N+1)+3);
}
{
block[blockId*N*N + 4*N + lane] = transposeLoadReal(sharedBase + lane*(N+1)+4);
}
{
block[blockId*N*N + 5*N + lane] = transposeLoadReal(sharedBase + lane*(N+1)+5);
}
{
block[blockId*N*N + 6*N + lane] = transposeLoadReal(sharedBase + lane*(N+1)+6);
}
{
block[blockId*N*N + 7*N + lane] = transposeLoadReal(sharedBase + lane*(N+1)+7);
}
{
block[blockId*N*N + 8*N + lane] = transposeLoadReal(sharedBase + lane*(N+1)+8);
}
{
block[blockId*N*N + 9*N + lane] = transposeLoadReal(sharedBase + lane*(N+1)+9);
}
{
block[blockId*N*N + 10*N + lane] = transposeLoadReal(sharedBase + lane*(N+1)+10);
}
{
block[blockId*N*N + 11*N + lane] = transposeLoadReal(sharedBase + lane*(N+1)+11);
}
{
block[blockId*N*N + 12*N + lane] = transposeLoadReal(sharedBase + lane*(N+1)+12);
}
{
block[blockId*N*N + 13*N + lane] = transposeLoadReal(sharedBase + lane*(N+1)+13);
}
{
block[blockId*N*N + 14*N + lane] = transposeLoadReal(sharedBase + lane*(N+1)+14);
}
{
block[blockId*N*N + 15*N + lane] = transposeLoadReal(sharedBase + lane*(N+1)+15);
}
}
}
#endif
#if T == 7
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
    {
{
result += vec2(value[0].x * phase[0].x - value[0].y * phase[0].y,
                       value[0].x * phase[0].y + value[0].y * phase[0].x);
}
{
result += vec2(value[1].x * phase[1].x - value[1].y * phase[1].y,
                       value[1].x * phase[1].y + value[1].y * phase[1].x);
}
{
result += vec2(value[2].x * phase[2].x - value[2].y * phase[2].y,
                       value[2].x * phase[2].y + value[2].y * phase[2].x);
}
{
result += vec2(value[3].x * phase[3].x - value[3].y * phase[3].y,
                       value[3].x * phase[3].y + value[3].y * phase[3].x);
}
{
result += vec2(value[4].x * phase[4].x - value[4].y * phase[4].y,
                       value[4].x * phase[4].y + value[4].y * phase[4].x);
}
{
result += vec2(value[5].x * phase[5].x - value[5].y * phase[5].y,
                       value[5].x * phase[5].y + value[5].y * phase[5].x);
}
{
result += vec2(value[6].x * phase[6].x - value[6].y * phase[6].y,
                       value[6].x * phase[6].y + value[6].y * phase[6].x);
}
}
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


    if (lane <= N/2) {
        {
{

            {
{

                spectrum[0*N+0] = validBlock ? readCachedSpectrum(0,blockId,0,lane)*temporalFactors[0] : vec2(0.0);
            
}
{

                spectrum[0*N+1] = validBlock ? readCachedSpectrum(0,blockId,1,lane)*temporalFactors[0] : vec2(0.0);
            
}
{

                spectrum[0*N+2] = validBlock ? readCachedSpectrum(0,blockId,2,lane)*temporalFactors[0] : vec2(0.0);
            
}
{

                spectrum[0*N+3] = validBlock ? readCachedSpectrum(0,blockId,3,lane)*temporalFactors[0] : vec2(0.0);
            
}
{

                spectrum[0*N+4] = validBlock ? readCachedSpectrum(0,blockId,4,lane)*temporalFactors[0] : vec2(0.0);
            
}
{

                spectrum[0*N+5] = validBlock ? readCachedSpectrum(0,blockId,5,lane)*temporalFactors[0] : vec2(0.0);
            
}
{

                spectrum[0*N+6] = validBlock ? readCachedSpectrum(0,blockId,6,lane)*temporalFactors[0] : vec2(0.0);
            
}
{

                spectrum[0*N+7] = validBlock ? readCachedSpectrum(0,blockId,7,lane)*temporalFactors[0] : vec2(0.0);
            
}
{

                spectrum[0*N+8] = validBlock ? readCachedSpectrum(0,blockId,8,lane)*temporalFactors[0] : vec2(0.0);
            
}
{

                spectrum[0*N+9] = validBlock ? readCachedSpectrum(0,blockId,9,lane)*temporalFactors[0] : vec2(0.0);
            
}
{

                spectrum[0*N+10] = validBlock ? readCachedSpectrum(0,blockId,10,lane)*temporalFactors[0] : vec2(0.0);
            
}
{

                spectrum[0*N+11] = validBlock ? readCachedSpectrum(0,blockId,11,lane)*temporalFactors[0] : vec2(0.0);
            
}
{

                spectrum[0*N+12] = validBlock ? readCachedSpectrum(0,blockId,12,lane)*temporalFactors[0] : vec2(0.0);
            
}
{

                spectrum[0*N+13] = validBlock ? readCachedSpectrum(0,blockId,13,lane)*temporalFactors[0] : vec2(0.0);
            
}
{

                spectrum[0*N+14] = validBlock ? readCachedSpectrum(0,blockId,14,lane)*temporalFactors[0] : vec2(0.0);
            
}
{

                spectrum[0*N+15] = validBlock ? readCachedSpectrum(0,blockId,15,lane)*temporalFactors[0] : vec2(0.0);
            
}
}
        
}
{

            {
{

                spectrum[1*N+0] = validBlock ? readCachedSpectrum(1,blockId,0,lane)*temporalFactors[1] : vec2(0.0);
            
}
{

                spectrum[1*N+1] = validBlock ? readCachedSpectrum(1,blockId,1,lane)*temporalFactors[1] : vec2(0.0);
            
}
{

                spectrum[1*N+2] = validBlock ? readCachedSpectrum(1,blockId,2,lane)*temporalFactors[1] : vec2(0.0);
            
}
{

                spectrum[1*N+3] = validBlock ? readCachedSpectrum(1,blockId,3,lane)*temporalFactors[1] : vec2(0.0);
            
}
{

                spectrum[1*N+4] = validBlock ? readCachedSpectrum(1,blockId,4,lane)*temporalFactors[1] : vec2(0.0);
            
}
{

                spectrum[1*N+5] = validBlock ? readCachedSpectrum(1,blockId,5,lane)*temporalFactors[1] : vec2(0.0);
            
}
{

                spectrum[1*N+6] = validBlock ? readCachedSpectrum(1,blockId,6,lane)*temporalFactors[1] : vec2(0.0);
            
}
{

                spectrum[1*N+7] = validBlock ? readCachedSpectrum(1,blockId,7,lane)*temporalFactors[1] : vec2(0.0);
            
}
{

                spectrum[1*N+8] = validBlock ? readCachedSpectrum(1,blockId,8,lane)*temporalFactors[1] : vec2(0.0);
            
}
{

                spectrum[1*N+9] = validBlock ? readCachedSpectrum(1,blockId,9,lane)*temporalFactors[1] : vec2(0.0);
            
}
{

                spectrum[1*N+10] = validBlock ? readCachedSpectrum(1,blockId,10,lane)*temporalFactors[1] : vec2(0.0);
            
}
{

                spectrum[1*N+11] = validBlock ? readCachedSpectrum(1,blockId,11,lane)*temporalFactors[1] : vec2(0.0);
            
}
{

                spectrum[1*N+12] = validBlock ? readCachedSpectrum(1,blockId,12,lane)*temporalFactors[1] : vec2(0.0);
            
}
{

                spectrum[1*N+13] = validBlock ? readCachedSpectrum(1,blockId,13,lane)*temporalFactors[1] : vec2(0.0);
            
}
{

                spectrum[1*N+14] = validBlock ? readCachedSpectrum(1,blockId,14,lane)*temporalFactors[1] : vec2(0.0);
            
}
{

                spectrum[1*N+15] = validBlock ? readCachedSpectrum(1,blockId,15,lane)*temporalFactors[1] : vec2(0.0);
            
}
}
        
}
{

            {
{

                spectrum[2*N+0] = validBlock ? readCachedSpectrum(2,blockId,0,lane)*temporalFactors[2] : vec2(0.0);
            
}
{

                spectrum[2*N+1] = validBlock ? readCachedSpectrum(2,blockId,1,lane)*temporalFactors[2] : vec2(0.0);
            
}
{

                spectrum[2*N+2] = validBlock ? readCachedSpectrum(2,blockId,2,lane)*temporalFactors[2] : vec2(0.0);
            
}
{

                spectrum[2*N+3] = validBlock ? readCachedSpectrum(2,blockId,3,lane)*temporalFactors[2] : vec2(0.0);
            
}
{

                spectrum[2*N+4] = validBlock ? readCachedSpectrum(2,blockId,4,lane)*temporalFactors[2] : vec2(0.0);
            
}
{

                spectrum[2*N+5] = validBlock ? readCachedSpectrum(2,blockId,5,lane)*temporalFactors[2] : vec2(0.0);
            
}
{

                spectrum[2*N+6] = validBlock ? readCachedSpectrum(2,blockId,6,lane)*temporalFactors[2] : vec2(0.0);
            
}
{

                spectrum[2*N+7] = validBlock ? readCachedSpectrum(2,blockId,7,lane)*temporalFactors[2] : vec2(0.0);
            
}
{

                spectrum[2*N+8] = validBlock ? readCachedSpectrum(2,blockId,8,lane)*temporalFactors[2] : vec2(0.0);
            
}
{

                spectrum[2*N+9] = validBlock ? readCachedSpectrum(2,blockId,9,lane)*temporalFactors[2] : vec2(0.0);
            
}
{

                spectrum[2*N+10] = validBlock ? readCachedSpectrum(2,blockId,10,lane)*temporalFactors[2] : vec2(0.0);
            
}
{

                spectrum[2*N+11] = validBlock ? readCachedSpectrum(2,blockId,11,lane)*temporalFactors[2] : vec2(0.0);
            
}
{

                spectrum[2*N+12] = validBlock ? readCachedSpectrum(2,blockId,12,lane)*temporalFactors[2] : vec2(0.0);
            
}
{

                spectrum[2*N+13] = validBlock ? readCachedSpectrum(2,blockId,13,lane)*temporalFactors[2] : vec2(0.0);
            
}
{

                spectrum[2*N+14] = validBlock ? readCachedSpectrum(2,blockId,14,lane)*temporalFactors[2] : vec2(0.0);
            
}
{

                spectrum[2*N+15] = validBlock ? readCachedSpectrum(2,blockId,15,lane)*temporalFactors[2] : vec2(0.0);
            
}
}
        
}
{

            {
{

                spectrum[3*N+0] = validBlock ? readCachedSpectrum(3,blockId,0,lane)*temporalFactors[3] : vec2(0.0);
            
}
{

                spectrum[3*N+1] = validBlock ? readCachedSpectrum(3,blockId,1,lane)*temporalFactors[3] : vec2(0.0);
            
}
{

                spectrum[3*N+2] = validBlock ? readCachedSpectrum(3,blockId,2,lane)*temporalFactors[3] : vec2(0.0);
            
}
{

                spectrum[3*N+3] = validBlock ? readCachedSpectrum(3,blockId,3,lane)*temporalFactors[3] : vec2(0.0);
            
}
{

                spectrum[3*N+4] = validBlock ? readCachedSpectrum(3,blockId,4,lane)*temporalFactors[3] : vec2(0.0);
            
}
{

                spectrum[3*N+5] = validBlock ? readCachedSpectrum(3,blockId,5,lane)*temporalFactors[3] : vec2(0.0);
            
}
{

                spectrum[3*N+6] = validBlock ? readCachedSpectrum(3,blockId,6,lane)*temporalFactors[3] : vec2(0.0);
            
}
{

                spectrum[3*N+7] = validBlock ? readCachedSpectrum(3,blockId,7,lane)*temporalFactors[3] : vec2(0.0);
            
}
{

                spectrum[3*N+8] = validBlock ? readCachedSpectrum(3,blockId,8,lane)*temporalFactors[3] : vec2(0.0);
            
}
{

                spectrum[3*N+9] = validBlock ? readCachedSpectrum(3,blockId,9,lane)*temporalFactors[3] : vec2(0.0);
            
}
{

                spectrum[3*N+10] = validBlock ? readCachedSpectrum(3,blockId,10,lane)*temporalFactors[3] : vec2(0.0);
            
}
{

                spectrum[3*N+11] = validBlock ? readCachedSpectrum(3,blockId,11,lane)*temporalFactors[3] : vec2(0.0);
            
}
{

                spectrum[3*N+12] = validBlock ? readCachedSpectrum(3,blockId,12,lane)*temporalFactors[3] : vec2(0.0);
            
}
{

                spectrum[3*N+13] = validBlock ? readCachedSpectrum(3,blockId,13,lane)*temporalFactors[3] : vec2(0.0);
            
}
{

                spectrum[3*N+14] = validBlock ? readCachedSpectrum(3,blockId,14,lane)*temporalFactors[3] : vec2(0.0);
            
}
{

                spectrum[3*N+15] = validBlock ? readCachedSpectrum(3,blockId,15,lane)*temporalFactors[3] : vec2(0.0);
            
}
}
        
}
{

            {
{

                spectrum[4*N+0] = validBlock ? readCachedSpectrum(4,blockId,0,lane)*temporalFactors[4] : vec2(0.0);
            
}
{

                spectrum[4*N+1] = validBlock ? readCachedSpectrum(4,blockId,1,lane)*temporalFactors[4] : vec2(0.0);
            
}
{

                spectrum[4*N+2] = validBlock ? readCachedSpectrum(4,blockId,2,lane)*temporalFactors[4] : vec2(0.0);
            
}
{

                spectrum[4*N+3] = validBlock ? readCachedSpectrum(4,blockId,3,lane)*temporalFactors[4] : vec2(0.0);
            
}
{

                spectrum[4*N+4] = validBlock ? readCachedSpectrum(4,blockId,4,lane)*temporalFactors[4] : vec2(0.0);
            
}
{

                spectrum[4*N+5] = validBlock ? readCachedSpectrum(4,blockId,5,lane)*temporalFactors[4] : vec2(0.0);
            
}
{

                spectrum[4*N+6] = validBlock ? readCachedSpectrum(4,blockId,6,lane)*temporalFactors[4] : vec2(0.0);
            
}
{

                spectrum[4*N+7] = validBlock ? readCachedSpectrum(4,blockId,7,lane)*temporalFactors[4] : vec2(0.0);
            
}
{

                spectrum[4*N+8] = validBlock ? readCachedSpectrum(4,blockId,8,lane)*temporalFactors[4] : vec2(0.0);
            
}
{

                spectrum[4*N+9] = validBlock ? readCachedSpectrum(4,blockId,9,lane)*temporalFactors[4] : vec2(0.0);
            
}
{

                spectrum[4*N+10] = validBlock ? readCachedSpectrum(4,blockId,10,lane)*temporalFactors[4] : vec2(0.0);
            
}
{

                spectrum[4*N+11] = validBlock ? readCachedSpectrum(4,blockId,11,lane)*temporalFactors[4] : vec2(0.0);
            
}
{

                spectrum[4*N+12] = validBlock ? readCachedSpectrum(4,blockId,12,lane)*temporalFactors[4] : vec2(0.0);
            
}
{

                spectrum[4*N+13] = validBlock ? readCachedSpectrum(4,blockId,13,lane)*temporalFactors[4] : vec2(0.0);
            
}
{

                spectrum[4*N+14] = validBlock ? readCachedSpectrum(4,blockId,14,lane)*temporalFactors[4] : vec2(0.0);
            
}
{

                spectrum[4*N+15] = validBlock ? readCachedSpectrum(4,blockId,15,lane)*temporalFactors[4] : vec2(0.0);
            
}
}
        
}
{

            {
{

                spectrum[5*N+0] = validBlock ? readCachedSpectrum(5,blockId,0,lane)*temporalFactors[5] : vec2(0.0);
            
}
{

                spectrum[5*N+1] = validBlock ? readCachedSpectrum(5,blockId,1,lane)*temporalFactors[5] : vec2(0.0);
            
}
{

                spectrum[5*N+2] = validBlock ? readCachedSpectrum(5,blockId,2,lane)*temporalFactors[5] : vec2(0.0);
            
}
{

                spectrum[5*N+3] = validBlock ? readCachedSpectrum(5,blockId,3,lane)*temporalFactors[5] : vec2(0.0);
            
}
{

                spectrum[5*N+4] = validBlock ? readCachedSpectrum(5,blockId,4,lane)*temporalFactors[5] : vec2(0.0);
            
}
{

                spectrum[5*N+5] = validBlock ? readCachedSpectrum(5,blockId,5,lane)*temporalFactors[5] : vec2(0.0);
            
}
{

                spectrum[5*N+6] = validBlock ? readCachedSpectrum(5,blockId,6,lane)*temporalFactors[5] : vec2(0.0);
            
}
{

                spectrum[5*N+7] = validBlock ? readCachedSpectrum(5,blockId,7,lane)*temporalFactors[5] : vec2(0.0);
            
}
{

                spectrum[5*N+8] = validBlock ? readCachedSpectrum(5,blockId,8,lane)*temporalFactors[5] : vec2(0.0);
            
}
{

                spectrum[5*N+9] = validBlock ? readCachedSpectrum(5,blockId,9,lane)*temporalFactors[5] : vec2(0.0);
            
}
{

                spectrum[5*N+10] = validBlock ? readCachedSpectrum(5,blockId,10,lane)*temporalFactors[5] : vec2(0.0);
            
}
{

                spectrum[5*N+11] = validBlock ? readCachedSpectrum(5,blockId,11,lane)*temporalFactors[5] : vec2(0.0);
            
}
{

                spectrum[5*N+12] = validBlock ? readCachedSpectrum(5,blockId,12,lane)*temporalFactors[5] : vec2(0.0);
            
}
{

                spectrum[5*N+13] = validBlock ? readCachedSpectrum(5,blockId,13,lane)*temporalFactors[5] : vec2(0.0);
            
}
{

                spectrum[5*N+14] = validBlock ? readCachedSpectrum(5,blockId,14,lane)*temporalFactors[5] : vec2(0.0);
            
}
{

                spectrum[5*N+15] = validBlock ? readCachedSpectrum(5,blockId,15,lane)*temporalFactors[5] : vec2(0.0);
            
}
}
        
}
{

            {
{

                spectrum[6*N+0] = validBlock ? readCachedSpectrum(6,blockId,0,lane)*temporalFactors[6] : vec2(0.0);
            
}
{

                spectrum[6*N+1] = validBlock ? readCachedSpectrum(6,blockId,1,lane)*temporalFactors[6] : vec2(0.0);
            
}
{

                spectrum[6*N+2] = validBlock ? readCachedSpectrum(6,blockId,2,lane)*temporalFactors[6] : vec2(0.0);
            
}
{

                spectrum[6*N+3] = validBlock ? readCachedSpectrum(6,blockId,3,lane)*temporalFactors[6] : vec2(0.0);
            
}
{

                spectrum[6*N+4] = validBlock ? readCachedSpectrum(6,blockId,4,lane)*temporalFactors[6] : vec2(0.0);
            
}
{

                spectrum[6*N+5] = validBlock ? readCachedSpectrum(6,blockId,5,lane)*temporalFactors[6] : vec2(0.0);
            
}
{

                spectrum[6*N+6] = validBlock ? readCachedSpectrum(6,blockId,6,lane)*temporalFactors[6] : vec2(0.0);
            
}
{

                spectrum[6*N+7] = validBlock ? readCachedSpectrum(6,blockId,7,lane)*temporalFactors[6] : vec2(0.0);
            
}
{

                spectrum[6*N+8] = validBlock ? readCachedSpectrum(6,blockId,8,lane)*temporalFactors[6] : vec2(0.0);
            
}
{

                spectrum[6*N+9] = validBlock ? readCachedSpectrum(6,blockId,9,lane)*temporalFactors[6] : vec2(0.0);
            
}
{

                spectrum[6*N+10] = validBlock ? readCachedSpectrum(6,blockId,10,lane)*temporalFactors[6] : vec2(0.0);
            
}
{

                spectrum[6*N+11] = validBlock ? readCachedSpectrum(6,blockId,11,lane)*temporalFactors[6] : vec2(0.0);
            
}
{

                spectrum[6*N+12] = validBlock ? readCachedSpectrum(6,blockId,12,lane)*temporalFactors[6] : vec2(0.0);
            
}
{

                spectrum[6*N+13] = validBlock ? readCachedSpectrum(6,blockId,13,lane)*temporalFactors[6] : vec2(0.0);
            
}
{

                spectrum[6*N+14] = validBlock ? readCachedSpectrum(6,blockId,14,lane)*temporalFactors[6] : vec2(0.0);
            
}
{

                spectrum[6*N+15] = validBlock ? readCachedSpectrum(6,blockId,15,lane)*temporalFactors[6] : vec2(0.0);
            
}
}
        
}
}
    }
    if (lane <= N/2) {
        {
{

            vec2 temporal[T];
            {
{
temporal[0] = spectrum[0*N+0];
}
{
temporal[1] = spectrum[1*N+0];
}
{
temporal[2] = spectrum[2*N+0];
}
{
temporal[3] = spectrum[3*N+0];
}
{
temporal[4] = spectrum[4*N+0];
}
{
temporal[5] = spectrum[5*N+0];
}
{
temporal[6] = spectrum[6*N+0];
}
}
            dftTemporal(temporal);
            {
{
spectrum[0*N+0] = temporal[0];
}
{
spectrum[1*N+0] = temporal[1];
}
{
spectrum[2*N+0] = temporal[2];
}
{
spectrum[3*N+0] = temporal[3];
}
{
spectrum[4*N+0] = temporal[4];
}
{
spectrum[5*N+0] = temporal[5];
}
{
spectrum[6*N+0] = temporal[6];
}
}
        
}
{

            vec2 temporal[T];
            {
{
temporal[0] = spectrum[0*N+1];
}
{
temporal[1] = spectrum[1*N+1];
}
{
temporal[2] = spectrum[2*N+1];
}
{
temporal[3] = spectrum[3*N+1];
}
{
temporal[4] = spectrum[4*N+1];
}
{
temporal[5] = spectrum[5*N+1];
}
{
temporal[6] = spectrum[6*N+1];
}
}
            dftTemporal(temporal);
            {
{
spectrum[0*N+1] = temporal[0];
}
{
spectrum[1*N+1] = temporal[1];
}
{
spectrum[2*N+1] = temporal[2];
}
{
spectrum[3*N+1] = temporal[3];
}
{
spectrum[4*N+1] = temporal[4];
}
{
spectrum[5*N+1] = temporal[5];
}
{
spectrum[6*N+1] = temporal[6];
}
}
        
}
{

            vec2 temporal[T];
            {
{
temporal[0] = spectrum[0*N+2];
}
{
temporal[1] = spectrum[1*N+2];
}
{
temporal[2] = spectrum[2*N+2];
}
{
temporal[3] = spectrum[3*N+2];
}
{
temporal[4] = spectrum[4*N+2];
}
{
temporal[5] = spectrum[5*N+2];
}
{
temporal[6] = spectrum[6*N+2];
}
}
            dftTemporal(temporal);
            {
{
spectrum[0*N+2] = temporal[0];
}
{
spectrum[1*N+2] = temporal[1];
}
{
spectrum[2*N+2] = temporal[2];
}
{
spectrum[3*N+2] = temporal[3];
}
{
spectrum[4*N+2] = temporal[4];
}
{
spectrum[5*N+2] = temporal[5];
}
{
spectrum[6*N+2] = temporal[6];
}
}
        
}
{

            vec2 temporal[T];
            {
{
temporal[0] = spectrum[0*N+3];
}
{
temporal[1] = spectrum[1*N+3];
}
{
temporal[2] = spectrum[2*N+3];
}
{
temporal[3] = spectrum[3*N+3];
}
{
temporal[4] = spectrum[4*N+3];
}
{
temporal[5] = spectrum[5*N+3];
}
{
temporal[6] = spectrum[6*N+3];
}
}
            dftTemporal(temporal);
            {
{
spectrum[0*N+3] = temporal[0];
}
{
spectrum[1*N+3] = temporal[1];
}
{
spectrum[2*N+3] = temporal[2];
}
{
spectrum[3*N+3] = temporal[3];
}
{
spectrum[4*N+3] = temporal[4];
}
{
spectrum[5*N+3] = temporal[5];
}
{
spectrum[6*N+3] = temporal[6];
}
}
        
}
{

            vec2 temporal[T];
            {
{
temporal[0] = spectrum[0*N+4];
}
{
temporal[1] = spectrum[1*N+4];
}
{
temporal[2] = spectrum[2*N+4];
}
{
temporal[3] = spectrum[3*N+4];
}
{
temporal[4] = spectrum[4*N+4];
}
{
temporal[5] = spectrum[5*N+4];
}
{
temporal[6] = spectrum[6*N+4];
}
}
            dftTemporal(temporal);
            {
{
spectrum[0*N+4] = temporal[0];
}
{
spectrum[1*N+4] = temporal[1];
}
{
spectrum[2*N+4] = temporal[2];
}
{
spectrum[3*N+4] = temporal[3];
}
{
spectrum[4*N+4] = temporal[4];
}
{
spectrum[5*N+4] = temporal[5];
}
{
spectrum[6*N+4] = temporal[6];
}
}
        
}
{

            vec2 temporal[T];
            {
{
temporal[0] = spectrum[0*N+5];
}
{
temporal[1] = spectrum[1*N+5];
}
{
temporal[2] = spectrum[2*N+5];
}
{
temporal[3] = spectrum[3*N+5];
}
{
temporal[4] = spectrum[4*N+5];
}
{
temporal[5] = spectrum[5*N+5];
}
{
temporal[6] = spectrum[6*N+5];
}
}
            dftTemporal(temporal);
            {
{
spectrum[0*N+5] = temporal[0];
}
{
spectrum[1*N+5] = temporal[1];
}
{
spectrum[2*N+5] = temporal[2];
}
{
spectrum[3*N+5] = temporal[3];
}
{
spectrum[4*N+5] = temporal[4];
}
{
spectrum[5*N+5] = temporal[5];
}
{
spectrum[6*N+5] = temporal[6];
}
}
        
}
{

            vec2 temporal[T];
            {
{
temporal[0] = spectrum[0*N+6];
}
{
temporal[1] = spectrum[1*N+6];
}
{
temporal[2] = spectrum[2*N+6];
}
{
temporal[3] = spectrum[3*N+6];
}
{
temporal[4] = spectrum[4*N+6];
}
{
temporal[5] = spectrum[5*N+6];
}
{
temporal[6] = spectrum[6*N+6];
}
}
            dftTemporal(temporal);
            {
{
spectrum[0*N+6] = temporal[0];
}
{
spectrum[1*N+6] = temporal[1];
}
{
spectrum[2*N+6] = temporal[2];
}
{
spectrum[3*N+6] = temporal[3];
}
{
spectrum[4*N+6] = temporal[4];
}
{
spectrum[5*N+6] = temporal[5];
}
{
spectrum[6*N+6] = temporal[6];
}
}
        
}
{

            vec2 temporal[T];
            {
{
temporal[0] = spectrum[0*N+7];
}
{
temporal[1] = spectrum[1*N+7];
}
{
temporal[2] = spectrum[2*N+7];
}
{
temporal[3] = spectrum[3*N+7];
}
{
temporal[4] = spectrum[4*N+7];
}
{
temporal[5] = spectrum[5*N+7];
}
{
temporal[6] = spectrum[6*N+7];
}
}
            dftTemporal(temporal);
            {
{
spectrum[0*N+7] = temporal[0];
}
{
spectrum[1*N+7] = temporal[1];
}
{
spectrum[2*N+7] = temporal[2];
}
{
spectrum[3*N+7] = temporal[3];
}
{
spectrum[4*N+7] = temporal[4];
}
{
spectrum[5*N+7] = temporal[5];
}
{
spectrum[6*N+7] = temporal[6];
}
}
        
}
{

            vec2 temporal[T];
            {
{
temporal[0] = spectrum[0*N+8];
}
{
temporal[1] = spectrum[1*N+8];
}
{
temporal[2] = spectrum[2*N+8];
}
{
temporal[3] = spectrum[3*N+8];
}
{
temporal[4] = spectrum[4*N+8];
}
{
temporal[5] = spectrum[5*N+8];
}
{
temporal[6] = spectrum[6*N+8];
}
}
            dftTemporal(temporal);
            {
{
spectrum[0*N+8] = temporal[0];
}
{
spectrum[1*N+8] = temporal[1];
}
{
spectrum[2*N+8] = temporal[2];
}
{
spectrum[3*N+8] = temporal[3];
}
{
spectrum[4*N+8] = temporal[4];
}
{
spectrum[5*N+8] = temporal[5];
}
{
spectrum[6*N+8] = temporal[6];
}
}
        
}
{

            vec2 temporal[T];
            {
{
temporal[0] = spectrum[0*N+9];
}
{
temporal[1] = spectrum[1*N+9];
}
{
temporal[2] = spectrum[2*N+9];
}
{
temporal[3] = spectrum[3*N+9];
}
{
temporal[4] = spectrum[4*N+9];
}
{
temporal[5] = spectrum[5*N+9];
}
{
temporal[6] = spectrum[6*N+9];
}
}
            dftTemporal(temporal);
            {
{
spectrum[0*N+9] = temporal[0];
}
{
spectrum[1*N+9] = temporal[1];
}
{
spectrum[2*N+9] = temporal[2];
}
{
spectrum[3*N+9] = temporal[3];
}
{
spectrum[4*N+9] = temporal[4];
}
{
spectrum[5*N+9] = temporal[5];
}
{
spectrum[6*N+9] = temporal[6];
}
}
        
}
{

            vec2 temporal[T];
            {
{
temporal[0] = spectrum[0*N+10];
}
{
temporal[1] = spectrum[1*N+10];
}
{
temporal[2] = spectrum[2*N+10];
}
{
temporal[3] = spectrum[3*N+10];
}
{
temporal[4] = spectrum[4*N+10];
}
{
temporal[5] = spectrum[5*N+10];
}
{
temporal[6] = spectrum[6*N+10];
}
}
            dftTemporal(temporal);
            {
{
spectrum[0*N+10] = temporal[0];
}
{
spectrum[1*N+10] = temporal[1];
}
{
spectrum[2*N+10] = temporal[2];
}
{
spectrum[3*N+10] = temporal[3];
}
{
spectrum[4*N+10] = temporal[4];
}
{
spectrum[5*N+10] = temporal[5];
}
{
spectrum[6*N+10] = temporal[6];
}
}
        
}
{

            vec2 temporal[T];
            {
{
temporal[0] = spectrum[0*N+11];
}
{
temporal[1] = spectrum[1*N+11];
}
{
temporal[2] = spectrum[2*N+11];
}
{
temporal[3] = spectrum[3*N+11];
}
{
temporal[4] = spectrum[4*N+11];
}
{
temporal[5] = spectrum[5*N+11];
}
{
temporal[6] = spectrum[6*N+11];
}
}
            dftTemporal(temporal);
            {
{
spectrum[0*N+11] = temporal[0];
}
{
spectrum[1*N+11] = temporal[1];
}
{
spectrum[2*N+11] = temporal[2];
}
{
spectrum[3*N+11] = temporal[3];
}
{
spectrum[4*N+11] = temporal[4];
}
{
spectrum[5*N+11] = temporal[5];
}
{
spectrum[6*N+11] = temporal[6];
}
}
        
}
{

            vec2 temporal[T];
            {
{
temporal[0] = spectrum[0*N+12];
}
{
temporal[1] = spectrum[1*N+12];
}
{
temporal[2] = spectrum[2*N+12];
}
{
temporal[3] = spectrum[3*N+12];
}
{
temporal[4] = spectrum[4*N+12];
}
{
temporal[5] = spectrum[5*N+12];
}
{
temporal[6] = spectrum[6*N+12];
}
}
            dftTemporal(temporal);
            {
{
spectrum[0*N+12] = temporal[0];
}
{
spectrum[1*N+12] = temporal[1];
}
{
spectrum[2*N+12] = temporal[2];
}
{
spectrum[3*N+12] = temporal[3];
}
{
spectrum[4*N+12] = temporal[4];
}
{
spectrum[5*N+12] = temporal[5];
}
{
spectrum[6*N+12] = temporal[6];
}
}
        
}
{

            vec2 temporal[T];
            {
{
temporal[0] = spectrum[0*N+13];
}
{
temporal[1] = spectrum[1*N+13];
}
{
temporal[2] = spectrum[2*N+13];
}
{
temporal[3] = spectrum[3*N+13];
}
{
temporal[4] = spectrum[4*N+13];
}
{
temporal[5] = spectrum[5*N+13];
}
{
temporal[6] = spectrum[6*N+13];
}
}
            dftTemporal(temporal);
            {
{
spectrum[0*N+13] = temporal[0];
}
{
spectrum[1*N+13] = temporal[1];
}
{
spectrum[2*N+13] = temporal[2];
}
{
spectrum[3*N+13] = temporal[3];
}
{
spectrum[4*N+13] = temporal[4];
}
{
spectrum[5*N+13] = temporal[5];
}
{
spectrum[6*N+13] = temporal[6];
}
}
        
}
{

            vec2 temporal[T];
            {
{
temporal[0] = spectrum[0*N+14];
}
{
temporal[1] = spectrum[1*N+14];
}
{
temporal[2] = spectrum[2*N+14];
}
{
temporal[3] = spectrum[3*N+14];
}
{
temporal[4] = spectrum[4*N+14];
}
{
temporal[5] = spectrum[5*N+14];
}
{
temporal[6] = spectrum[6*N+14];
}
}
            dftTemporal(temporal);
            {
{
spectrum[0*N+14] = temporal[0];
}
{
spectrum[1*N+14] = temporal[1];
}
{
spectrum[2*N+14] = temporal[2];
}
{
spectrum[3*N+14] = temporal[3];
}
{
spectrum[4*N+14] = temporal[4];
}
{
spectrum[5*N+14] = temporal[5];
}
{
spectrum[6*N+14] = temporal[6];
}
}
        
}
{

            vec2 temporal[T];
            {
{
temporal[0] = spectrum[0*N+15];
}
{
temporal[1] = spectrum[1*N+15];
}
{
temporal[2] = spectrum[2*N+15];
}
{
temporal[3] = spectrum[3*N+15];
}
{
temporal[4] = spectrum[4*N+15];
}
{
temporal[5] = spectrum[5*N+15];
}
{
temporal[6] = spectrum[6*N+15];
}
}
            dftTemporal(temporal);
            {
{
spectrum[0*N+15] = temporal[0];
}
{
spectrum[1*N+15] = temporal[1];
}
{
spectrum[2*N+15] = temporal[2];
}
{
spectrum[3*N+15] = temporal[3];
}
{
spectrum[4*N+15] = temporal[4];
}
{
spectrum[5*N+15] = temporal[5];
}
{
spectrum[6*N+15] = temporal[6];
}
}
        
}
}
    }
#if ZMEAN
    if (lane == 0) blockMean[tile] = spectrum[0].x / windowSpectrum(0,0).x;
    TILE_BARRIER();
#endif
    vec2 column[N];
    if (lane <= N/2) {
        {
{

            vec2 temporal[T];
            {
{

                int frequency = (0 * N + 0) * (N / 2 + 1) + lane;
                vec2 v = spectrum[0*N+0];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(0*N+0,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[0] = v;
            
}
{

                int frequency = (1 * N + 0) * (N / 2 + 1) + lane;
                vec2 v = spectrum[1*N+0];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(1*N+0,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[1] = v;
            
}
{

                int frequency = (2 * N + 0) * (N / 2 + 1) + lane;
                vec2 v = spectrum[2*N+0];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(2*N+0,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[2] = v;
            
}
{

                int frequency = (3 * N + 0) * (N / 2 + 1) + lane;
                vec2 v = spectrum[3*N+0];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(3*N+0,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[3] = v;
            
}
{

                int frequency = (4 * N + 0) * (N / 2 + 1) + lane;
                vec2 v = spectrum[4*N+0];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(4*N+0,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[4] = v;
            
}
{

                int frequency = (5 * N + 0) * (N / 2 + 1) + lane;
                vec2 v = spectrum[5*N+0];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(5*N+0,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[5] = v;
            
}
{

                int frequency = (6 * N + 0) * (N / 2 + 1) + lane;
                vec2 v = spectrum[6*N+0];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(6*N+0,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[6] = v;
            
}
}
            column[0] = inverseTemporalCenter(temporal);
        
}
{

            vec2 temporal[T];
            {
{

                int frequency = (0 * N + 1) * (N / 2 + 1) + lane;
                vec2 v = spectrum[0*N+1];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(0*N+1,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[0] = v;
            
}
{

                int frequency = (1 * N + 1) * (N / 2 + 1) + lane;
                vec2 v = spectrum[1*N+1];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(1*N+1,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[1] = v;
            
}
{

                int frequency = (2 * N + 1) * (N / 2 + 1) + lane;
                vec2 v = spectrum[2*N+1];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(2*N+1,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[2] = v;
            
}
{

                int frequency = (3 * N + 1) * (N / 2 + 1) + lane;
                vec2 v = spectrum[3*N+1];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(3*N+1,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[3] = v;
            
}
{

                int frequency = (4 * N + 1) * (N / 2 + 1) + lane;
                vec2 v = spectrum[4*N+1];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(4*N+1,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[4] = v;
            
}
{

                int frequency = (5 * N + 1) * (N / 2 + 1) + lane;
                vec2 v = spectrum[5*N+1];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(5*N+1,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[5] = v;
            
}
{

                int frequency = (6 * N + 1) * (N / 2 + 1) + lane;
                vec2 v = spectrum[6*N+1];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(6*N+1,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[6] = v;
            
}
}
            column[1] = inverseTemporalCenter(temporal);
        
}
{

            vec2 temporal[T];
            {
{

                int frequency = (0 * N + 2) * (N / 2 + 1) + lane;
                vec2 v = spectrum[0*N+2];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(0*N+2,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[0] = v;
            
}
{

                int frequency = (1 * N + 2) * (N / 2 + 1) + lane;
                vec2 v = spectrum[1*N+2];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(1*N+2,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[1] = v;
            
}
{

                int frequency = (2 * N + 2) * (N / 2 + 1) + lane;
                vec2 v = spectrum[2*N+2];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(2*N+2,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[2] = v;
            
}
{

                int frequency = (3 * N + 2) * (N / 2 + 1) + lane;
                vec2 v = spectrum[3*N+2];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(3*N+2,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[3] = v;
            
}
{

                int frequency = (4 * N + 2) * (N / 2 + 1) + lane;
                vec2 v = spectrum[4*N+2];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(4*N+2,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[4] = v;
            
}
{

                int frequency = (5 * N + 2) * (N / 2 + 1) + lane;
                vec2 v = spectrum[5*N+2];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(5*N+2,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[5] = v;
            
}
{

                int frequency = (6 * N + 2) * (N / 2 + 1) + lane;
                vec2 v = spectrum[6*N+2];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(6*N+2,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[6] = v;
            
}
}
            column[2] = inverseTemporalCenter(temporal);
        
}
{

            vec2 temporal[T];
            {
{

                int frequency = (0 * N + 3) * (N / 2 + 1) + lane;
                vec2 v = spectrum[0*N+3];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(0*N+3,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[0] = v;
            
}
{

                int frequency = (1 * N + 3) * (N / 2 + 1) + lane;
                vec2 v = spectrum[1*N+3];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(1*N+3,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[1] = v;
            
}
{

                int frequency = (2 * N + 3) * (N / 2 + 1) + lane;
                vec2 v = spectrum[2*N+3];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(2*N+3,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[2] = v;
            
}
{

                int frequency = (3 * N + 3) * (N / 2 + 1) + lane;
                vec2 v = spectrum[3*N+3];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(3*N+3,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[3] = v;
            
}
{

                int frequency = (4 * N + 3) * (N / 2 + 1) + lane;
                vec2 v = spectrum[4*N+3];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(4*N+3,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[4] = v;
            
}
{

                int frequency = (5 * N + 3) * (N / 2 + 1) + lane;
                vec2 v = spectrum[5*N+3];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(5*N+3,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[5] = v;
            
}
{

                int frequency = (6 * N + 3) * (N / 2 + 1) + lane;
                vec2 v = spectrum[6*N+3];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(6*N+3,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[6] = v;
            
}
}
            column[3] = inverseTemporalCenter(temporal);
        
}
{

            vec2 temporal[T];
            {
{

                int frequency = (0 * N + 4) * (N / 2 + 1) + lane;
                vec2 v = spectrum[0*N+4];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(0*N+4,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[0] = v;
            
}
{

                int frequency = (1 * N + 4) * (N / 2 + 1) + lane;
                vec2 v = spectrum[1*N+4];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(1*N+4,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[1] = v;
            
}
{

                int frequency = (2 * N + 4) * (N / 2 + 1) + lane;
                vec2 v = spectrum[2*N+4];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(2*N+4,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[2] = v;
            
}
{

                int frequency = (3 * N + 4) * (N / 2 + 1) + lane;
                vec2 v = spectrum[3*N+4];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(3*N+4,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[3] = v;
            
}
{

                int frequency = (4 * N + 4) * (N / 2 + 1) + lane;
                vec2 v = spectrum[4*N+4];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(4*N+4,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[4] = v;
            
}
{

                int frequency = (5 * N + 4) * (N / 2 + 1) + lane;
                vec2 v = spectrum[5*N+4];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(5*N+4,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[5] = v;
            
}
{

                int frequency = (6 * N + 4) * (N / 2 + 1) + lane;
                vec2 v = spectrum[6*N+4];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(6*N+4,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[6] = v;
            
}
}
            column[4] = inverseTemporalCenter(temporal);
        
}
{

            vec2 temporal[T];
            {
{

                int frequency = (0 * N + 5) * (N / 2 + 1) + lane;
                vec2 v = spectrum[0*N+5];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(0*N+5,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[0] = v;
            
}
{

                int frequency = (1 * N + 5) * (N / 2 + 1) + lane;
                vec2 v = spectrum[1*N+5];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(1*N+5,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[1] = v;
            
}
{

                int frequency = (2 * N + 5) * (N / 2 + 1) + lane;
                vec2 v = spectrum[2*N+5];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(2*N+5,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[2] = v;
            
}
{

                int frequency = (3 * N + 5) * (N / 2 + 1) + lane;
                vec2 v = spectrum[3*N+5];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(3*N+5,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[3] = v;
            
}
{

                int frequency = (4 * N + 5) * (N / 2 + 1) + lane;
                vec2 v = spectrum[4*N+5];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(4*N+5,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[4] = v;
            
}
{

                int frequency = (5 * N + 5) * (N / 2 + 1) + lane;
                vec2 v = spectrum[5*N+5];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(5*N+5,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[5] = v;
            
}
{

                int frequency = (6 * N + 5) * (N / 2 + 1) + lane;
                vec2 v = spectrum[6*N+5];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(6*N+5,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[6] = v;
            
}
}
            column[5] = inverseTemporalCenter(temporal);
        
}
{

            vec2 temporal[T];
            {
{

                int frequency = (0 * N + 6) * (N / 2 + 1) + lane;
                vec2 v = spectrum[0*N+6];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(0*N+6,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[0] = v;
            
}
{

                int frequency = (1 * N + 6) * (N / 2 + 1) + lane;
                vec2 v = spectrum[1*N+6];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(1*N+6,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[1] = v;
            
}
{

                int frequency = (2 * N + 6) * (N / 2 + 1) + lane;
                vec2 v = spectrum[2*N+6];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(2*N+6,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[2] = v;
            
}
{

                int frequency = (3 * N + 6) * (N / 2 + 1) + lane;
                vec2 v = spectrum[3*N+6];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(3*N+6,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[3] = v;
            
}
{

                int frequency = (4 * N + 6) * (N / 2 + 1) + lane;
                vec2 v = spectrum[4*N+6];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(4*N+6,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[4] = v;
            
}
{

                int frequency = (5 * N + 6) * (N / 2 + 1) + lane;
                vec2 v = spectrum[5*N+6];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(5*N+6,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[5] = v;
            
}
{

                int frequency = (6 * N + 6) * (N / 2 + 1) + lane;
                vec2 v = spectrum[6*N+6];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(6*N+6,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[6] = v;
            
}
}
            column[6] = inverseTemporalCenter(temporal);
        
}
{

            vec2 temporal[T];
            {
{

                int frequency = (0 * N + 7) * (N / 2 + 1) + lane;
                vec2 v = spectrum[0*N+7];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(0*N+7,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[0] = v;
            
}
{

                int frequency = (1 * N + 7) * (N / 2 + 1) + lane;
                vec2 v = spectrum[1*N+7];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(1*N+7,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[1] = v;
            
}
{

                int frequency = (2 * N + 7) * (N / 2 + 1) + lane;
                vec2 v = spectrum[2*N+7];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(2*N+7,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[2] = v;
            
}
{

                int frequency = (3 * N + 7) * (N / 2 + 1) + lane;
                vec2 v = spectrum[3*N+7];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(3*N+7,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[3] = v;
            
}
{

                int frequency = (4 * N + 7) * (N / 2 + 1) + lane;
                vec2 v = spectrum[4*N+7];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(4*N+7,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[4] = v;
            
}
{

                int frequency = (5 * N + 7) * (N / 2 + 1) + lane;
                vec2 v = spectrum[5*N+7];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(5*N+7,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[5] = v;
            
}
{

                int frequency = (6 * N + 7) * (N / 2 + 1) + lane;
                vec2 v = spectrum[6*N+7];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(6*N+7,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[6] = v;
            
}
}
            column[7] = inverseTemporalCenter(temporal);
        
}
{

            vec2 temporal[T];
            {
{

                int frequency = (0 * N + 8) * (N / 2 + 1) + lane;
                vec2 v = spectrum[0*N+8];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(0*N+8,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[0] = v;
            
}
{

                int frequency = (1 * N + 8) * (N / 2 + 1) + lane;
                vec2 v = spectrum[1*N+8];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(1*N+8,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[1] = v;
            
}
{

                int frequency = (2 * N + 8) * (N / 2 + 1) + lane;
                vec2 v = spectrum[2*N+8];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(2*N+8,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[2] = v;
            
}
{

                int frequency = (3 * N + 8) * (N / 2 + 1) + lane;
                vec2 v = spectrum[3*N+8];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(3*N+8,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[3] = v;
            
}
{

                int frequency = (4 * N + 8) * (N / 2 + 1) + lane;
                vec2 v = spectrum[4*N+8];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(4*N+8,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[4] = v;
            
}
{

                int frequency = (5 * N + 8) * (N / 2 + 1) + lane;
                vec2 v = spectrum[5*N+8];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(5*N+8,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[5] = v;
            
}
{

                int frequency = (6 * N + 8) * (N / 2 + 1) + lane;
                vec2 v = spectrum[6*N+8];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(6*N+8,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[6] = v;
            
}
}
            column[8] = inverseTemporalCenter(temporal);
        
}
{

            vec2 temporal[T];
            {
{

                int frequency = (0 * N + 9) * (N / 2 + 1) + lane;
                vec2 v = spectrum[0*N+9];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(0*N+9,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[0] = v;
            
}
{

                int frequency = (1 * N + 9) * (N / 2 + 1) + lane;
                vec2 v = spectrum[1*N+9];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(1*N+9,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[1] = v;
            
}
{

                int frequency = (2 * N + 9) * (N / 2 + 1) + lane;
                vec2 v = spectrum[2*N+9];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(2*N+9,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[2] = v;
            
}
{

                int frequency = (3 * N + 9) * (N / 2 + 1) + lane;
                vec2 v = spectrum[3*N+9];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(3*N+9,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[3] = v;
            
}
{

                int frequency = (4 * N + 9) * (N / 2 + 1) + lane;
                vec2 v = spectrum[4*N+9];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(4*N+9,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[4] = v;
            
}
{

                int frequency = (5 * N + 9) * (N / 2 + 1) + lane;
                vec2 v = spectrum[5*N+9];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(5*N+9,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[5] = v;
            
}
{

                int frequency = (6 * N + 9) * (N / 2 + 1) + lane;
                vec2 v = spectrum[6*N+9];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(6*N+9,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[6] = v;
            
}
}
            column[9] = inverseTemporalCenter(temporal);
        
}
{

            vec2 temporal[T];
            {
{

                int frequency = (0 * N + 10) * (N / 2 + 1) + lane;
                vec2 v = spectrum[0*N+10];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(0*N+10,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[0] = v;
            
}
{

                int frequency = (1 * N + 10) * (N / 2 + 1) + lane;
                vec2 v = spectrum[1*N+10];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(1*N+10,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[1] = v;
            
}
{

                int frequency = (2 * N + 10) * (N / 2 + 1) + lane;
                vec2 v = spectrum[2*N+10];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(2*N+10,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[2] = v;
            
}
{

                int frequency = (3 * N + 10) * (N / 2 + 1) + lane;
                vec2 v = spectrum[3*N+10];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(3*N+10,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[3] = v;
            
}
{

                int frequency = (4 * N + 10) * (N / 2 + 1) + lane;
                vec2 v = spectrum[4*N+10];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(4*N+10,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[4] = v;
            
}
{

                int frequency = (5 * N + 10) * (N / 2 + 1) + lane;
                vec2 v = spectrum[5*N+10];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(5*N+10,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[5] = v;
            
}
{

                int frequency = (6 * N + 10) * (N / 2 + 1) + lane;
                vec2 v = spectrum[6*N+10];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(6*N+10,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[6] = v;
            
}
}
            column[10] = inverseTemporalCenter(temporal);
        
}
{

            vec2 temporal[T];
            {
{

                int frequency = (0 * N + 11) * (N / 2 + 1) + lane;
                vec2 v = spectrum[0*N+11];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(0*N+11,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[0] = v;
            
}
{

                int frequency = (1 * N + 11) * (N / 2 + 1) + lane;
                vec2 v = spectrum[1*N+11];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(1*N+11,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[1] = v;
            
}
{

                int frequency = (2 * N + 11) * (N / 2 + 1) + lane;
                vec2 v = spectrum[2*N+11];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(2*N+11,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[2] = v;
            
}
{

                int frequency = (3 * N + 11) * (N / 2 + 1) + lane;
                vec2 v = spectrum[3*N+11];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(3*N+11,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[3] = v;
            
}
{

                int frequency = (4 * N + 11) * (N / 2 + 1) + lane;
                vec2 v = spectrum[4*N+11];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(4*N+11,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[4] = v;
            
}
{

                int frequency = (5 * N + 11) * (N / 2 + 1) + lane;
                vec2 v = spectrum[5*N+11];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(5*N+11,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[5] = v;
            
}
{

                int frequency = (6 * N + 11) * (N / 2 + 1) + lane;
                vec2 v = spectrum[6*N+11];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(6*N+11,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[6] = v;
            
}
}
            column[11] = inverseTemporalCenter(temporal);
        
}
{

            vec2 temporal[T];
            {
{

                int frequency = (0 * N + 12) * (N / 2 + 1) + lane;
                vec2 v = spectrum[0*N+12];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(0*N+12,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[0] = v;
            
}
{

                int frequency = (1 * N + 12) * (N / 2 + 1) + lane;
                vec2 v = spectrum[1*N+12];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(1*N+12,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[1] = v;
            
}
{

                int frequency = (2 * N + 12) * (N / 2 + 1) + lane;
                vec2 v = spectrum[2*N+12];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(2*N+12,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[2] = v;
            
}
{

                int frequency = (3 * N + 12) * (N / 2 + 1) + lane;
                vec2 v = spectrum[3*N+12];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(3*N+12,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[3] = v;
            
}
{

                int frequency = (4 * N + 12) * (N / 2 + 1) + lane;
                vec2 v = spectrum[4*N+12];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(4*N+12,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[4] = v;
            
}
{

                int frequency = (5 * N + 12) * (N / 2 + 1) + lane;
                vec2 v = spectrum[5*N+12];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(5*N+12,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[5] = v;
            
}
{

                int frequency = (6 * N + 12) * (N / 2 + 1) + lane;
                vec2 v = spectrum[6*N+12];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(6*N+12,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[6] = v;
            
}
}
            column[12] = inverseTemporalCenter(temporal);
        
}
{

            vec2 temporal[T];
            {
{

                int frequency = (0 * N + 13) * (N / 2 + 1) + lane;
                vec2 v = spectrum[0*N+13];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(0*N+13,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[0] = v;
            
}
{

                int frequency = (1 * N + 13) * (N / 2 + 1) + lane;
                vec2 v = spectrum[1*N+13];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(1*N+13,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[1] = v;
            
}
{

                int frequency = (2 * N + 13) * (N / 2 + 1) + lane;
                vec2 v = spectrum[2*N+13];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(2*N+13,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[2] = v;
            
}
{

                int frequency = (3 * N + 13) * (N / 2 + 1) + lane;
                vec2 v = spectrum[3*N+13];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(3*N+13,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[3] = v;
            
}
{

                int frequency = (4 * N + 13) * (N / 2 + 1) + lane;
                vec2 v = spectrum[4*N+13];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(4*N+13,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[4] = v;
            
}
{

                int frequency = (5 * N + 13) * (N / 2 + 1) + lane;
                vec2 v = spectrum[5*N+13];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(5*N+13,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[5] = v;
            
}
{

                int frequency = (6 * N + 13) * (N / 2 + 1) + lane;
                vec2 v = spectrum[6*N+13];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(6*N+13,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[6] = v;
            
}
}
            column[13] = inverseTemporalCenter(temporal);
        
}
{

            vec2 temporal[T];
            {
{

                int frequency = (0 * N + 14) * (N / 2 + 1) + lane;
                vec2 v = spectrum[0*N+14];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(0*N+14,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[0] = v;
            
}
{

                int frequency = (1 * N + 14) * (N / 2 + 1) + lane;
                vec2 v = spectrum[1*N+14];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(1*N+14,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[1] = v;
            
}
{

                int frequency = (2 * N + 14) * (N / 2 + 1) + lane;
                vec2 v = spectrum[2*N+14];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(2*N+14,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[2] = v;
            
}
{

                int frequency = (3 * N + 14) * (N / 2 + 1) + lane;
                vec2 v = spectrum[3*N+14];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(3*N+14,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[3] = v;
            
}
{

                int frequency = (4 * N + 14) * (N / 2 + 1) + lane;
                vec2 v = spectrum[4*N+14];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(4*N+14,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[4] = v;
            
}
{

                int frequency = (5 * N + 14) * (N / 2 + 1) + lane;
                vec2 v = spectrum[5*N+14];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(5*N+14,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[5] = v;
            
}
{

                int frequency = (6 * N + 14) * (N / 2 + 1) + lane;
                vec2 v = spectrum[6*N+14];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(6*N+14,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[6] = v;
            
}
}
            column[14] = inverseTemporalCenter(temporal);
        
}
{

            vec2 temporal[T];
            {
{

                int frequency = (0 * N + 15) * (N / 2 + 1) + lane;
                vec2 v = spectrum[0*N+15];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(0*N+15,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[0] = v;
            
}
{

                int frequency = (1 * N + 15) * (N / 2 + 1) + lane;
                vec2 v = spectrum[1*N+15];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(1*N+15,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[1] = v;
            
}
{

                int frequency = (2 * N + 15) * (N / 2 + 1) + lane;
                vec2 v = spectrum[2*N+15];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(2*N+15,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[2] = v;
            
}
{

                int frequency = (3 * N + 15) * (N / 2 + 1) + lane;
                vec2 v = spectrum[3*N+15];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(3*N+15,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[3] = v;
            
}
{

                int frequency = (4 * N + 15) * (N / 2 + 1) + lane;
                vec2 v = spectrum[4*N+15];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(4*N+15,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[4] = v;
            
}
{

                int frequency = (5 * N + 15) * (N / 2 + 1) + lane;
                vec2 v = spectrum[5*N+15];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(5*N+15,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[5] = v;
            
}
{

                int frequency = (6 * N + 15) * (N / 2 + 1) + lane;
                vec2 v = spectrum[6*N+15];
#if ZMEAN
                vec2 meanFrequency = blockMean[tile] * windowSpectrum(6*N+15,lane);
                v -= meanFrequency;
#endif
                v = filterFrequency(v, getSigma(frequency));
#if ZMEAN
                v += meanFrequency;
#endif
                temporal[6] = v;
            
}
}
            column[15] = inverseTemporalCenter(temporal);
        
}
}
        idft16(column);
        {
{
transposeStore(sharedBase + 0 * (N + 1) + lane,column[0]);
}
{
transposeStore(sharedBase + 1 * (N + 1) + lane,column[1]);
}
{
transposeStore(sharedBase + 2 * (N + 1) + lane,column[2]);
}
{
transposeStore(sharedBase + 3 * (N + 1) + lane,column[3]);
}
{
transposeStore(sharedBase + 4 * (N + 1) + lane,column[4]);
}
{
transposeStore(sharedBase + 5 * (N + 1) + lane,column[5]);
}
{
transposeStore(sharedBase + 6 * (N + 1) + lane,column[6]);
}
{
transposeStore(sharedBase + 7 * (N + 1) + lane,column[7]);
}
{
transposeStore(sharedBase + 8 * (N + 1) + lane,column[8]);
}
{
transposeStore(sharedBase + 9 * (N + 1) + lane,column[9]);
}
{
transposeStore(sharedBase + 10 * (N + 1) + lane,column[10]);
}
{
transposeStore(sharedBase + 11 * (N + 1) + lane,column[11]);
}
{
transposeStore(sharedBase + 12 * (N + 1) + lane,column[12]);
}
{
transposeStore(sharedBase + 13 * (N + 1) + lane,column[13]);
}
{
transposeStore(sharedBase + 14 * (N + 1) + lane,column[14]);
}
{
transposeStore(sharedBase + 15 * (N + 1) + lane,column[15]);
}
}
    }
    TILE_BARRIER();
    float realData[N + 2];
    {
{

        vec2 v = transposeLoad(sharedBase + lane * (N + 1) + 0);
        realData[2*0] = v.x;
        realData[2*0+1] = v.y;
    
}
{

        vec2 v = transposeLoad(sharedBase + lane * (N + 1) + 1);
        realData[2*1] = v.x;
        realData[2*1+1] = v.y;
    
}
{

        vec2 v = transposeLoad(sharedBase + lane * (N + 1) + 2);
        realData[2*2] = v.x;
        realData[2*2+1] = v.y;
    
}
{

        vec2 v = transposeLoad(sharedBase + lane * (N + 1) + 3);
        realData[2*3] = v.x;
        realData[2*3+1] = v.y;
    
}
{

        vec2 v = transposeLoad(sharedBase + lane * (N + 1) + 4);
        realData[2*4] = v.x;
        realData[2*4+1] = v.y;
    
}
{

        vec2 v = transposeLoad(sharedBase + lane * (N + 1) + 5);
        realData[2*5] = v.x;
        realData[2*5+1] = v.y;
    
}
{

        vec2 v = transposeLoad(sharedBase + lane * (N + 1) + 6);
        realData[2*6] = v.x;
        realData[2*6+1] = v.y;
    
}
{

        vec2 v = transposeLoad(sharedBase + lane * (N + 1) + 7);
        realData[2*7] = v.x;
        realData[2*7+1] = v.y;
    
}
{

        vec2 v = transposeLoad(sharedBase + lane * (N + 1) + 8);
        realData[2*8] = v.x;
        realData[2*8+1] = v.y;
    
}
}
    TILE_BARRIER();
    irdft16(realData);
    // FFTW's generated r2cb codelet uses the reverse real ordering.
    {
{
transposeStoreReal(sharedBase + 0 * (N + 1) + lane,realData[0 == 0 ? 0 : N-0]);
}
{
transposeStoreReal(sharedBase + 1 * (N + 1) + lane,realData[1 == 0 ? 0 : N-1]);
}
{
transposeStoreReal(sharedBase + 2 * (N + 1) + lane,realData[2 == 0 ? 0 : N-2]);
}
{
transposeStoreReal(sharedBase + 3 * (N + 1) + lane,realData[3 == 0 ? 0 : N-3]);
}
{
transposeStoreReal(sharedBase + 4 * (N + 1) + lane,realData[4 == 0 ? 0 : N-4]);
}
{
transposeStoreReal(sharedBase + 5 * (N + 1) + lane,realData[5 == 0 ? 0 : N-5]);
}
{
transposeStoreReal(sharedBase + 6 * (N + 1) + lane,realData[6 == 0 ? 0 : N-6]);
}
{
transposeStoreReal(sharedBase + 7 * (N + 1) + lane,realData[7 == 0 ? 0 : N-7]);
}
{
transposeStoreReal(sharedBase + 8 * (N + 1) + lane,realData[8 == 0 ? 0 : N-8]);
}
{
transposeStoreReal(sharedBase + 9 * (N + 1) + lane,realData[9 == 0 ? 0 : N-9]);
}
{
transposeStoreReal(sharedBase + 10 * (N + 1) + lane,realData[10 == 0 ? 0 : N-10]);
}
{
transposeStoreReal(sharedBase + 11 * (N + 1) + lane,realData[11 == 0 ? 0 : N-11]);
}
{
transposeStoreReal(sharedBase + 12 * (N + 1) + lane,realData[12 == 0 ? 0 : N-12]);
}
{
transposeStoreReal(sharedBase + 13 * (N + 1) + lane,realData[13 == 0 ? 0 : N-13]);
}
{
transposeStoreReal(sharedBase + 14 * (N + 1) + lane,realData[14 == 0 ? 0 : N-14]);
}
{
transposeStoreReal(sharedBase + 15 * (N + 1) + lane,realData[15 == 0 ? 0 : N-15]);
}
}
    TILE_BARRIER();
    if (validBlock)
        {
{
block[blockId*N*N + 0*N + lane] = transposeLoadReal(sharedBase + lane*(N+1)+0);
}
{
block[blockId*N*N + 1*N + lane] = transposeLoadReal(sharedBase + lane*(N+1)+1);
}
{
block[blockId*N*N + 2*N + lane] = transposeLoadReal(sharedBase + lane*(N+1)+2);
}
{
block[blockId*N*N + 3*N + lane] = transposeLoadReal(sharedBase + lane*(N+1)+3);
}
{
block[blockId*N*N + 4*N + lane] = transposeLoadReal(sharedBase + lane*(N+1)+4);
}
{
block[blockId*N*N + 5*N + lane] = transposeLoadReal(sharedBase + lane*(N+1)+5);
}
{
block[blockId*N*N + 6*N + lane] = transposeLoadReal(sharedBase + lane*(N+1)+6);
}
{
block[blockId*N*N + 7*N + lane] = transposeLoadReal(sharedBase + lane*(N+1)+7);
}
{
block[blockId*N*N + 8*N + lane] = transposeLoadReal(sharedBase + lane*(N+1)+8);
}
{
block[blockId*N*N + 9*N + lane] = transposeLoadReal(sharedBase + lane*(N+1)+9);
}
{
block[blockId*N*N + 10*N + lane] = transposeLoadReal(sharedBase + lane*(N+1)+10);
}
{
block[blockId*N*N + 11*N + lane] = transposeLoadReal(sharedBase + lane*(N+1)+11);
}
{
block[blockId*N*N + 12*N + lane] = transposeLoadReal(sharedBase + lane*(N+1)+12);
}
{
block[blockId*N*N + 13*N + lane] = transposeLoadReal(sharedBase + lane*(N+1)+13);
}
{
block[blockId*N*N + 14*N + lane] = transposeLoadReal(sharedBase + lane*(N+1)+14);
}
{
block[blockId*N*N + 15*N + lane] = transposeLoadReal(sharedBase + lane*(N+1)+15);
}
}
}
#endif
#else
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
    UNROLL_LOOP for (int t=0; t<T; ++t)
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


    if (lane <= N/2) {
        UNROLL_LOOP for (int t=0; t<T; ++t) {
            UNROLL_LOOP for (int y=0; y<N; ++y) {
                spectrum[t*N+y] = validBlock ? readCachedSpectrum(t,blockId,y,lane)*temporalFactors[t] : vec2(0.0);
            }
        }
    }
    if (lane <= N/2) {
        UNROLL_LOOP for (int y=0; y<N; ++y) {
            vec2 temporal[T];
            UNROLL_LOOP for (int t=0; t<T; ++t) temporal[t] = spectrum[t*N+y];
            dftTemporal(temporal);
            UNROLL_LOOP for (int t=0; t<T; ++t) spectrum[t*N+y] = temporal[t];
        }
    }
#if ZMEAN
    if (lane == 0) blockMean[tile] = spectrum[0].x / windowSpectrum(0,0).x;
    TILE_BARRIER();
#endif
    vec2 column[N];
    if (lane <= N/2) {
        UNROLL_LOOP for (int y=0; y<N; ++y) {
            vec2 temporal[T];
            UNROLL_LOOP for (int t=0; t<T; ++t) {
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
        UNROLL_LOOP for (int y=0; y<N; ++y)
            transposeStore(sharedBase + y * (N + 1) + lane,column[y]);
    }
    TILE_BARRIER();
    float realData[N + 2];
    UNROLL_LOOP for (int x=0; x<=N/2; ++x) {
        vec2 v = transposeLoad(sharedBase + lane * (N + 1) + x);
        realData[2*x] = v.x;
        realData[2*x+1] = v.y;
    }
    TILE_BARRIER();
    irdft16(realData);
    // FFTW's generated r2cb codelet uses the reverse real ordering.
    UNROLL_LOOP for (int x=0; x<N; ++x)
        transposeStoreReal(sharedBase + x * (N + 1) + lane,realData[x == 0 ? 0 : N-x]);
    TILE_BARRIER();
    if (validBlock)
        UNROLL_LOOP for (int y=0; y<N; ++y)
            block[blockId*N*N + y*N + lane] = transposeLoadReal(sharedBase + lane*(N+1)+y);
}
#endif
)glsl";
} // namespace cached_shader
