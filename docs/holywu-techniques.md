# 本次使用的 HolyWu Vulkan 方法

本次使用的是从 HolyWu 的 BM3DVK2、EEDI3VK2、NNEDI3VK 源码归纳出的
优化方法。DFTTest 的数学算法来自原 DFTTest，固定 FFT codelet 来自
DFTTest2 / FFTW。下面把方法来源和本项目的实际实现对应起来。

| 方法 | HolyWu 源码依据 | DFTTestVK 中的应用 |
| --- | --- | --- |
| 先消除重复工作，再调线程布局 | [BM3DVK2 的时域增量处理](https://github.com/HolyWu/VapourSynth-bm3dvk2/blob/5e30997c4b097d720bf2f33956c507d4b024e125/src/bm3dvk.cpp#L1697) | 相邻输出重复使用同一源帧的空间 FFT。B=16、T>1 时缓存源帧半谱，后续只完成时域 FFT、滤波和中心切片逆变换。顺序 T=3 的空间正变换由每输出三次降为约一次；代价是谱写入、读取和显存。 |
| 将已知参数移到 host / 编译期，减少动态索引 | [BM3DVK2 的生成 codelet 与 host plan](https://github.com/HolyWu/VapourSynth-bm3dvk2/blob/5e30997c4b097d720bf2f33956c507d4b024e125/src/dct_codelets.h#L14) | 对 N=16、T=1/3/5/7 展开小循环，使私有数组索引在 GLSL 编译前成为常数；窗与频域常量在构图时生成并上传。平面内部坐标走直接访问，边界再计算反射映射。 |
| 将寄存器、shared memory 和工作组大小一起选择 | [BM3DVK2 makePlan](https://github.com/HolyWu/VapourSynth-bm3dvk2/blob/5e30997c4b097d720bf2f33956c507d4b024e125/src/bm3dvk.cpp#L784) | 实测比较工作组内 block 数、shared 布局、subgroup 和展开候选；生产默认以 16 tiles 起步，按设备线程与 shared 限制退到 8/4/2。变换算术保持 FP32，实验开关和默认路径分开。 |
| 数据放置与生命周期属于优化的一部分 | [BM3DVK2 的 core GPU 资源管理](https://github.com/HolyWu/VapourSynth-bm3dvk2/blob/5e30997c4b097d720bf2f33956c507d4b024e125/src/bm3dvk.cpp) | 复用 VapourSynth R81 设备、GPU 帧与执行池；谱放在普通 device-local buffer。独立显卡先将预处理源平面复制到 device-local buffer，以减少重复读取成本；GPU 链可使用 DFTTestGPU 入口。 |
| 持久状态必须覆盖真实请求顺序 | [BM3DVK2 restart / temporalFrame](https://github.com/HolyWu/VapourSynth-bm3dvk2/blob/5e30997c4b097d720bf2f33956c507d4b024e125/src/bm3dvk.cpp#L1697) | 按源帧编号缓存，shared future 合并并发创建；LRU 淘汰后，仍被 GPU 请求使用的谱保留到完成。逆序、跳帧、重复、首尾与短片用无缓存路径和独立 oracle 对照。DFTTestVK 用键控缓存，未照搬 BM3D 的顺序环形 accumulator。 |

FFT codelet、融合 FFT → 滤波 → 逆变换、shared transpose，以及 gather
overlap-add，DFTTest2 已有相应设计。本项目将其适配到 Vulkan，但这些
不能计作新发明的 HolyWu 技巧。窗口带有逐像素权重，也没有使用
BM3DVK2 的“block 原点分母 + 均匀 box sum”重写；没有采用 EEDI3 的 DP
shuffle、NNEDI3 的稀疏 predictor compaction 或 Tensor Core 网络。

本次最主要的新数据流是跨输出复用空间半谱。它带来额外显存和缓存缺失
成本，收益取决于 T、尺寸、重叠、平面数与请求顺序。Windows RTX 3090 Ti
上的实际结果见 [benchmark.md](benchmark.md)，T=1 的 NVRTC 更快。
总 FPS 不能证明每项优化的独立收益，也不能把时钟差异归因于代码。

技能还要求数值验证和完整处理链测量。本次以独立 NumPy float64 FFT
oracle 检查全部像素与参数边界，以同步验证检查资源访问；测速包含完成的
CPU 输出及必要上传/下载，采用轮换后端、独立进程重复、二进制与输入
哈希。这是本次复现采用的验证方法，不代表 HolyWu 私人的开发流程。

实现与验证材料：

- [tools/generate_shaders.py](../tools/generate_shaders.py)：直接 / 缓存 kernel 和静态索引生成。
- [src/dfttestvk.cpp](../src/dfttestvk.cpp)：设备布局、资源生命周期、缓存与执行计划。
- [temporal-cache-design.md](temporal-cache-design.md)：半谱布局、并发、同步、预算与访问顺序。
- [optimization-notes.md](optimization-notes.md)：保留和撤回的实验。
- [THIRD_PARTY.md](../THIRD_PARTY.md)：算法、codelet 的来源与许可。
