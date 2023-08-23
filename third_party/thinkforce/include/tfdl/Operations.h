//
// Created by huangyuyang on 2/9/20.
//

#ifndef PROJECT_OPERATIONS_H
#define PROJECT_OPERATIONS_H

#include "common.h"
#include "Data.h"
#include "INT8Config.h"
#ifdef USE_TFACC40T
#include "tfacc40t.h"
#include "tfnn.h"

#include <mutex>
#endif

namespace tfacc10t {
    void Convolution(tfdl::TFDataInt8 *input, tfdl::TFDataInt8 *output, tfdl::TFDataInt8 *weight, tfdl::TFDataInt8 *bias,
                     int outputChannels, int kernel, int stride, bool relu);

    void Convolution(tfdl::TFDataInt8 *input, tfdl::TFDataInt8 *output, tfdl::TFDataInt8 *weight, tfdl::TFDataInt8 *bias,
                     int outputChannels, int kernel, int stride, uint8_t* mapTable);

    void Convolution(tfdl::TFDataInt8 *input, tfdl::TFDataInt8 *output, tfdl::TFDataInt8 *weight, tfdl::TFDataInt8 *bias,
                     int outputChannels, int group, int kernelH, int kernelW, int strideH, int strideW,
                     int padH, int padW, int dilationH, int dilationW, bool relu=false);
    void Convolution(tfdl::TFDataInt8 *input, tfdl::TFDataInt8 *output, tfdl::TFDataInt8 *weight, tfdl::TFDataInt8 *bias,
                     int outputChannels, int group, int kernelH, int kernelW, int strideH, int strideW,
                     int padHBegin, int padHEnd, int padWBegin, int padWEnd, int dilationH, int dilationW, bool relu);

    void InnerProduct(tfdl::TFDataInt8 *input, tfdl::TFDataInt8 *output, tfdl::TFDataInt8 *weight, tfdl::TFDataInt8 *bias, int outputChannels);

    // input0: [n, m]的矩阵
    // input1: [m, k]的矩阵
    // output: [n, k]的矩阵
    // (以上尺寸均为转置后的尺寸)
    // output = alpha * input0 * input1 + bias
    // transpose0代表input0是否需要转置
    // transpose1代表input1是否需要转置
    // bias = NULL时, 代表忽略bias以及biasForPerRow参数
    // biasForPerRow = true时, bias是长度为n的向量, 代表output每行加上一个数
    // biasForPerRow = false时, bias是长度为k的向量, 代表output每列加上一个数
    // relu代表是否对output做relu
    // mapTable代表计算结束后对output做一次映射,对于output中的每一个点取output[i] = mapTable[output[i]]
    // mapTable为NULL时忽略该参数
    // hardwareId代表使用哪个簇进行计算
    void Multiply(tfdl::TFDataInt8 *input0, tfdl::TFDataInt8 *input1, tfdl::TFDataInt8 *output,
                  float alpha, bool transpose0, bool transpose1,
                  tfdl::TFData *bias = nullptr, bool biasForPerRow = true, bool relu = false, const uint8_t* mapTable = nullptr, int harewareId = 0);

    void Multiply(tfdl::TFDataInt8 *input0, tfdl::TFDataInt8 *input1, tfdl::TFDataInt8 *output, int n, int m, int k, float alpha = 1.0, tfdl::TFData *bias = nullptr);

    void MultiplyBatch(tfdl::TFDataInt8 *input0, tfdl::TFDataInt8 *input1, tfdl::TFDataInt8 *output, float alpha, bool transpose0, bool transpose1, tfdl::TFData *bias = nullptr);

    void Pooling(tfdl::TFDataInt8 *input, tfdl::TFDataInt8 *output, int kernel, int stride, int pad, string type);
}

namespace tfacc40t {
#define MAXCHIPNUM 4
#ifdef  USE_TFACC40T
    static std::mutex thread_lock;

    static map<long long int, tfacc40t::MEM_U8 *> tfacc40t_weight_map[MAXCHIPNUM];
    static map<long long int, tfacc40t::MEM_U8 *> tfacc40t_bias_map[MAXCHIPNUM];
    static map<long long int, vector<vector<float>>> tfacc40t_weight_sum[MAXCHIPNUM];
    static map<long long int, tfdl::PerChannelConfig> tfacc40t_weight_config[MAXCHIPNUM];

    static vector<pair<bool, tfacc40t::MEM_U8 *>> tfacc40t_data_pool[8 * MAXCHIPNUM];

    template<typename T>
    tfacc40t::Memory<T> *MallocDeviceMemory(int requiredSize, int deviceId);

    template<typename T>
    tfacc40t::Memory<T> *MallocDeviceMemory(tfdl::TFData *host, int deviceId);

    template<typename T>
    void ReleaseDeviceMemory(tfacc40t::Memory<T> *device, int deviceId);

    void ReleaseAllDeviceMemory();

    void ReleaseAllDeviceWeight();

    template<typename T>
    void HostToDevice(tfdl::TFData *host, tfacc40t::Memory<T> *device);

    template<typename T>
    void DeviceToHost(tfdl::TFData *host, tfacc40t::Memory<T> *device);

    template<typename T>
    void InvalidCache(tfacc40t::BlasopList *blasopList, tfacc40t::Memory<T> *mem, int type);
#endif

    void Convolution(tfdl::TFDataInt8 *input, tfdl::TFDataInt8 *output, tfdl::TFDataInt8 *weight, tfdl::TFDataInt8 *bias,
                     int deviceId, int outputChannels, int kernel, int stride, bool relu);

    void Convolution(tfdl::TFDataInt8 *input, tfdl::TFDataInt8 *output, tfdl::TFDataInt8 *weight, tfdl::TFDataInt8 *bias,
                     int deviceId, int outputChannels, int group, int kernelH, int kernelW, int strideH, int strideW,
                     int padHBegin, int padHEnd, int padWBegin, int padWEnd, int dilationH, int dilationW, bool relu);

    void Convolution(tfdl::TFDataInt8 *input, tfdl::TFDataInt8 *output, tfdl::TFDataInt8 *weight, tfdl::TFDataInt8 *bias,
                     int deviceId, int outputChannels, int group, int kernelH, int kernelW, int strideH, int strideW,
                     int padH, int padW, int dilationH, int dilationW, bool relu);

    void Convolution(tfdl::TFDataFloat *input, tfdl::TFDataFloat *output, tfdl::TFDataFloat *weight, tfdl::TFDataFloat *bias,
                     int deviceId, int outputChannels, int group, int kernelH, int kernelW, int strideH, int strideW,
                     int padHBegin, int padHEnd, int padWBegin, int padWEnd, int dilationH, int dilationW, bool relu);
    
    void InnerProduct(tfdl::TFDataInt8 *input, tfdl::TFDataInt8 *output, tfdl::TFDataInt8 *weight, tfdl::TFDataInt8 *bias,
                      int deviceId);

    void InnerProduct(tfdl::TFDataInt8 *input, tfdl::TFDataFloat *output, tfdl::TFDataInt8 *weight, tfdl::TFDataFloat *bias,
                      int deviceId, void *blasopCache = nullptr, long long int weight_key = 0);

    void InnerProduct(tfdl::TFDataInt8 *input, tfdl::TFDataFloat *output, tfdl::TFDataFloat *weight, tfdl::TFDataFloat *bias,
                      int deviceId, void *blasopCache = nullptr, long long int weight_key = 0);

    void InnerProduct(tfdl::TFDataFloat *input, tfdl::TFDataFloat *output, tfdl::TFDataFloat *weight, tfdl::TFDataFloat *bias,
                      int deviceId, void *blasopCache = nullptr, long long int weight_key = 0);

    void InnerProduct(tfdl::TFDataFloat *input, tfdl::TFDataFloat *output, tfdl::TFDataInt8 *weight, tfdl::TFDataFloat *bias,
                      int deviceId, void *blasopCache = nullptr, long long int weight_key = 0);

#ifdef __ARM_FEATURE_FP16_VECTOR_ARITHMETIC
    void InnerProduct(tfdl::TFDataInt8 *input, tfdl::TFDataFloat16 *output, tfdl::TFDataInt8 *weight, tfdl::TFDataFloat *bias,
                      int deviceId, void *blasopCache = nullptr, long long int weight_key = 0);

    void InnerProduct(tfdl::TFDataInt8 *input, tfdl::TFDataFloat16 *output, tfdl::TFDataFloat16 *weight, tfdl::TFDataFloat *bias,
                      int deviceId, void *blasopCache = nullptr, long long int weight_key = 0);

    void InnerProduct(tfdl::TFDataFloat16 *input, tfdl::TFDataFloat16 *output, tfdl::TFDataFloat16 *weight, tfdl::TFDataFloat *bias,
                      int deviceId, void *blasopCache = nullptr, long long int weight_key = 0);

    void InnerProduct(tfdl::TFDataFloat *input, tfdl::TFDataFloat16 *output, tfdl::TFDataInt8 *weight, tfdl::TFDataFloat *bias,
                      int deviceId, void *blasopCache = nullptr, long long int weight_key = 0);
#endif

    void Pooling(tfdl::TFDataInt8 *input, tfdl::TFDataInt8 *output, int deviceId, int kernel, int stride, int pad, string type);

    void BatchMatrixMul(tfdl::TFDataInt8 *x, tfdl::TFDataInt8 *y, tfdl::TFDataInt8 *output, int device_id);

    void BatchMatrixMul(tfdl::TFDataFloat *x, tfdl::TFDataFloat *y, tfdl::TFDataFloat *output, int device_id,
                        void *blasopCache = nullptr);

    typedef float (*functionType)(float);
    void MapTable(tfdl::TFDataFloat *input, tfdl::TFDataFloat *output, functionType function, int deviceId);

    void DotProd(float *input, float *weight, float *output, int channel, int len, int weightStride, int deviceId);
}

namespace tfdl {
    // 分析工具控制
    // 打开分析工具
    void OpenProfiler();

    // 关闭分析工具
    void CloseProfiler();

    // 设置是否输出到屏幕
    void SetWriteProfilerToScreen(bool writeToScreen);

    // 清空分析工具缓冲区
    void ClearProfilerCache();

    // 提取缓冲区数据
    string GetProfilerCache();

    // 提取并清空缓冲区
    string GetAndClearProfilerCache();

    // 清空分析工具表格
    void ClearProfilerTable();

    // 提取表格数据
    string GetProfilerTable();

    // 提取并清空表格
    string GetAndClearProfilerTable();

    class Model {
    public:
        ~Model();

        void Load(string fileName);
        void Load(char *buffer, int len);

        void LoadInt8Model(string fileName);
        void LoadInt8Model(char *buffer, int len);

        void LoadTFLiteModel(string fileName);
        void LoadTFLiteModel(char *buffer, int len);

        void SaveTFLiteModel(string fileName);

        vector <TFData*>& operator [] (const string &layerName);
        TFData* GetWeightData(string layerName, int index);
        void RemoveWeight(string layerName);

        vector <string> GetWeightNames();
    private:
        map <string, vector <TFData*> > weights;
    };

    void BiasAddInt8Map(TFDataInt8 *input0, TFDataInt8 *bias, TFDataInt8 *output);

    void BroadcastAdd(TFData *input0, TFData *input1, TFData *output, int threadNum = 1);
    void BroadcastMul(TFData *input0, TFData *input1, TFData *output);

    void Concat(TFData *input0, TFData *input1, TFData *output);

    void Convolution(TFData *input, TFData *output, TFData *weight, TFData *bias,
                     int outputChannels, int group, int kernelH, int kernelW, int strideH, int strideW,
                     int padH, int padW, int dilationH, int dilationW);
    void Convolution(TFData *input, TFData *output, TFData *weight, TFData *bias,
                     int outputChannels, int group, int kernelH, int kernelW, int strideH, int strideW,
                     int padHBegin, int padHEnd, int padWBegin, int padWEnd, int dilationH, int dilationW);
    void Convolution(TFData *input, TFData *output, TFData *weight, TFData *bias, int outputChannels, int kernel, int stride);

    void CTC(TFDataInt8 *result, TFDataFloat *bias, int N, int T, int tokenSize, const vector <int> &realLen, vector<int> &shape, vector <int> &indice, vector <int> &value);

    void DisableInt8MapAcceleration();

    void EltwiseAdd(TFData *input0, TFData *input1, TFData *output);
    void EltwiseAddInt8Map(TFDataInt8 *input0, TFDataInt8 *input1, TFDataInt8 *output);
    void EltwiseMul(TFData *input0, TFData *input1, TFData *output);

    void EnableInt8MapAcceleration();

    void Gelu(TFData *input, TFData *output);

    void InnerProduct(TFData *input, TFData *output, TFData *weight, TFData *bias, int outputChannels);

    bool Int8MapAccelerationIsEnabled();

    void LayerNorm(TFData *input, TFData *output, TFData *gamma, TFData *beta, int axis);
    void LayerNorm(TFData *input, TFData *output, const vector <float> &gamma, const vector <float> &beta, int axis);
    void LayerNorm(TFData *input, TFData *output, const float *gamma, const float *beta, int axis);

    // input0: [n, m]的矩阵
    // input1: [m, k]的矩阵
    // output: [n, k]的矩阵
    // (以上尺寸均为转置后的尺寸)
    // output = alpha * input0 * input1 + bias
    // transpose0代表input0是否需要转置
    // transpose1代表input1是否需要转置
    // bias = NULL时, 代表忽略bias以及biasForPerRow参数
    // biasForPerRow = true时, bias是长度为n的向量, 代表output每行加上一个数
    // biasForPerRow = false时, bias是长度为k的向量, 代表output每列加上一个数
    // relu代表是否对output做relu
    // mapTable代表计算结束后对output做一次映射,对于output中的每一个点取output[i] = mapTable[output[i]]
    // mapTable为NULL时忽略该参数
    void Multiply(tfdl::TFDataInt8 *input0, tfdl::TFDataInt8 *input1, tfdl::TFDataInt8 *output,
                  float alpha, bool transpose0, bool transpose1,
                  tfdl::TFData *bias, bool biasForPerRow, bool relu = false, const uint8_t* mapTable = nullptr, int harewareId = 0);
    // input0 [n, m], input1 [k, m], output[n, k]
    // 行与行相乘　(output = input0 * input1')
    void Multiply(TFData *input0, TFData *input1, TFData *output, int n, int m, int k, float alpha = 1.0);
    // 除了最后两维, 其余都当成batch
    // input0 [batch, n, m], input1 [(batch or 1), k, m], output [batch, n, k]
    // 对于每个batch, (output = input0 * input1')
    void MultiplyBatch(TFData *input0, TFData *input1, TFData *output);

    // input0 [n, m], input1 [m, k], output[n, k]
    // output = input0 * input1 * alpha
    // n, m, k均为转置后的参数
    // transpose0: input0是否转置
    // transpose1: input1是否转置
    void Multiply(TFData *input0, TFData *input1, TFData *output, float alpha, bool transpose0, bool transpose1, int n, int m, int k);

    // 除了最后两维, 其余都当成batch
    // input0 [batch, n, m], input1 [(batch or 1), m, k], output [batch, n, k]
    // 对于每个batch, (output = input0 * input1)
    void MultiplyBatch(TFData *input0, TFData *input1, TFData *output, float alpha, bool transpose0, bool transpose1, TFData *bias = nullptr);

    void Permute(TFData *input, TFData *output, const vector <int> &orders);

    void Pooling(TFData *input, TFData *output, int kernelH, int kernelW, int strideH, int strideW, int padH, int padW, string type);
    void Pooling(TFData *input, TFData *output, int kernel, int stride, int pad, string type);

    void ReduceSum(TFData *input, TFData *output);

    void Relu(TFData *input, TFData *output);

    void Scale(TFData *input, TFData *output, float scale);

    void Sigmoid(TFData *input, TFData *output);

    void Softmax(TFData *input, TFData *output, int axis);
    void SoftmaxWithMask(TFData *input, TFData *output, TFData *mask, int axis);
    void SoftmaxWithMask(TFData *input, TFData *output, const vector <float> &mask, int axis);
    void SoftmaxWithMask(TFData *input, TFData *output, const float *mask, int axis);
    // 这种模式下, mask[i] = 0 or 255, 0代表弃用某一位
    void SoftmaxWithMask(TFDataInt8 *input, TFDataInt8 *output, const uint8_t *mask, int axis);
    // SoftMax + topK
    void SoftMaxTopK(TFDataInt8 *input, int M, int N, int K, vector <int> &index, vector <float> &score, int threadNum = 1);

    void Tanh(TFData *input, TFData *output);
    
    // Ref: https://github.com/onnx/onnx/blob/master/docs/Operators.md#Softplus
    void Softplus(TFData *input, TFData *output);

    class LSTM {
        int inputSize, outputSize;
        bool reverse;

        TFDataInt8 * Wx;
        TFDataInt8 * X;
        TFDataInt8 * Wicfo;
        TFDataFloat * bicfo;
        TFDataInt8 *ICFO, * I, * C, * F, * O;
        TFDataFloat * it, * ct, * ft, * ot;
        TFDataFloat * Wf_peephole, * Wi_peephole, * Wo_peephole;
        TFDataInt8 * Xt;
        TFDataFloat * forgetBias;
        TFDataInt8 *oppo_Wx, *oppo_Wicfo;
        TFDataFloat *Wx_fake_bias, *Wicfo_fake_bias;

        TFDataFloat * temp;

        float rangeMinX, rangeMaxX;
        float rangeMinICFO, rangeMaxICFO;
        float fbias;
        bool useRelu;

        int threadNums = 4;
        void * sourceLSTM;

    public:
        LSTM (const LSTM &a);

        LSTM (int inputSize, int outputSize, bool reverse,
             TFDataInt8 *Wx, TFDataInt8 *Wicfo, TFDataFloat *bicfo,
              TFDataFloat *Wf_peephole, TFDataFloat *Wi_peephole, TFDataFloat *Wo_peephole,
             float rangeMinX, float rangeMaxX, float rangeMinICFO, float rangeMaxICFO,
             float forgetBias, bool useTwoTU = true, bool useRelu = true);

        ~LSTM();

        void Forward(TFDataInt8 *x, TFDataInt8 *output, TFDataInt8 *h, TFDataFloat *c, int N, int T, const vector <int> &realLen = vector<int>());
        void LasLstmForward(TFDataInt8 *x, TFDataInt8 *output, TFDataInt8 *h, TFDataFloat *c, int N, int T,
                            const vector<int> &realLen, vector<float> quant_infos);

        void SetThreadNums(int threadNums);
    };

    class BiasAddLayer{
    private:
        uint8_t* bias_add_map_;
        const int bias_len_;
        const float input_min_, input_max_;
        const float output_min_, output_max_;
        PerChannelConfig input_perchannel_config_;
        PerChannelConfig output_perchannel_config_;
        BiasAddLayer(TFData *bias,
                     float input_min, float input_max,
                     float output_min, float output_max,
                     const PerChannelConfig &input_perchannel_config,
                     const PerChannelConfig &output_perchannel_config);
    public:
        BiasAddLayer(const BiasAddLayer &a);
        BiasAddLayer(TFData *bias, TFDataInt8 *input, TFDataInt8 *output);
        BiasAddLayer(TFData *bias,
                     float input_min, float input_max,
                     float output_min, float output_max );
        BiasAddLayer(TFData *bias,
                     float input_min, float input_max,
                     const PerChannelConfig &output_perchannel_config );
        BiasAddLayer(TFData *bias,
                     const PerChannelConfig &input_perchannel_config,
                     float output_min, float output_max);
        BiasAddLayer(TFData *bias,
                     const PerChannelConfig &input_perchannel_config,
                     const PerChannelConfig &output_perchannel_config);
        ~BiasAddLayer();
        void Forward(TFDataInt8 *input0, TFDataInt8 *output);
    };

    class BertTransFormer {
        int headNum, sizePerHead;

        TFDataInt8 *WQ_k, *WK_k, *WV_k, *WAO_k, *WIM_k, *WO_k;
        TFDataFloat *BQ_k, *BK_k, *BV_k, *BAO_k, *BIM_k, *BO_k;
        TFDataFloat *softmaxMask;
        TFDataFloat *attrLayerNormGamma, *attrLayerNormBeta;
        TFDataFloat *outputLayerNormGamma, *outputLayerNormBeta;

        int threadNums = 4;
        void * sourceBertTransFormer;
    public:
        BertTransFormer (const BertTransFormer &a);

        BertTransFormer(int headNum, int sizePerHead,
                        TFDataInt8 *WQ_k, TFDataFloat *BQ_k, TFDataInt8 *WK_k, TFDataFloat *BK_k, TFDataInt8 *WV_k, TFDataFloat *BV_k,
                        TFDataFloat *softmaxMask, TFDataInt8 *WAO_k, TFDataFloat *BAO_k,
                        TFDataFloat *attrLayerNormGamma, TFDataFloat *attrLayerNormBeta,
                        TFDataInt8 *WIM_k, TFDataFloat *BIM_k, TFDataInt8 *WO_k, TFDataFloat *BO_k,
                        TFDataFloat *outputLayerNormGamma, TFDataFloat *outputLayerNormBeta);

        ~BertTransFormer();

        void Forward(TFDataInt8 *input, TFDataInt8 *output, int N, int T);

        void SetThreadNums(int threadNums);
    };
}


#endif //PROJECT_OPERATIONS_H
