//
// Created by yuyang.huang on 17-11-7.
//

#ifndef TFDL_DATA_H
#define TFDL_DATA_H

#include "Data.h"
#ifdef USE_GPU
#include "GPU_MATH.h"
#endif
#ifdef USE_TFACC
#include <tfblas_api.hpp>
#include <common.hpp>
#endif
#include "common.h"
#include "transforms.hpp"

#ifdef USE_TFACC40T
#include "tfacc40t.h"
#endif

using namespace std;

namespace tfdl {
    template <typename T>
    class Data {

    public:
        Data (): cpuData(NULL) {}
        Data (string dataType) : cpuData(NULL), dataType(dataType) {}
        Data (string dataType, float rangeMin, float rangeMax) :
            cpuData(NULL), dataType(dataType), rangeMin(rangeMin), rangeMax(rangeMax) {}

        ~Data() {
            ClearData();
        }

        // 获取该数据块的数据数组的首地址指针
        T *GetCpuData() const;

        // 获取该数据块的数据数组中指定位置的值
        T Get(int n, int c, int h, int w) const;

        // 设置该数据块的数据数组中指定位置的值
        void Set(int n, int c, int h, int w, T src);

        // 获取该数据块第index个维度的尺寸
        int GetDim(int index) const;
        int GetDim(int index);

        // 获取该数据块所有维度的尺寸
        vector<int> GetDims() const;

        // 获取该数据块的最小元素
        T GetMinValue() const;

        // 获取该数据块的最大元素
        T GetMaxValue() const;

        // 获取该数据块的名称
        string GetName();

        void ClearData();
        int GetRealAxis(int index);
        int GetAxisNum();
        void ResizeBatchNum(int batchNum);
        void Resize(const vector<int> &_dims);
        void Alignment(int dim, int toAlignment);
        void SetFrugalMode(bool frugalMode);
        T GetMinValue(int start, int end) const ;
        T GetMaxValue(int start, int end) const ;

        void Create(const vector<int> &_dims);

        //memory
        void Reallocate(T value);
        void Reallocate();
        void ForceReallocate();
        void ReallocateTF(int size);
        void ShareData(const Data &v);
        void ShareData(T *v);

        string GetShapeString() const;
        int GetTFInputBase(int offset);
        int GetSize();
        int GetMallocSize();
        long long Count(int st, int end);
        long long Count(int st);
        long long Count(int st, int end) const;
        long long Count(int st) const;

        void SetName(const string &name);
        void SetFixed(bool isFixed);
        void SetFixedFloatData(bool isFixedFloatData);
        void SetFixShape(bool fixShape);
        bool GetFixed();
        bool GetFixedFloatData();
        bool GetIsConv();
        void SetConv(bool isconv);
        string GetDataType() const;
        string GetDataType();
        int GetDataTypeBytes() const; // 返回1个元素是多少字节

        void GetFloatData(float * floatData) const;
        void SetFloatData(float * floatData) const;
        void GetIntData(int * intData) const;
        void SetIntData(int * intData) const;
        void GetInt16Data(uint16_t * int16Data) const;
        void SetInt16Data(uint16_t * int16Data) const;
        void GetInt8Data(uint8_t * int8Data) const;
        void SetInt8Data(uint8_t * int8Data) const;
        float * GetFloatRawData() const;
        int * GetIntRawData() const;
        uint16_t * GetInt16RawData() const;
        uint8_t * GetInt8RawData() const;

        void PrepareFloatData(float * &floatData);
        void FinishFloatData(float * &floatData);

        void ChangeDataType(string dataType);
        void ConvertToDataType(string dataType);
        void ConvertToDataType(string dataType, float rangeMin, float rangeMax);
        void GetRange(float &rangeMin, float &rangeMax) const;
        void GetRange(float &rangeMin, float &rangeMax);
        void Permute(vector<int> axis);
        void FillZeroW(T zero, int fixZero, int fixCnt);

        int UnitSize(); // 每个元素多少字节
        bool IsPacked(); // 是否是紧凑结构
        int RealLength(); // 真实尺寸(既不考虑stride的尺寸，等于每一维乘起来)
        void Alignment(vector <char> &ori); // ori.length = this->RealLength() * unitSize, 把这个数据对齐之后填充到this->cpuData中
        void Alignment(char *ori, int length); // ori.length = this->RealLength() * unitSize, 把这个数据对齐之后填充到this->cpuData中
        void Revert(vector <char> &ori); // 把this->cpuData中的数据恢复到真实尺寸之后填充进ori中
        void Revert(char *ori); // 把this->cpuData中的数据恢复到真实尺寸之后填充进ori中
#ifdef USE_TFACC
        void ShareTFData(tfblas::MmapBuf<magicType>* TFData);
        tfblas::MmapBuf<magicType>* getTFData();
        void SetBaseOffset(int offset);
#endif

#ifdef USE_GPU
        T* getGpuData(){
            return gpuData;
        }
        void clear_gpu(){
            CUDA_CHECK(cudaFree(gpuData));
            is_set_gpu=false;
        }
        void reallocate(T value){
            if(is_set_gpu)clear_gpu();
            cudaMalloc((void **)&gpuData,count(0)*sizeof(T));
            cudaMemset(gpuData,value,count(0)*sizeof(T));
            is_set_gpu=true;
        }
#endif
    protected:
        string dataName;
#ifdef USE_GPU
        T* gpuData;
        bool is_set_gpu;
        bool is_set_cpu;
#endif

        string dataType; //dataType, support "float" or "double" or "int8"
        vector <int> dims;
        vector <int> offsets;
        T *cpuData;
        bool needClear = false;
        bool fixed = false; // fixed的数据在Reshape时就会计算出来，Forward时不计算
        bool fixedFloatData = false; // 如果fixedFloatData为真，那么永远存储Float数据，不做量化
        bool frugalMode = false;
        bool isConv = false;

        bool fixShape = true;
        float rangeMin, rangeMax; //数据范围，非float时用于量化
#ifdef USE_TFACC
        int BaseOffset = 0;
        tfblas::MmapBuf<magicType>* TFData = nullptr;
#endif
#ifdef USE_TFACC40T
    public:
        tfacc40t::MEM_U8 *TFData = nullptr;
        int typeAsOutput = 0; // 作为输出时的类型 0 for NULL, 1 for TFACC, 2 for CPU
        int typeAsInput = 0; // 作为输入时的类型 0 for NULL, 1 for TFACC, 2 for CPU

        void AddInvalidCacheBlasop(tfacc40t::BlasopList *blasopList, int type);
#endif
    };

    class Randn : public Data<float> {
    public:
        Randn(const vector<int> &dims, float mean = 0.f, float var = 1.f) : Data<float>() {
            this->Resize(dims);
            this->Reallocate();
            transforms::fill_randn_f(this->cpuData, this->Count(0), mean, var);
        }
    };

    template<typename T>
    class Zeros : public Data<T> {
    public:
        Zeros(const vector<int> &dims) : Data<T>() {
            this->Resize(dims);
            this->Reallocate(0);
        }
    };

    struct PerChannelConfig {
        bool usePerChannel;
        int axis;
        vector <QuantizationConfig> configs;

        PerChannelConfig () {
            usePerChannel = false;
        }

        PerChannelConfig (bool use, int axis, const vector <QuantizationConfig> configs) {
            this->usePerChannel = use;
            this->axis = axis;
            this->configs = configs;
        }

        PerChannelConfig extract(int st, int end) const {
            PerChannelConfig res;
            res.usePerChannel = this->usePerChannel;
            res.axis = axis;
            for (int i = st; i < end; i++) {
                res.configs.push_back(this->configs[i]);
            }
            return res;
        }

        bool equal(const PerChannelConfig &b, int dimsLen ) const{
            int realAxis_a = (this->axis % dimsLen + dimsLen) % dimsLen;
            int realAxis_b = (b.axis % dimsLen + dimsLen) % dimsLen;
            if( this->usePerChannel != b.usePerChannel || realAxis_a != realAxis_b
                    || this->configs.size() != b.configs.size() )
                return false;
            for( int i = 0; i < configs.size(); ++i )
                if( !this->configs[i].equal( b.configs[i] ) )
                    return false;
            return true;
        }

        float getScale(int channel) const {
            return channel >= this->configs.size() ? this->configs[0].getScale() : this->configs[channel].getScale();
        }

        uint8_t getZeroPoint(int channel) const {
            return channel >= this->configs.size() ? this->configs[0].getZeroPoint() : this->configs[channel].getZeroPoint();
        }
    };

    class TFData {
    public:
        TFData (string dataType);
        TFData (float rangeMin, float rangeMax, string dataType);

        ~TFData();

        // 创建形状为_dims的TFData
        virtual void Create(const vector<int> &_dims) = 0;

        // 创建形状为_dims的TFData并用values初始化
        virtual void Create(const vector<int> &_dims, const vector<float> &values) = 0;

        // 仅支持dims.size() = 2的情况, 用将第row行[offset, offset + length)的数据创建一个尺寸为{length}的TFData (类型和target保持一致)
        void CopyToTFDataByRow(TFData* target, int row, int offset, int length);

        // return CountInEltments(st)
        int Count(int st = 0) const;

        // dims[st] * GetStrideInBytes(st)
        int CountInBytes(int st = 0) const;

        // dims[st] * GetStrideInElements(st)
        int CountInEltments(int st = 0) const;

        // 将W维度上真实长度以外的部分填成0, realLen是长度为batch的vector, 代表每个batch的真实长度
        void FillZeroW(const vector <int> &realLen) const;

        // 返回数据类型
        string GetDataType() const;

        // 返回1个元素是多少字节
        int GetDataTypeBytes() const;

        // 获取第index维的尺寸
        int GetDim(int index) const;

        // 获取该数据块所有维度的尺寸
        vector<int> GetDims() const;

        // 第i维的跨度(第index维前进1的时候实际地址前进多少元素)
        int GetStrideInElements(int index) const;

        // 返回跨度数组(单位为元素)
        vector<int> GetStridesInElements() const;

        // 第i维的跨度(第index维前进1的时候实际地址前进多少元素)
        int GetStrideInBytes(int index) const;

        // 返回跨度数组(单位为元素)
        vector<int> GetStridesInBytes() const;

        // 获得形状字符串
        string GetShapeString() const;

        // 导出float数据
        virtual void GetFloatData(float * floatData) const = 0;

        // 导入float数据
        virtual void SetFloatData(float * floatData) const = 0;

        // 获取取值范围
        void GetRange(float &rangeMin, float &rangeMax) const;

        // 获取取值范围最小值
        float GetRangeMin() const;

        // 获取取值范围最小值
        float GetRangeMax() const;

        // 获取分通道量化参数
        const PerChannelConfig & GetPerChannelConfig() const;

        PerChannelConfig GetPerChannelConfig(int st, int end);

        // 设置分通道量化参数
        void SetPerChannelConfig(const PerChannelConfig &config);

        // 严格变换形状(保持数据不变)
        void Reshape(const vector <int> &_dims);

        // 变换形状
        void Resize(const vector<int> &_dims);

        // 重排列通道
        void Permute(const vector<int> &orders);

        // 获取isWeight的值
        bool GetIsWeight();

        // 设置isWeight的值
        void SetIsWeight(bool isWeight);

        void PrepareFloatData(float * &floatData, bool readData);
        void FinishFloatData(float * &floatData, bool writeBack);
        void *GetRawDataPtr() const;

#ifdef USE_TFACC
        int tfaccHardwareId = -1;
        tfblas::MmapBuf <magicType> * tfaccSpace = nullptr;

        int BaseOffset = 0;
        string registerType;
        int lutId = -1;
        tfblas::MmapBuf<magicType>* TFDataAddr = nullptr;
        tfblas::MmapBuf<magicType>* TFDataAddr2 = nullptr;

        int tfaccCount = 0;
        // 拆分多段计算时使用
        vector <int> lutIds;
#endif
    protected:
        void ClearData();

        bool isWeight = false; // isWeight设置为true时,计算时中会被缓存在tfacc中,但数据的修改不会实时生效

        string dataType; //dataType, support "float" or "intX" or "tfX"
        int dataBytes;
        vector <int> dims;
        vector <int> stridesInBytes, stridesInEltments;
        int offsetInBytes = 0, offsetInElements = 0;
        char *cpuData = nullptr;
        float rangeMin, rangeMax; //数据范围，非float时用于量化
        PerChannelConfig perChannelConfig; //分通道量化信息

        int calcTag = 0;

        bool fake = false; // 虚拟TFData,仅用于承载老Data数据进行计算
    };

    class TFDataInt8;
    class TFDataInt16;
    class TFDataFloat;
    class TFDataFloat16;

    class TFDataInt8 : public TFData {
    public:
        // 创建一个fake的TFDataInt8, 承载老TFData里的数据
        TFDataInt8(float rangeMin, float rangeMax, Data<magicType> *tfdata);

        // 初始化量化范围为[rangeMin, rangeMax]的TFDataInt8
        TFDataInt8(float rangeMin, float rangeMax);

        // 初始化量化范围为[rangeMin, rangeMax], 形状为_dims
        TFDataInt8(float rangeMin, float rangeMax, const vector<int> &_dims);

        // 初始化量化范围为[rangeMin, rangeMax], 形状为_dims, 值为values的TFDataInt8
        TFDataInt8(float rangeMin, float rangeMax, const vector<int> &_dims, const vector<float> &values);

        TFDataInt8(float rangeMin, float rangeMax, const vector<int> &_dims, uint8_t *data);

        // 用origin的尺寸, 数据, 量化参数[rangeMin, rangeMax]创建TFDataInt8
        TFDataInt8(TFData *origin, float rangeMin, float rangeMax);

        // 用origin的范围, 尺寸, 数据创建TFDataInt8
        TFDataInt8(TFDataInt8 *origin);

        // 用origin的范围, 尺寸, 数据创建TFDataInt8
        TFDataInt8(TFDataInt16 *origin);

        // 用origin的范围, 尺寸, 数据创建TFDataInt8
        TFDataInt8(TFDataFloat *origin);

        TFDataInt8(TFDataFloat16 *origin);

        // 用origin的范围, 尺寸, 数据创建TFDataInt8, 并以perchannelAxis维做多通道量化
        TFDataInt8(TFDataFloat *origin, int perChannelAxis);

        TFDataInt8(TFDataFloat16 *origin, int perChannelAxis);

        // 用origin的范围, 尺寸, 数据创建TFDataInt8
        TFDataInt8(TFData *origin);

        // 析构函数
        ~TFDataInt8();

        // 创建形状为_dims的TFDataInt8
        void Create(const vector<int> &_dims);

        // 创建形状为_dims的TFData并用values初始化
        void Create(const vector<int> &_dims, const vector<float> &values);

        // 获取量化范围
        void GetRange(float &rangeMin, float &rangeMax) const;

        // 导出int8数据
        void GetInt8Data(uint8_t * int8Data) const;

        // 导入int8数据
        void SetInt8Data(uint8_t * int8Data) const;

        // 导出float数据
        void GetFloatData(float * floatData) const;

        // 导入float数据
        void SetFloatData(float * floatData) const;

        // 把数据当成[n, m]旋转成[m, n]
        void Trans2D(int n, int m);

        // 获取int8实际地址
        uint8_t * GetInt8RawData() const;

        // 注册为InnerProduct的Weight
        void RegisterForInnerProductWeight(int n, int m, bool useTwoCore);

        // 注册为Convolution的Weight
        void RegisterForConvolution();

        // 取消所有注册(包括InnerProduct和Convolution)
        void Unregister();

        // 将数据存储到TFACC空间, hardwareId代表硬件Id(可取0或1), copyData代表是否要拷贝数据(如果为false那么仅做空间分配)
        void ToTFACC(int hardwareId, bool copyData);

        // 将数据存入非TFACC空间, copyData代表是否要拷贝数据(如果为false那么仅做空间分配)
        void ToCPU(bool copyData);
    };

    class TFDataInt16 : public TFData {
    public:
        // 初始化量化范围为[rangeMin, rangeMax]的TFDataInt16
        TFDataInt16(float rangeMin, float rangeMax);

        // 初始化量化范围为[rangeMin, rangeMax], 形状为_dims
        TFDataInt16(float rangeMin, float rangeMax, const vector<int> &_dims);

        // 初始化量化范围为[rangeMin, rangeMax], 形状为_dims, 值为values的TFDataInt16
        TFDataInt16(float rangeMin, float rangeMax, const vector<int> &_dims, const vector<float> &values);

        // 用origin的尺寸, 数据, 量化参数[rangeMin, rangeMax]创建TFDataInt16
        TFDataInt16(TFData *origin, float rangeMin, float rangeMax);

        // 用origin的范围, 尺寸, 数据创建TFDataInt16
        TFDataInt16(TFDataInt8 *origin);

        // 用origin的范围, 尺寸, 数据创建TFDataInt16
        TFDataInt16(TFDataInt16 *origin);

        // 用origin的范围, 尺寸, 数据创建TFDataInt16
        TFDataInt16(TFDataFloat *origin);

        // 用origin的范围, 尺寸, 数据创建TFDataInt16
        TFDataInt16(TFData *origin);

        // 析构函数
        ~TFDataInt16();

        // 创建形状为_dims的TFDataInt16
        void Create(const vector<int> &_dims);

        // 创建形状为_dims的TFData并用values初始化
        void Create(const vector<int> &_dims, const vector<float> &values);

        // 获取量化范围
        void GetRange(float &rangeMin, float &rangeMax) const;

        // 导出int16数据
        void GetInt16Data(uint16_t * int16Data) const;

        // 导入int16数据
        void SetInt16Data(uint16_t * int16Data) const;

        // 导出float数据
        void GetFloatData(float * floatData) const;

        // 导入float数据
        void SetFloatData(float * floatData) const;

        // 获取int16实际地址
        uint16_t * GetInt16RawData() const;
    };

    class TFDataFloat : public TFData {
    public:
        // 创建一个fake的TFDataFloat, 承载老TFData里的数据
        TFDataFloat(Data <float> *tfdata);

        // 初始化TFDataFloat
        TFDataFloat();

        // 初始化形状为_dims
        TFDataFloat(const vector<int> &_dims);

        // 初始化形状为_dims, 值为values的TFDataFloat
        TFDataFloat(const vector<int> &_dims, const vector<float> &values);

        TFDataFloat(const vector<int> &_dims, float *data);

        // 用origin的尺寸, 数据, 创建TFDataFloat
        TFDataFloat(TFData *origin);

        // 析构函数
        ~TFDataFloat();

        // 创建形状为_dims的TFDataIntFloat
        void Create(const vector<int> &_dims);

        // 创建形状为_dims的TFData并用values初始化
        void Create(const vector<int> &_dims, const vector<float> &values);

        // 获取该数据块的最小元素
        float GetMinValue() const;

        // 获取该数据块的最大元素
        float GetMaxValue() const;

        // 获取该数据块下标范围[start, end)之间的最小元素
        float GetMinValue(int start, int end) const;

        // 获取该数据块下标范围[start, end)之间的最大元素
        float GetMaxValue(int start, int end) const;

        // 导出float数据
        void GetFloatData(float * floatData) const;

        // 导入float数据
        void SetFloatData(float * floatData) const;

        // 获取float实际地址
        float * GetFloatRawData() const;
    };

    class TFDataFloat16 : public TFData {
    public:
        TFDataFloat16();

        TFDataFloat16(const vector<int> &_dims);

        TFDataFloat16(const vector<int> &_dims, const vector<uint16_t> &values);

        TFDataFloat16(const vector<int> &_dims, uint16_t *data);

        ~TFDataFloat16();

        void Create(const vector<int> &_dims);

        void Create(const vector<int> &_dims, const vector<float> &values);

        void Create(const vector<int> &_dims, const vector<uint16_t> &values);

        float GetMinValue() const;

        float GetMaxValue() const;

        float GetMinValue(int start, int end) const;

        float GetMaxValue(int start, int end) const;

        void GetFloatData(float * floatData) const;

        void SetFloatData(float * floatData) const;

        uint16_t *GetFloat16RawData() const;
    };
}


#endif //CAFFE_DATA_H
