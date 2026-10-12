// SPDX-License-Identifier: GPL-3.0-or-later
// Algorithm: DFTTest (HolyWu), DFTTest2 (sunflower dolls / AliceTeaParty).
#include <VapourSynth4.h>
#include <VSVulkan4.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <future>
#include <limits>
#include <memory>
#include <mutex>
#include <numbers>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <string>
#include <vector>
#include <unordered_map>
#include "shaders.h"
#include "generic_shaders.h"
#include "cached_shaders.h"

namespace {
constexpr int errorSize=8192;
constexpr const char* pluginID="com.ryougikukoc.dfttestvk";
[[noreturn]] void fail(const std::string& message) { throw std::runtime_error("DFTTestVK: "+message); }
void check(VkResult result,const char* action) {
    if(result!=VK_SUCCESS) fail(std::string(action)+" failed ("+std::to_string(result)+")");
}
int integer(const VSMap* map,const char* name,int fallback,const VSAPI* api) {
    int error{};int64_t value=api->mapGetInt(map,name,0,&error);
    if(error) return fallback;
    if(value<std::numeric_limits<int>::min() || value>std::numeric_limits<int>::max()) fail(std::string(name)+" is out of range");
    return int(value);
}
double number(const VSMap* map,const char* name,double fallback,const VSAPI* api) {
    int error{};double value=api->mapGetFloat(map,name,0,&error);
    if(error) return fallback;
    if(!std::isfinite(value)) fail(std::string(name)+" must be finite");
    return value;
}
std::string literal(double value) {
    // Match the reference's conversion to binary32 before use by native kernels.
    std::ostringstream stream;stream<<std::scientific<<std::setprecision(9)<<float(value);return stream.str();
}
double bessel(double value) {
    value/=2;double numerator=1,denominator=1,total=1;
    for(int k=1;k<15;++k) {
        numerator*=value;denominator*=k;double term=numerator/denominator;total+=term*term;
        if(term<=1e-8) break;
    }
    return total;
}
double windowValue(int index,int size,int mode,double beta) {
    double location=index+0.5,p=std::numbers::pi*location/size;
    switch(mode) {
        case 0:return 0.5*(1-std::cos(2*p));
        case 1:return 0.53836-0.46164*std::cos(2*p);
        case 2:return 0.42-0.5*std::cos(2*p)+0.08*std::cos(4*p);
        case 3:return 0.35875-0.48829*std::cos(2*p)+0.14128*std::cos(4*p)-0.01168*std::cos(6*p);
        case 4:{double v=2*location/size-1;return bessel(std::numbers::pi*beta*std::sqrt(1-v*v))/bessel(std::numbers::pi*beta);}
        case 5:return 0.27105140069342415-0.433297939234486060*std::cos(2*p)+0.218122999543110620*std::cos(4*p)
            -0.065925446388030898*std::cos(6*p)+0.010811742098372268*std::cos(8*p)
            -7.7658482522509342e-4*std::cos(10*p)+1.3887217350903198e-5*std::cos(12*p);
        case 6:return 0.2810639-0.5208972*std::cos(2*p)+0.1980399*std::cos(4*p);
        case 7:return 1;
        case 8:return 1-2*std::abs(location-size/2.0)/size;
        // Preserve DFTTest2's current formula, including the unsymmetrized term.
        case 9:return 0.62-0.48*(location/size-0.5)-0.38*std::cos(2*p);
        case 10:return 0.355768-0.487396*std::cos(2*p)+0.144232*std::cos(4*p)-0.012604*std::cos(6*p);
        case 11:return 0.3635819-0.4891775*std::cos(2*p)+0.1365995*std::cos(4*p)-0.0106411*std::cos(6*p);
    }
    fail("window mode must be in 0..11");
}
struct Profile {
    std::vector<std::pair<double,double>> points;
    double constant{};
    double operator()(double x)const {
        if(points.empty()) return constant;
        for(size_t i=0;i+1<points.size();++i) if(x<=points[i+1].first) {
            double w=(x-points[i].first)/(points[i+1].first-points[i].first);
            return (1-w)*points[i].second+w*points[i+1].second;
        }
        return points.back().second;
    }
};
struct Kernel {VkDescriptorSetLayout descriptors{};VkPipelineLayout layout{};VkPipeline pipeline{};};
struct Push {
    int32_t width,height,dstStride,blocksX,blocksY,padX,padY;
    int32_t srcStride[7];
};
static_assert(sizeof(Push)==56);
struct CachedSpectrum {
    const VSAPI* api{};const VSVULKANAPI* gpu{};VSGPUBuffer* storage{};VSVulkanBufferInfo info{};
    VSFrame* token{};uint64_t tick{};
    std::promise<void> submitted;std::shared_future<void> ready=submitted.get_future().share();
    bool pending=true;
    ~CachedSpectrum(){if(token)api->freeFrame(token);if(storage)gpu->destroyGPUBuffer(storage);}
    static void VS_CC release(void* p){delete static_cast<std::shared_ptr<CachedSpectrum>*>(p);}
};
struct Filter {
    const VSAPI* api{};const VSVULKANAPI* gpu{};const VSVulkanFunctions* vk{};
    VSVulkanCoreHandles handles{};VSNode* source{};VSVideoInfo vi{};
    bool temporalCache=false;
    bool cacheDeviceCopy=false;
    std::array<int,3> cacheRow{};int cacheRows{};int cacheFrames=12;
    std::string temporalShaderText;
    int blockSize=16,step=4,temporal=3,filterType=0;
    int tiles=4;bool useSubgroup=false,unroll=false;
    bool staticMain=false,shortReflect=false;
    bool deviceConstants=false;
    bool fftFMA=false,realShared=false;
    bool embedConstants=false;int sharedPad=0;
    bool sigmaScalarKnown=false;double sigmaScalar{};
    std::string constantShaderText;
    bool rowConstants=false;std::string rowShaderText;
    bool warpTile=false;
    uint32_t maxGroupsX{},maxGroupsY{};
    bool zeroMean=true;std::array<bool,3> process{true,true,true};
    bool profileEnabled=false;uint32_t timestampBits{};double timestampPeriod{};
    std::array<std::atomic<uint64_t>,3> fusedNs{},gatherNs{},profileFrames{};
    double sigma2{},pmin{},pmax{};
    VSGPUExecPool* pool{};VSGPUBuffer* constants{};VSVulkanBufferInfo constantInfo{};
    VSGPUExecPool* cachePool{};std::mutex cacheMutex;uint64_t cacheTick{};
    std::atomic<uint64_t> cacheHits{},cacheMisses{};
    std::unordered_map<int,std::shared_ptr<CachedSpectrum>> spectra;
    std::array<Kernel,3> kernels{};
    ~Filter() {
        if(pool) gpu->freeGPUExecPool(pool);
        if(cachePool)gpu->freeGPUExecPool(cachePool);
        spectra.clear();
        if(profileEnabled&&temporalCache)std::fprintf(stderr,"DFTTESTVK_PROFILE cache_hits=%llu cache_misses=%llu\n",
            (unsigned long long)cacheHits.load(),(unsigned long long)cacheMisses.load());
        if(profileEnabled) for(int p=0;p<vi.format.numPlanes;++p) if(profileFrames[p]) {
            double count=double(profileFrames[p].load());
            std::fprintf(stderr,"DFTTESTVK_PROFILE plane=%d frames=%.0f fused_ms=%.6f gather_ms=%.6f\n",p,count,
                double(fusedNs[p].load())/count/1e6,double(gatherNs[p].load())/count/1e6);
        }
        if(constants) gpu->destroyGPUBuffer(constants);
        if(vk) for(auto& k:kernels) {
            if(k.pipeline) vk->vkDestroyPipeline(handles.device,k.pipeline,nullptr);
            if(k.layout) vk->vkDestroyPipelineLayout(handles.device,k.layout,nullptr);
            if(k.descriptors) vk->vkDestroyDescriptorSetLayout(handles.device,k.descriptors,nullptr);
        }
        if(source) api->freeNode(source);
    }
    Profile profile(const VSMap* in,const char* name,double sigma,bool radial,const VSAPI* api) {
        const int dimensions=(blockSize>1?2:0)+(temporal>1?1:0);
        auto norm=[&](double x) {return radial||dimensions<=1 ? x : (dimensions==2 ? std::sqrt(x) : std::cbrt(x));};
        Profile result;result.constant=norm(sigma);
        int count=api->mapNumElements(in,name);
        if(count<0) return result;
        if(count<4 || count%2) fail(std::string(name)+" must contain at least two frequency/sigma pairs");
        for(int i=0;i<count;i+=2) {
            double x=api->mapGetFloat(in,name,i,nullptr),s=api->mapGetFloat(in,name,i+1,nullptr);
            if(!std::isfinite(x)||!std::isfinite(s)||x<0||x>1||s<0) fail(std::string(name)+" requires finite frequencies in [0,1] and nonnegative sigma");
            result.points.emplace_back(x,norm(s));
        }
        std::sort(result.points.begin(),result.points.end());
        if(result.points.front().first!=0 || result.points.back().first!=1) fail(std::string(name)+" must cover frequencies 0 and 1");
        for(size_t i=1;i<result.points.size();++i) if(result.points[i-1].first==result.points[i].first) fail(std::string(name)+" has duplicate frequencies");
        return result;
    }
    void makeConstants(const VSMap* in,VSCore* core) {
        const int B=blockSize,T=temporal,H=B/2+1;
        int spatialMode=integer(in,"swin",0,api),temporalMode=integer(in,"twin",7,api);
        double spatialBeta=number(in,"sbeta",2.5,api),temporalBeta=number(in,"tbeta",2.5,api);
        if(spatialMode<0||spatialMode>11||temporalMode<0||temporalMode>11) fail("swin and twin must be in 0..11");
        if(spatialBeta<0||temporalBeta<0) fail("window beta must be nonnegative");
        std::vector<double> spatial(B),time(T),window(T*B*B);
        for(int i=0;i<B;++i) spatial[i]=windowValue(i,B,spatialMode,spatialBeta);
        auto unnormalized=spatial;
        for(int q=0;q<B;++q) {
            double sum=0;
            for(int h=q;h>=0;h-=step) sum+=unnormalized[h]*unnormalized[h];
            for(int h=q+step;h<B;h+=step) sum+=unnormalized[h]*unnormalized[h];
            if(!(sum>0)) fail("window has zero overlap energy");
            spatial[q]/=std::sqrt(sum);
        }
        for(int i=0;i<T;++i) time[i]=windowValue(i,T,temporalMode,temporalBeta);
        if(temporalCache) {
            if(std::abs(time[T/2])<1e-15) fail("temporal FFT caching requires a nonzero central window");
            temporalShaderText="const float temporalFactors[T]=float[T](";
            for(int t=0;t<T;++t){if(t)temporalShaderText+=",";temporalShaderText+=literal(time[t]/time[T/2]);}
            temporalShaderText+=");\n";
        }
        double wscale=0;
        for(int t=0;t<T;++t) for(int y=0;y<B;++y) for(int x=0;x<B;++x) {
            double w=time[t]*spatial[y]*spatial[x]/(std::sqrt(double(T))*B);
            window[(t*B+y)*B+x]=w;wscale+=w*w;
        }
        // The separable window's DFT is computed in binary64 once, like RDFT in the reference.
        auto dft=[](const std::vector<double>& input) {
            std::vector<std::complex<double>> output(input.size());
            for(size_t k=0;k<input.size();++k) for(size_t j=0;j<input.size();++j) {
                double angle=-2*std::numbers::pi*double(k*j)/input.size();
                output[k]+=input[j]*std::complex<double>(std::cos(angle),std::sin(angle));
            }
            return output;
        };
        auto sf=dft(spatial),tf=dft(time);
        std::vector<float> data;data.reserve(T*B*B+3*T*B*H);
        for(double w:window) data.push_back(float(w));
        for(int t=0;t<T;++t) for(int y=0;y<B;++y) for(int x=0;x<H;++x) {
            auto value=tf[t]*sf[y]*sf[x]*(255/(std::sqrt(double(T))*B));
            data.push_back(float(value.real()));data.push_back(float(value.imag()));
        }
        if(zeroMean && (!std::isfinite(data[T*B*B]) || std::abs(data[T*B*B])<1e-15f)) fail("window has zero DC response with zmean enabled");
        double sigma=number(in,"sigma",8,api);
        sigma2=number(in,"sigma2",8,api);pmin=number(in,"pmin",0,api);pmax=number(in,"pmax",500,api);
        double beta=number(in,"f0beta",1,api);
        if(sigma<0||sigma2<0||pmin<0||pmax<0||beta<0) fail("sigma/sigma2/pmin/pmax/f0beta must be nonnegative");
        int originalType=filterType;
        if(filterType==0) filterType=std::abs(beta-1)<0.00005 ? 0 : (std::abs(beta-0.5)<0.0005 ? 6 : 5);
        int system=integer(in,"ssystem",0,api);
        if(system!=0 && system!=1) fail("ssystem must be 0 or 1");
        bool location=api->mapNumElements(in,"slocation")>=0;
        bool axes=api->mapNumElements(in,"ssx")>=0||api->mapNumElements(in,"ssy")>=0||api->mapNumElements(in,"sst")>=0;
        sigmaScalarKnown=!location&&!axes;
        if(const char* option=std::getenv("DFTTESTVK_SCALAR_SIGMA"); option&&std::string(option)=="0") sigmaScalarKnown=false;
        sigmaScalar=sigma*(originalType<2?wscale:1.0);
        auto sx=profile(in,location?"slocation":"ssx",sigma,location&&system==1,api);
        auto sy=profile(in,location?"slocation":"ssy",sigma,location&&system==1,api);
        auto st=profile(in,location?"slocation":"sst",sigma,location&&system==1,api);
        const int dimensions=(B>1?2:0)+(T>1?1:0);
        auto frequency=[](int i,int n){return n==1?0.0:double(std::min(i,n-i))/(n/2);};
        for(int t=0;t<T;++t) for(int y=0;y<B;++y) for(int x=0;x<H;++x) {
            double s=sigma;
            if(location||axes) {
                double fx=frequency(x,B),fy=frequency(y,B),ft=frequency(t,T);
                // A 1x1x1 block has only DC: common/temporal profiles are
                // evaluated at zero, while absent temporal data uses sigma.
                if(dimensions==0) s=st(0);
                else if(system==0) s=(T==1?1:st(ft))*(B==1?1:sy(fy))*(B==1?1:sx(fx));
                else s=st(std::sqrt((fx*fx+fy*fy+ft*ft)/std::max(dimensions,1)));
            }
            if(originalType<2) s*=wscale;
            if(!std::isfinite(s)||s>std::numeric_limits<float>::max()) fail("sigma profile is out of binary32 range");
            data.push_back(float(s));
        }
        if(originalType<2) sigma2*=wscale;
        pmin=filterType==5 ? beta : pmin*wscale;pmax*=wscale;
        auto finite32=[](double value){return std::isfinite(value)&&std::abs(value)<=std::numeric_limits<float>::max();};
        if(!finite32(wscale)||!finite32(pmin)||!finite32(pmax)||!finite32(sigma2)) fail("window or filter constants exceed binary32 range");
        for(float value:data) if(!std::isfinite(value)) fail("window constants exceed binary32 range");
        if(embedConstants) {
            constantShaderText="const float constants["+std::to_string(data.size())+"]=float["+std::to_string(data.size())+"](";
            for(size_t i=0;i<data.size();++i) {if(i)constantShaderText+=",";constantShaderText+=literal(data[i]);}
            constantShaderText+=");\n";
        }
        if(rowConstants) {
            rowShaderText="float analysisWindow(int row,int lane) {switch(row){\n";
            for(int row=0;row<T*B;++row) {
                rowShaderText+="case "+std::to_string(row)+":{const float w["+std::to_string(B)+"]=float["+std::to_string(B)+"](";
                for(int x=0;x<B;++x){if(x)rowShaderText+=",";rowShaderText+=literal(data[row*B+x]);}
                rowShaderText+=");return w[lane];}\n";
            }
            rowShaderText+="}return 0.0;}\nvec2 windowSpectrum(int row,int lane){switch(row){\n";
            for(int row=0;row<T*B;++row) {
                rowShaderText+="case "+std::to_string(row)+":{const vec2 w["+std::to_string(H)+"]=vec2["+std::to_string(H)+"](";
                for(int x=0;x<H;++x) {
                    if(x)rowShaderText+=",";size_t at=T*B*B+2*(row*H+x);
                    rowShaderText+="vec2("+literal(data[at])+","+literal(data[at+1])+")";
                }
                rowShaderText+=");return w[lane];}\n";
            }
            rowShaderText+="}return vec2(0.0);}\nfloat synthesisWindow(int y,int x){return analysisWindow("+std::to_string(T/2*B)+"+y,x);}\n";
        }
        char error[errorSize]{};
        constants=gpu->createGPUBuffer(core,data.size()*sizeof(float),VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            deviceConstants ? VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT : VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            0,&constantInfo,error,errorSize);
        if(!constants) fail(error);
        if(profileEnabled) std::fprintf(stderr,"DFTTESTVK_PROFILE constants_bytes=%zu memory_flags=%u\n",data.size()*sizeof(float),unsigned(constantInfo.memoryFlags));
        if(!deviceConstants) std::memcpy(constantInfo.mapped,data.data(),data.size()*sizeof(float));
        else {
            auto abandon=[&](VSGPUExecContext* p){if(p)gpu->gpuExecAbandon(p);};
            std::unique_ptr<VSGPUExecContext,decltype(abandon)> recording(gpu->gpuExecAcquire(pool,error,errorSize),abandon);
            if(!recording) fail(error);
            VkCommandBuffer cmd=gpu->gpuExecCommandBuffer(recording.get());
            const size_t bytes=data.size()*sizeof(float);
            for(size_t offset=0;offset<bytes;offset+=65536) {
                size_t count=std::min(size_t(65536),bytes-offset);
                vk->vkCmdUpdateBuffer(cmd,constantInfo.buffer,offset,count,reinterpret_cast<const char*>(data.data())+offset);
            }
            VkMemoryBarrier2 memory{VK_STRUCTURE_TYPE_MEMORY_BARRIER_2};memory.srcStageMask=VK_PIPELINE_STAGE_2_TRANSFER_BIT;
            memory.srcAccessMask=VK_ACCESS_2_TRANSFER_WRITE_BIT;memory.dstStageMask=VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
            memory.dstAccessMask=VK_ACCESS_2_SHADER_STORAGE_READ_BIT;
            VkDependencyInfo dependency{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};dependency.memoryBarrierCount=1;dependency.pMemoryBarriers=&memory;
            vk->vkCmdPipelineBarrier2(cmd,&dependency);
            uint64_t value{};
            if(gpu->gpuExecSubmit(recording.release(),&value,error,errorSize)) fail(error);
            if(gpu->gpuExecWaitValue(pool,value,error,errorSize)) fail(error);
        }
    }
    void makeKernel(Kernel& kernel,const char* source,VSCore* core) {
        std::array<VkDescriptorSetLayoutBinding,10> bindings{};
        for(uint32_t i=0;i<bindings.size();++i) bindings[i]={i,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr};
        VkDescriptorSetLayoutCreateInfo set{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        set.flags=VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT;set.bindingCount=uint32_t(bindings.size());set.pBindings=bindings.data();
        check(vk->vkCreateDescriptorSetLayout(handles.device,&set,nullptr,&kernel.descriptors),"descriptor layout");
        VkPushConstantRange range{VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(Push)};
        VkPipelineLayoutCreateInfo layout{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        layout.setLayoutCount=1;layout.pSetLayouts=&kernel.descriptors;layout.pushConstantRangeCount=1;layout.pPushConstantRanges=&range;
        check(vk->vkCreatePipelineLayout(handles.device,&layout,nullptr,&kernel.layout),"pipeline layout");
        const auto& format=vi.format;
        std::string type=format.sampleType==stFloat ? (format.bitsPerSample==16 ? "float16_t":"float") : (format.bytesPerSample==1?"uint8_t":"uint16_t");
        double scale=format.sampleType==stFloat?255.0:1.0/(1<<(format.bitsPerSample-8));
        std::string text="#version 460\n#define SAMPLE "+type+"\n#define INTEGER "+std::to_string(format.sampleType==stInteger)+
            "\n#define SCALE "+literal(scale)+"\n#define PEAK "+literal(format.sampleType==stInteger ? (1<<format.bitsPerSample)-1:1)+
            "\n#define N "+std::to_string(blockSize)+"\n#define T "+std::to_string(temporal)+"\n#define STEP "+std::to_string(step)+
            "\n#define TILES "+std::to_string(tiles)+"\n#define SUBGROUP "+std::to_string(useSubgroup)+"\n#define UNROLL "+std::to_string(unroll)+
            "\n#define STATIC_MAIN "+std::to_string(staticMain)+"\n#define SHORT_REFLECT "+std::to_string(shortReflect)+
            "\n#define FFT_FMA "+std::to_string(fftFMA)+"\n#define REAL_SHARED "+std::to_string(realShared)+
            "\n#define EMBED_CONSTANTS "+std::to_string(embedConstants)+"\n#define SHARED_PAD "+std::to_string(sharedPad)+
            "\n#define ROW_CONSTANTS "+std::to_string(rowConstants)+
            "\n#define WARP_TILE "+std::to_string(warpTile)+
            "\n#define FTYPE "+std::to_string(filterType)+"\n#define ZMEAN "+std::to_string(zeroMean)+
            "\n#define SIGMA_IS_SCALAR "+std::to_string(sigmaScalarKnown)+"\n#define SIGMA_SCALAR "+literal(sigmaScalarKnown?sigmaScalar:0.0)+
            "\n#define SIGMA2 "+literal(sigma2)+"\n#define PMIN "+literal(pmin)+"\n#define PMAX "+literal(pmax)+"\n";
        if(embedConstants||rowConstants||temporalCache) {
            // Extensions precede the injected declaration and the shader's later extension directives.
            text+="#extension GL_EXT_shader_8bit_storage : enable\n#extension GL_EXT_shader_16bit_storage : enable\n";
            text+="#extension GL_EXT_shader_explicit_arithmetic_types_int8 : enable\n#extension GL_EXT_shader_explicit_arithmetic_types_int16 : enable\n";
            text+="#extension GL_EXT_shader_explicit_arithmetic_types_float16 : enable\n#extension GL_EXT_control_flow_attributes : enable\n";
            if(useSubgroup) text+="#extension GL_KHR_shader_subgroup_basic : require\n";
            text+=constantShaderText;
            text+=rowShaderText;
            text+=temporalShaderText;
        }
        text+=source;
        char error[errorSize]{};VSGPUShader* shader=gpu->compileGPUShader(core,slGLSL,text.c_str(),error,errorSize);
        if(!shader) fail(error);
        auto deleter=[&](VSGPUShader* p){gpu->freeGPUShader(p);};
        std::unique_ptr<VSGPUShader,decltype(deleter)> owned(shader,deleter);
        size_t bytes{};const uint32_t* code=gpu->getGPUShaderCode(shader,&bytes);
        std::vector<uint32_t> overrideCode;
        const char* kernelName=(&kernel==&kernels[0])?"fused":((&kernel==&kernels[1])?"gather":"forward");
        if(const char* folder=std::getenv("DFTTESTVK_DUMP_SPIRV")) {
            std::filesystem::create_directories(folder);
            std::ofstream output(std::filesystem::path(folder)/(std::string(kernelName)+".spv"),std::ios::binary);
            output.write(reinterpret_cast<const char*>(code),bytes);if(!output)fail("cannot dump diagnostic SPIR-V");
        }
        if(const char* folder=std::getenv("DFTTESTVK_LOAD_SPIRV")) {
            std::ifstream input(std::filesystem::path(folder)/(std::string(kernelName)+".spv"),std::ios::binary|std::ios::ate);
            if(!input)fail("cannot load diagnostic SPIR-V");
            auto size=input.tellg();if(size<20||size%4)fail("invalid diagnostic SPIR-V size");
            overrideCode.resize(size_t(size)/4);input.seekg(0);input.read(reinterpret_cast<char*>(overrideCode.data()),size);
            if(!input||overrideCode[0]!=0x07230203u)fail("invalid diagnostic SPIR-V module");
            code=overrideCode.data();bytes=overrideCode.size()*4;
        }
        VkShaderModuleCreateInfo module{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};module.codeSize=bytes;module.pCode=code;
        VkComputePipelineCreateInfo pipeline{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
        pipeline.stage.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;pipeline.stage.pNext=&module;
        VkPipelineShaderStageRequiredSubgroupSizeCreateInfo subgroup{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_REQUIRED_SUBGROUP_SIZE_CREATE_INFO};
        if(useSubgroup) {
            subgroup.requiredSubgroupSize=32;module.pNext=&subgroup;
            pipeline.stage.flags|=VK_PIPELINE_SHADER_STAGE_CREATE_REQUIRE_FULL_SUBGROUPS_BIT;
        }
        pipeline.stage.stage=VK_SHADER_STAGE_COMPUTE_BIT;pipeline.stage.pName="main";pipeline.layout=kernel.layout;
        check(vk->vkCreateComputePipelines(handles.device,VK_NULL_HANDLE,1,&pipeline,nullptr,&kernel.pipeline),"compute pipeline");
    }
};
struct ProfileFrame {
    Filter* filter{};VkQueryPool queries{};
    ~ProfileFrame(){if(queries)filter->vk->vkDestroyQueryPool(filter->handles.device,queries,nullptr);}
    static void VS_CC release(void* opaque) {
        std::unique_ptr<ProfileFrame> frame(static_cast<ProfileFrame*>(opaque));auto* f=frame->filter;
        for(int p=0;p<f->vi.format.numPlanes;++p) if(f->process[p]) {
            uint64_t times[3]{};
            VkResult result=f->vk->vkGetQueryPoolResults(f->handles.device,frame->queries,p*3,3,sizeof(times),times,sizeof(uint64_t),VK_QUERY_RESULT_64_BIT);
            // Abandoned recordings have no results; callbacks never wait on an unsubmitted query.
            if(result!=VK_SUCCESS) continue;
            uint64_t mask=f->timestampBits==64 ? UINT64_MAX:((uint64_t(1)<<f->timestampBits)-1);
            f->fusedNs[p].fetch_add(uint64_t(double((times[1]-times[0])&mask)*f->timestampPeriod),std::memory_order_relaxed);
            f->gatherNs[p].fetch_add(uint64_t(double((times[2]-times[1])&mask)*f->timestampPeriod),std::memory_order_relaxed);
            f->profileFrames[p].fetch_add(1,std::memory_order_relaxed);
        }
    }
};
struct FrameDeleter {const VSAPI* api;void operator()(const VSFrame* p)const{if(p)api->freeFrame(p);}};
using Frame=std::unique_ptr<const VSFrame,FrameDeleter>;
struct Recording {const VSVULKANAPI* gpu;VSGPUExecContext* context;~Recording(){if(context)gpu->gpuExecAbandon(context);}};
int paddedSize(int size,int block,int step) {
    int64_t value=((int64_t(size)+block-1)/block)*block+2*std::max(block-step,step);
    if(value>std::numeric_limits<int>::max()) fail("padded dimension is too large");
    return int(value);
}
std::shared_ptr<CachedSpectrum> acquireSpectrum(Filter& f,int n,const VSFrame* source,VSCore* core) {
    std::shared_ptr<CachedSpectrum> entry;bool creator=false;
    {
        std::lock_guard lock(f.cacheMutex);
        auto it=f.spectra.find(n);
        if(it!=f.spectra.end()){f.cacheHits.fetch_add(1);entry=it->second;entry->tick=++f.cacheTick;}
        else {
            entry=std::make_shared<CachedSpectrum>();entry->api=f.api;entry->gpu=f.gpu;entry->tick=++f.cacheTick;
            f.spectra.emplace(n,entry);creator=true;
        }
    }
    if(!creator){entry->ready.get();return entry;}
    f.cacheMisses.fetch_add(1);
    try {
    char error[errorSize]{};constexpr int packedStride=16*9*2;
    entry->storage=f.gpu->createGPUBuffer(core,VkDeviceSize(f.cacheRows)*packedStride*4,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,0,&entry->info,error,errorSize);
    if(!entry->storage)fail(error);
    VSVideoFormat tokenFormat{};f.api->queryVideoFormat(&tokenFormat,cfGray,stFloat,32,0,0,core);
    entry->token=f.gpu->newGPUVideoFrame(&tokenFormat,1,1,nullptr,core);if(!entry->token)fail("FFT cache token allocation failed");
    Recording record{f.gpu,f.gpu->gpuExecAcquire(f.cachePool,error,errorSize)};if(!record.context)fail(error);
    f.gpu->gpuExecRetain(record.context,CachedSpectrum::release,new std::shared_ptr<CachedSpectrum>(entry),entry->info.size);
    f.gpu->gpuExecReadsFrame(record.context,source);f.gpu->gpuExecWritesPlane(record.context,entry->token,0);
    VkCommandBuffer cmd=f.gpu->gpuExecCommandBuffer(record.context);
    VSVulkanPlaneInfo token{};if(f.gpu->getGPUPlane(entry->token,0,&token))fail("FFT cache token is not GPU resident");
    f.vk->vkCmdFillBuffer(cmd,token.buffer,0,token.bufferSize,0);
    for(int p=0;p<f.vi.format.numPlanes;++p)if(f.process[p]) {
        Push push{};push.width=f.api->getFrameWidth(source,p);push.height=f.api->getFrameHeight(source,p);push.dstStride=packedStride;
        int pw=paddedSize(push.width,f.blockSize,f.step),ph=paddedSize(push.height,f.blockSize,f.step);
        push.blocksX=(pw-f.blockSize)/f.step+1;push.blocksY=(ph-f.blockSize)/f.step+1;
        push.padX=(pw-push.width)/2;push.padY=(ph-push.height)/2;
        int stride=int(f.api->getStride(source,p)/f.vi.format.bytesPerSample);
        VSVulkanPlaneInfo raw{};if(f.gpu->getGPUPlane(source,p,&raw))fail("FFT cache input is not GPU resident");
        if(f.cacheDeviceCopy) {
            VSVulkanBufferInfo local{};
            auto* owner=f.gpu->createGPUBuffer(core,raw.bufferSize,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,0,&local,error,errorSize);
            if(!owner)fail(error);f.gpu->gpuExecUsesBuffer(record.context,owner);
            VkBufferCopy2 region{VK_STRUCTURE_TYPE_BUFFER_COPY_2};region.size=raw.bufferSize;
            VkCopyBufferInfo2 copy{VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2};copy.srcBuffer=raw.buffer;copy.dstBuffer=local.buffer;
            copy.regionCount=1;copy.pRegions=&region;f.vk->vkCmdCopyBuffer2(cmd,&copy);
            VkMemoryBarrier2 copied{VK_STRUCTURE_TYPE_MEMORY_BARRIER_2};copied.srcStageMask=VK_PIPELINE_STAGE_2_TRANSFER_BIT;
            copied.srcAccessMask=VK_ACCESS_2_TRANSFER_WRITE_BIT;copied.dstStageMask=VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
            copied.dstAccessMask=VK_ACCESS_2_SHADER_STORAGE_READ_BIT;
            VkDependencyInfo available{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};available.memoryBarrierCount=1;available.pMemoryBarriers=&copied;
            f.vk->vkCmdPipelineBarrier2(cmd,&available);raw.buffer=local.buffer;
        }
        std::array<VkDescriptorBufferInfo,10> buffers{};
        for(int i=0;i<7;++i){buffers[i]={raw.buffer,0,raw.bufferSize};push.srcStride[i]=stride;}
        buffers[7]=buffers[8]={f.constantInfo.buffer,0,f.constantInfo.size};
        VkDeviceSize offset=VkDeviceSize(f.cacheRow[p])*packedStride*4,range=VkDeviceSize(push.blocksX)*push.blocksY*packedStride*4;
        if(offset+range>entry->info.size)fail("FFT cache descriptor exceeds allocated storage");
        buffers[9]={entry->info.buffer,offset,range};
        std::array<VkWriteDescriptorSet,10> writes{};
        for(uint32_t i=0;i<writes.size();++i){auto& w=writes[i];w.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            w.dstBinding=i;w.descriptorCount=1;w.descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;w.pBufferInfo=&buffers[i];}
        auto& k=f.kernels[2];f.vk->vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_COMPUTE,k.pipeline);
        f.vk->vkCmdPushDescriptorSet(cmd,VK_PIPELINE_BIND_POINT_COMPUTE,k.layout,0,uint32_t(writes.size()),writes.data());
        VkPushConstantsInfo values{VK_STRUCTURE_TYPE_PUSH_CONSTANTS_INFO};values.layout=k.layout;values.stageFlags=VK_SHADER_STAGE_COMPUTE_BIT;
        values.size=sizeof(push);values.pValues=&push;f.vk->vkCmdPushConstants2(cmd,&values);
        uint32_t groups=(push.blocksX*push.blocksY+f.tiles-1)/f.tiles,x=std::min(groups,f.maxGroupsX);
        f.vk->vkCmdDispatch(cmd,x,(groups+x-1)/x,1);
    }
    // Producer waits on the token carry completion; this also makes storage
    // writes available to the next compute submission on the shared queue.
    VkMemoryBarrier2 memory{VK_STRUCTURE_TYPE_MEMORY_BARRIER_2};memory.srcStageMask=VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    memory.srcAccessMask=VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;memory.dstStageMask=VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    memory.dstAccessMask=VK_ACCESS_2_SHADER_STORAGE_READ_BIT;
    VkDependencyInfo dependency{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};dependency.memoryBarrierCount=1;dependency.pMemoryBarriers=&memory;
    f.vk->vkCmdPipelineBarrier2(cmd,&dependency);
    auto* recording=record.context;record.context=nullptr;
    if(f.gpu->gpuExecSubmit(recording,nullptr,error,errorSize))fail(error);
    std::vector<std::shared_ptr<CachedSpectrum>> evicted;
    {
        std::lock_guard lock(f.cacheMutex);
        entry->pending=false;
        while(f.spectra.size()>size_t(f.cacheFrames)) {
            auto oldest=f.spectra.end();
            for(auto i=f.spectra.begin();i!=f.spectra.end();++i)if(!i->second->pending &&
                (oldest==f.spectra.end()||i->second->tick<oldest->second->tick))oldest=i;
            if(oldest==f.spectra.end())break;
            evicted.push_back(oldest->second);
            f.spectra.erase(oldest);
        }
    }
    entry->submitted.set_value();return entry;
    } catch(...) {
        entry->submitted.set_exception(std::current_exception());
        {std::lock_guard lock(f.cacheMutex);auto i=f.spectra.find(n);if(i!=f.spectra.end()&&i->second==entry)f.spectra.erase(i);}
        throw;
    }
}
const VSFrame* VS_CC getFrame(int n,int reason,void* instance,void**,VSFrameContext* ctx,VSCore* core,const VSAPI* api) {
    auto* f=static_cast<Filter*>(instance);int radius=f->temporal/2,last=f->vi.numFrames-1;
    if(reason==arInitial) {
        for(int i=std::max(n-radius,0);i<=std::min(n+radius,last);++i) api->requestFrameFilter(i,f->source,ctx);
        return nullptr;
    }
    if(reason!=arAllFramesReady) return nullptr;
    try {
        std::vector<Frame> frames;frames.reserve(f->temporal);
        for(int i=0;i<f->temporal;++i) frames.emplace_back(api->getFrameFilter(std::clamp(n+i-radius,0,last),f->source,ctx),FrameDeleter{api});
        const VSFrame* center=frames[radius].get();
        std::vector<std::shared_ptr<CachedSpectrum>> cached;
        if(f->temporalCache)for(int t=0;t<f->temporal;++t)cached.push_back(acquireSpectrum(*f,std::clamp(n+t-radius,0,last),frames[t].get(),core));
        const VSFrame* planeSources[3]{};int planeIndices[3]={0,1,2};
        bool sharesPlane=false;
        for(int i=0;i<f->vi.format.numPlanes;++i) if(!f->process[i]) {planeSources[i]=center;sharesPlane=true;}
        VSFrame* output=sharesPlane ? api->newVideoFrame2(&f->vi.format,f->vi.width,f->vi.height,planeSources,planeIndices,center,core) :
            f->gpu->newGPUVideoFrame(&f->vi.format,f->vi.width,f->vi.height,center,core);
        if(!output) fail("output allocation failed");
        Frame dst{output,FrameDeleter{api}};char error[errorSize]{};
        Recording record{f->gpu,f->gpu->gpuExecAcquire(f->pool,error,errorSize)};
        if(!record.context) fail(error);
        if(f->temporalCache)for(int t=0;t<f->temporal;++t) {
            if(t==0||cached[t]!=cached[t-1]) {
                f->gpu->gpuExecReadsFrame(record.context,cached[t]->token);
                f->gpu->gpuExecRetain(record.context,CachedSpectrum::release,new std::shared_ptr<CachedSpectrum>(cached[t]),cached[t]->info.size);
            }
        }
        for(int t=0;t<f->temporal;++t) {
            // Slice descriptors may repeat endpoints; retain/request each real input only once.
            int frameNumber=std::clamp(n+t-radius,0,last);
            if(t==0 || frameNumber!=std::clamp(n+t-1-radius,0,last)) f->gpu->gpuExecReadsFrame(record.context,frames[t].get());
        }
        auto cmd=f->gpu->gpuExecCommandBuffer(record.context);
        VkQueryPool queries{};
        if(f->profileEnabled) {
            auto profiling=std::make_unique<ProfileFrame>();profiling->filter=f;
            VkQueryPoolCreateInfo query{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};query.queryType=VK_QUERY_TYPE_TIMESTAMP;query.queryCount=9;
            check(f->vk->vkCreateQueryPool(f->handles.device,&query,nullptr,&profiling->queries),"timestamp query pool");
            queries=profiling->queries;
            f->gpu->gpuExecRetain(record.context,ProfileFrame::release,profiling.release(),0);
            f->vk->vkCmdResetQueryPool(cmd,queries,0,9);
        }
        // One scratch allocation reused between planes, bounded by the largest plane.
        int pw=paddedSize(f->vi.width,f->blockSize,f->step),ph=paddedSize(f->vi.height,f->blockSize,f->step);
        VkDeviceSize scratchSize=VkDeviceSize((pw-f->blockSize)/f->step+1)*((ph-f->blockSize)/f->step+1)*f->blockSize*f->blockSize*sizeof(float);
        VSVulkanBufferInfo scratchInfo{};
        auto* scratch=f->gpu->createGPUBuffer(core,scratchSize,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,0,&scratchInfo,error,errorSize);
        if(!scratch) fail(error);
        f->gpu->gpuExecUsesBuffer(record.context,scratch);
        std::array<VkDescriptorBufferInfo,10> buffers{};
        buffers[7]={scratchInfo.buffer,0,scratchSize};buffers[8]={f->constantInfo.buffer,0,f->constantInfo.size};
        auto barrier=[&]() {
            VkMemoryBarrier2 memory{VK_STRUCTURE_TYPE_MEMORY_BARRIER_2};
            memory.srcStageMask=memory.dstStageMask=VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
            memory.srcAccessMask=VK_ACCESS_2_SHADER_STORAGE_READ_BIT|VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
            memory.dstAccessMask=VK_ACCESS_2_SHADER_STORAGE_READ_BIT|VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
            VkDependencyInfo dependency{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};dependency.memoryBarrierCount=1;dependency.pMemoryBarriers=&memory;
            f->vk->vkCmdPipelineBarrier2(cmd,&dependency);
        };
        for(int plane=0;plane<f->vi.format.numPlanes;++plane) {
            if(!f->process[plane]) continue;
            f->gpu->gpuExecWritesPlane(record.context,output,plane);
            Push push{};push.width=api->getFrameWidth(center,plane);push.height=api->getFrameHeight(center,plane);
            push.dstStride=int32_t(api->getStride(output,plane)/f->vi.format.bytesPerSample);
            pw=paddedSize(push.width,f->blockSize,f->step);ph=paddedSize(push.height,f->blockSize,f->step);
            push.blocksX=(pw-f->blockSize)/f->step+1;push.blocksY=(ph-f->blockSize)/f->step+1;
            push.padX=(pw-push.width)/2;push.padY=(ph-push.height)/2;
            for(int t=0;t<7;++t) {
                const VSFrame* src=frames[t<f->temporal?t:radius].get();VSVulkanPlaneInfo info{};
                if(f->temporalCache) {
                    auto& entry=cached[t<f->temporal?t:radius];
                    VkDeviceSize offset=VkDeviceSize(f->cacheRow[plane])*16*9*2*4,range=VkDeviceSize(push.blocksX)*push.blocksY*16*9*2*4;
                    buffers[t]={entry->info.buffer,offset,range};push.srcStride[t]=16*9*2;
                }else {
                    if(f->gpu->getGPUPlane(src,plane,&info)) fail("input is not GPU resident");
                    buffers[t]={info.buffer,0,info.bufferSize};push.srcStride[t]=int32_t(api->getStride(src,plane)/f->vi.format.bytesPerSample);
                }
            }
            if(!f->temporalCache&&f->cacheDeviceCopy) {
                std::unordered_map<VkBuffer,VkBuffer> localCopies;
                for(int t=0;t<f->temporal;++t) {
                    VkBuffer inputBuffer=buffers[t].buffer;
                    auto found=localCopies.find(inputBuffer);
                    if(found!=localCopies.end()){buffers[t].buffer=found->second;continue;}
                    VSVulkanBufferInfo local{};
                    auto* owner=f->gpu->createGPUBuffer(core,buffers[t].range,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,0,&local,error,errorSize);
                    if(!owner)fail(error);f->gpu->gpuExecUsesBuffer(record.context,owner);
                    VkBufferCopy2 region{VK_STRUCTURE_TYPE_BUFFER_COPY_2};region.size=buffers[t].range;
                    VkCopyBufferInfo2 copy{VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2};copy.srcBuffer=inputBuffer;copy.dstBuffer=local.buffer;
                    copy.regionCount=1;copy.pRegions=&region;f->vk->vkCmdCopyBuffer2(cmd,&copy);
                    localCopies.emplace(inputBuffer,local.buffer);buffers[t].buffer=local.buffer;
                }
                VkMemoryBarrier2 copied{VK_STRUCTURE_TYPE_MEMORY_BARRIER_2};copied.srcStageMask=VK_PIPELINE_STAGE_2_TRANSFER_BIT;
                copied.srcAccessMask=VK_ACCESS_2_TRANSFER_WRITE_BIT;copied.dstStageMask=VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
                copied.dstAccessMask=VK_ACCESS_2_SHADER_STORAGE_READ_BIT;
                VkDependencyInfo available{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};available.memoryBarrierCount=1;available.pMemoryBarriers=&copied;
                f->vk->vkCmdPipelineBarrier2(cmd,&available);
            }
            VSVulkanPlaneInfo info{};if(f->gpu->getGPUPlane(output,plane,&info)) fail("output is not GPU resident");
            buffers[9]={info.buffer,0,info.bufferSize};
            std::array<VkWriteDescriptorSet,10> writes{};
            for(uint32_t i=0;i<writes.size();++i) {auto& write=writes[i];write.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                write.dstBinding=i;write.descriptorCount=1;write.descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;write.pBufferInfo=&buffers[i];}
            for(int pass=0;pass<2;++pass) {
                if(queries && pass==0) f->vk->vkCmdWriteTimestamp2(cmd,VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,queries,plane*3);
                auto& k=f->kernels[pass];f->vk->vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_COMPUTE,k.pipeline);
                f->vk->vkCmdPushDescriptorSet(cmd,VK_PIPELINE_BIND_POINT_COMPUTE,k.layout,0,uint32_t(writes.size()),writes.data());
                VkPushConstantsInfo constants{VK_STRUCTURE_TYPE_PUSH_CONSTANTS_INFO};constants.layout=k.layout;
                constants.stageFlags=VK_SHADER_STAGE_COMPUTE_BIT;constants.size=sizeof(push);constants.pValues=&push;
                f->vk->vkCmdPushConstants2(cmd,&constants);
                if(pass==0) {
                    uint32_t groups=(push.blocksX*push.blocksY+f->tiles-1)/f->tiles;
                    uint32_t x=std::min(groups,f->maxGroupsX),y=(groups+x-1)/x;
                    f->vk->vkCmdDispatch(cmd,x,y,1);
                }
                else f->vk->vkCmdDispatch(cmd,(push.width+15)/16,(push.height+15)/16,1);
                barrier();
                if(queries) f->vk->vkCmdWriteTimestamp2(cmd,VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,queries,plane*3+pass+1);
            }
        }
        auto* recording=record.context;record.context=nullptr;
        if(f->gpu->gpuExecSubmit(recording,nullptr,error,errorSize)) fail(error);
        return dst.release();
    } catch(const std::exception& e) {api->setFilterError(e.what(),ctx);return nullptr;}
}
void VS_CC freeFilter(void* data,VSCore*,const VSAPI*) {delete static_cast<Filter*>(data);}
void VS_CC createGPU(const VSMap* in,VSMap* out,void*,VSCore* core,const VSAPI* api) {
    try {
        auto f=std::make_unique<Filter>();f->api=api;f->gpu=api->getVulkanAPI();
        f->source=api->mapGetNode(in,"clip",0,nullptr);f->vi=*api->getVideoInfo(f->source);
        const auto& format=f->vi.format;
        if(f->vi.width<=0||f->vi.height<=0||format.colorFamily==cfUndefined||f->vi.numFrames<=0) fail("expected constant format and dimensions");
        if(api->getNodeResidency(f->source)!=nrGPU) fail("DFTTestGPU expects a GPU clip");
        if((format.sampleType==stInteger && (format.bitsPerSample<8||format.bitsPerSample>16))||
           (format.sampleType==stFloat && format.bitsPerSample!=16&&format.bitsPerSample!=32)) fail("supported samples are 8-16 bit integer and 16/32 bit float");
        f->blockSize=integer(in,"sbsize",16,api);int overlap=integer(in,"sosize",12,api);
        f->temporal=integer(in,"tbsize",3,api);f->filterType=integer(in,"ftype",0,api);f->zeroMean=integer(in,"zmean",1,api)!=0;
        if(f->blockSize<1||f->blockSize>64) fail("sbsize must be in 1..64");
        if(f->blockSize!=16) f->tiles=1;
        if(f->temporal<1||f->temporal>7||f->temporal%2==0) fail("tbsize must be 1, 3, 5 or 7");
        int cacheMode=integer(in,"cache",-1,api);f->cacheFrames=integer(in,"cache_frames",12,api);
        int cacheMB=integer(in,"cache_mb",0,api);
        if(cacheMode<-1||cacheMode>1||f->cacheFrames<1||f->cacheFrames>128||cacheMB<0) fail("invalid cache/cache_frames/cache_mb");
        f->temporalCache=cacheMode==1||(cacheMode==-1&&f->blockSize==16&&f->temporal>1);
        if(f->temporalCache&&f->blockSize!=16)fail("FFT caching requires sbsize=16; select cache=0 for other block sizes");
        if(const char* option=std::getenv("DFTTESTVK_TEMPORAL_CACHE")) {
            f->temporalCache=std::string(option)!="0";
            if(f->temporalCache&&f->blockSize!=16)fail("FFT caching requires sbsize=16");
        }
        if(const char* option=std::getenv("DFTTESTVK_CACHE_DEVICE_COPY");option&&std::string(option)!="0") f->cacheDeviceCopy=true;
        if(const char* option=std::getenv("DFTTESTVK_CACHE_FRAMES")) {
            char* end{};long value=std::strtol(option,&end,10);
            if(*end||value<1||value>128)fail("DFTTESTVK_CACHE_FRAMES must be in 1..128");
            f->cacheFrames=int(value);
        }
        if(overlap<0||overlap>=f->blockSize) fail("sosize must be in 0..sbsize-1");
        f->step=f->blockSize-overlap;
        if(overlap>f->blockSize/2 && f->blockSize%f->step) fail("overlap greater than 50% requires step to divide sbsize");
        if(integer(in,"smode",1,api)!=1 || integer(in,"tmode",0,api)!=0 || integer(in,"tosize",0,api)!=0)
            fail("this implementation uses smode=1, tmode=0 and tosize=0, as DFTTest2");
        if(f->filterType<0||f->filterType>4) fail("ftype must be in 0..4");
        int count=api->mapNumElements(in,"planes");
        if(count>0) {
            f->process.fill(false);
            for(int i=0;i<count;++i) {int64_t plane=api->mapGetInt(in,"planes",i,nullptr);
                if(plane<0||plane>=format.numPlanes||f->process[size_t(plane)]) fail("planes must be distinct valid indices");
                f->process[size_t(plane)]=true;}
        }
        bool any=false;for(int p=0;p<format.numPlanes;++p) any|=f->process[p];
        if(!any) {api->mapSetNode(out,"clip",f->source,maReplace);return;}
        char error[errorSize]{};
        if(f->gpu->getVulkanHandles(core,&f->handles,error,errorSize)) fail(error);
        f->vk=f->gpu->getVulkanFunctions(core,error,errorSize);if(!f->vk) fail(error);
        VkPhysicalDeviceSubgroupSizeControlProperties subgroupLimits{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_PROPERTIES};
        VkPhysicalDeviceSubgroupProperties subgroupProperties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES};subgroupProperties.pNext=&subgroupLimits;
        VkPhysicalDeviceProperties2 properties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};properties.pNext=&subgroupProperties;
        f->vk->vkGetPhysicalDeviceProperties2(f->handles.physicalDevice,&properties);const auto& limits=properties.properties.limits;
        f->maxGroupsX=limits.maxComputeWorkGroupCount[0];f->maxGroupsY=limits.maxComputeWorkGroupCount[1];
        f->deviceConstants=true;
        f->cacheDeviceCopy=properties.properties.deviceType==VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
        if(const char* option=std::getenv("DFTTESTVK_CACHE_DEVICE_COPY"))f->cacheDeviceCopy=std::string(option)!="0";
        if(const char* option=std::getenv("DFTTESTVK_DEVICE_CONSTANTS"))f->deviceConstants=std::string(option)!="0";
        if(const char* option=std::getenv("DFTTESTVK_EMBED_CONSTANTS");option&&std::string(option)!="0") f->embedConstants=true;
        if(const char* option=std::getenv("DFTTESTVK_ROW_CONSTANTS");option&&std::string(option)!="0") f->rowConstants=true;
        if(f->blockSize==16) {
            f->tiles=16;
            while(f->tiles>2&&(uint32_t(f->tiles*16)>limits.maxComputeWorkGroupInvocations||
                uint32_t(f->tiles*16)>limits.maxComputeWorkGroupSize[0]||uint32_t(f->tiles*(16*17*8+4))>limits.maxComputeSharedMemorySize)) f->tiles/=2;
            f->staticMain=true;f->shortReflect=true;
            if(const char* tileCount=std::getenv("DFTTESTVK_TILES")) {
                char* end{};long value=std::strtol(tileCount,&end,10);
                if(*end || (value!=2&&value!=4&&value!=8&&value!=16)) fail("DFTTESTVK_TILES must be 2, 4, 8 or 16");
                f->tiles=int(value);
            }
            bool subgroupSupported=(subgroupProperties.supportedStages&VK_SHADER_STAGE_COMPUTE_BIT)&&
                (subgroupProperties.supportedOperations&VK_SUBGROUP_FEATURE_BASIC_BIT)&&
                (subgroupLimits.requiredSubgroupSizeStages&VK_SHADER_STAGE_COMPUTE_BIT)&&
                subgroupLimits.minSubgroupSize<=32&&subgroupLimits.maxSubgroupSize>=32;
            if(const char* option=std::getenv("DFTTESTVK_SUBGROUP");option&&std::string(option)!="0") {
                if(!subgroupSupported) fail("DFTTESTVK_SUBGROUP requires controllable 32-lane compute subgroups");
                f->useSubgroup=true;
            }
            if(const char* option=std::getenv("DFTTESTVK_UNROLL");option&&std::string(option)!="0") f->unroll=true;
            if(const char* option=std::getenv("DFTTESTVK_STATIC_MAIN"))f->staticMain=std::string(option)!="0";
            if(const char* option=std::getenv("DFTTESTVK_SHORT_REFLECT"))f->shortReflect=std::string(option)!="0";
            if(const char* option=std::getenv("DFTTESTVK_FFT_FMA");option&&std::string(option)!="0") f->fftFMA=true;
            if(const char* option=std::getenv("DFTTESTVK_REAL_SHARED");option&&std::string(option)!="0") f->realShared=true;
            if(const char* option=std::getenv("DFTTESTVK_SHARED_PAD");option&&std::string(option)!="0") f->sharedPad=1;
            if(const char* option=std::getenv("DFTTESTVK_WARP_TILE");option&&std::string(option)!="0") {
                if(!f->useSubgroup) fail("DFTTESTVK_WARP_TILE requires DFTTESTVK_SUBGROUP=1");
                f->warpTile=true;
            }
        }
        if(const char* profiling=std::getenv("DFTTESTVK_PROFILE"); profiling && std::string(profiling)!="0") {
            uint32_t familyCount{};f->vk->vkGetPhysicalDeviceQueueFamilyProperties2(f->handles.physicalDevice,&familyCount,nullptr);
            std::vector<VkQueueFamilyProperties2> families(familyCount);
            for(auto& family:families)family.sType=VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2;
            f->vk->vkGetPhysicalDeviceQueueFamilyProperties2(f->handles.physicalDevice,&familyCount,families.data());
            f->timestampBits=families.at(f->handles.computeQueueFamily).queueFamilyProperties.timestampValidBits;
            f->timestampPeriod=limits.timestampPeriod;f->profileEnabled=f->timestampBits!=0;
            if(!f->profileEnabled) api->logMessage(mtWarning,"DFTTestVK: queue does not support timestamps",core);
        }
        int pw=paddedSize(f->vi.width,f->blockSize,f->step),ph=paddedSize(f->vi.height,f->blockSize,f->step);
        uint64_t blocks=uint64_t((pw-f->blockSize)/f->step+1)*((ph-f->blockSize)/f->step+1);
        uint32_t localThreads=f->blockSize==16 ? uint32_t(f->tiles*(f->warpTile?32:16)):64;
        if(limits.maxComputeWorkGroupInvocations<256 || limits.maxComputeWorkGroupSize[0]<64 || limits.maxComputeWorkGroupSize[1]<16 ||
           localThreads>limits.maxComputeWorkGroupInvocations || localThreads>limits.maxComputeWorkGroupSize[0] ||
           uint32_t(f->blockSize==16 ? f->tiles*f->blockSize*(f->blockSize+1)*8+f->tiles*(4+f->sharedPad*4) : f->blockSize*(f->blockSize+1)*8+4)>limits.maxComputeSharedMemorySize ||
           (blocks+f->tiles-1)/f->tiles>uint64_t(f->maxGroupsX)*f->maxGroupsY || blocks*f->blockSize*f->blockSize*4>limits.maxStorageBufferRange ||
           (uint64_t(f->vi.width)+15)/16>limits.maxComputeWorkGroupCount[0] || (uint64_t(f->vi.height)+15)/16>limits.maxComputeWorkGroupCount[1] ||
           blocks>uint64_t(std::numeric_limits<int32_t>::max())/uint64_t(f->blockSize*f->blockSize)) fail("dimensions or scratch exceed the device's compute limits");
        if(format.sampleType==stFloat&&format.bitsPerSample==16) {
            VkPhysicalDeviceVulkan12Features features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
            VkPhysicalDeviceFeatures2 query{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};query.pNext=&features;
            f->vk->vkGetPhysicalDeviceFeatures2(f->handles.physicalDevice,&query);
            if(!features.shaderFloat16) fail("device does not support float16 arithmetic");
        }
        if(f->temporalCache) {
            int rowWidth=f->blockSize*(f->blockSize/2+1)*2;
            uint64_t rowBytes=((uint64_t(rowWidth)*4+127)/128)*128;
            for(int p=0;p<format.numPlanes;++p)if(f->process[p]) {
                f->cacheRow[p]=f->cacheRows;
                int w=f->vi.width>>(p?format.subSamplingW:0),h=f->vi.height>>(p?format.subSamplingH:0);
                int px=paddedSize(w,f->blockSize,f->step),py=paddedSize(h,f->blockSize,f->step);
                uint64_t rows=uint64_t((px-f->blockSize)/f->step+1)*((py-f->blockSize)/f->step+1);
                if(rows+f->cacheRows>uint64_t(INT32_MAX)||rows*rowBytes>limits.maxStorageBufferRange)fail("spatial FFT cache exceeds device limits");
                f->cacheRows+=int(rows);
            }
            if(rowBytes%limits.minStorageBufferOffsetAlignment || rowBytes*f->cacheRows>limits.maxStorageBufferRange)
                fail("spatial FFT cache pitch or size exceeds buffer limits");
            VSVulkanCoreInfo info{};if(f->gpu->getVulkanCoreInfo(core,&info,error,errorSize))fail(error);
            uint64_t budget=cacheMB ? uint64_t(cacheMB)*1024*1024 : uint64_t(std::max(int64_t(0),info.limit))/4;
            uint64_t possible=budget/(rowBytes*f->cacheRows);
            if(possible==0) {
                if(cacheMode==1)fail("cache_mb is too small for one frame's spatial spectrum");
                f->temporalCache=false;f->cacheRows=0;
            } else f->cacheFrames=int(std::min(uint64_t(f->cacheFrames),possible));
        }
        f->pool=f->gpu->createGPUExecPool(core,vqCompute,error,errorSize);if(!f->pool) fail(error);
        f->makeConstants(in,core);
        f->makeKernel(f->kernels[0],f->temporalCache?cached_shader::finish:(f->blockSize==16?shader::fused:generic_shader::fused),core);
        f->makeKernel(f->kernels[1],shader::gather,core);
        if(f->temporalCache) {
            f->cachePool=f->gpu->createGPUExecPool(core,vqCompute,error,errorSize);if(!f->cachePool)fail(error);
            f->makeKernel(f->kernels[2],cached_shader::forward,core);
        }
        VSFilterDependency dependency{f->source,f->temporal==1 ? rpStrictSpatial:rpGeneral};
        auto* result=api->createVideoFilterEx2("DFTTestGPU",&f->vi,getFrame,freeFilter,fmParallel,ffGPUOutput,&dependency,1,f.get(),core);
        if(!result) fail("filter creation failed");
        f.release();api->mapConsumeNode(out,"clip",result,maReplace);
    } catch(const std::exception& e) {api->mapSetError(out,e.what());}
}
void VS_CC createCPU(const VSMap* in,VSMap* out,void*,VSCore* core,const VSAPI* api) {
    VSNode* node=nullptr;
    try {
        node=api->mapGetNode(in,"clip",0,nullptr);
        if(api->getNodeResidency(node)!=nrCPU) fail("DFTTest expects a CPU clip; use DFTTestGPU for GPU input");
        auto* standard=api->getPluginByID("com.vapoursynth.std",core);auto* self=api->getPluginByID(pluginID,core);
        for(auto [plugin,name]:std::array<std::pair<VSPlugin*,const char*>,3>{{{standard,"GPUUpload"},{self,"DFTTestGPU"},{standard,"GPUDownload"}}}) {
            VSMap* args=api->createMap();if(plugin==self) api->copyMap(in,args);
            api->mapSetNode(args,"clip",node,maReplace);VSMap* result=api->invoke(plugin,name,args);api->freeMap(args);
            const char* error=api->mapGetError(result);
            if(error) {std::string message=error;api->freeMap(result);fail(message);}
            VSNode* next=api->mapGetNode(result,"clip",0,nullptr);api->freeMap(result);api->freeNode(node);node=next;
        }
        api->mapConsumeNode(out,"clip",node,maReplace);node=nullptr;
    } catch(const std::exception& e) {if(node)api->freeNode(node);api->mapSetError(out,e.what());}
}
constexpr const char* parameters="ftype:int:opt;sigma:float:opt;sigma2:float:opt;pmin:float:opt;pmax:float:opt;sbsize:int:opt;smode:int:opt;sosize:int:opt;tbsize:int:opt;tmode:int:opt;tosize:int:opt;swin:int:opt;twin:int:opt;sbeta:float:opt;tbeta:float:opt;zmean:int:opt;f0beta:float:opt;slocation:float[]:opt;ssx:float[]:opt;ssy:float[]:opt;sst:float[]:opt;ssystem:int:opt;planes:int[]:opt;cache:int:opt;cache_frames:int:opt;cache_mb:int:opt;";
}
VS_EXTERNAL_API(void) VapourSynthPluginInit2(VSPlugin* plugin,const VSPLUGINAPI* api) {
    api->configPlugin(pluginID,"dfttestvk","DFTTest Vulkan",VS_MAKE_VERSION(0,1),VAPOURSYNTH_API_VERSION,0,plugin);
    std::string gpu="clip:vnode:gpu;"+std::string(parameters),cpu="clip:vnode;"+std::string(parameters);
    api->registerFunction("DFTTestGPU",gpu.c_str(),"clip:vnode:gpu;",createGPU,nullptr,plugin);
    api->registerFunction("DFTTest",cpu.c_str(),"clip:vnode;",createCPU,nullptr,plugin);
}
