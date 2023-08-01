//
// Created by huangyuyang on 11/24/20.
//

#ifndef PROJECT_TFDLBUILDER_H_H
#define PROJECT_TFDLBUILDER_H_H

#include "Operations.h"
#include "Net.h"

namespace tfdl {
    class TFDLLayerBuilder {
    public:
        TFDLLayerBuilder(string layerName, string layerType);
        TFDLLayerBuilder(string layerName, string layerType,
                         vector <TFData *> inputs, vector <TFData *> outputs, vector <TFData *> weights,
                         json11::Json params);

        ~TFDLLayerBuilder();

        void AddInput(TFData *data);
        void AddOutput(TFData *data);
        void AddWeight(TFData *data);

        string GetName() const;
        string GetType() const;
        void SetGivenName(const string &name);
        vector<TFData *>& GetInputs();
        vector<TFData *>& GetOutputs();
        vector<TFData *>& GetWeights();
        json11::Json& GetParams();

    private:
        string layerName;
        string givenName; // 如果创建的时候没有设定layerName，那么分配一个Name

        string layerType;
        vector<TFData *> inputs;
        vector<TFData *> outputs;
        vector<TFData *> weights;
        json11::Json params;
    };

    class TFDLNetBuilder {

    public:
        TFDLNetBuilder();
        ~TFDLNetBuilder();

        Net <float> *ExportFloatNet();
        HybridNet *ExportHybridNet();

        void SaveJson(string fileName);
        void SaveFloatModel(string fileName);
        void SaveInt8Model(string fileName);

        void AddInput(TFData *data);
        void AddOutput(TFData *data);
        void AddLayer(string layerType, vector <TFData*> inputs, vector <TFData*> outputs,
                      vector <TFData*> weights = vector <TFData*> {},
                      json11::Json params = json11::Json(), string layerName = "");
        void AddLayer(TFDLLayerBuilder *layer);

        void AddBatchMatrixMul(TFData *input0, TFData *input1, TFData *output,
                               float alpha, bool transpose0, bool transpose1, string layerName = "");

        // Add BiLSTM Layer
        /*
         * input: 输入, 形状为{N, T, inputSize}
         * lens: 输入的真实长度，形状为{N}
         * output: 输出, 形状为{N, T, outputSize * 2}
         * inputSize: 输入的单位尺寸
         * outputSize: 输出的单位尺寸
         * useXRange: 对输入做全连接之后，得到的数据是否使用给定量化区间
            如果useXRange = true, 那么两个量化区间分别为[forwardXMin, forwardXMax], [backwardXMin, backwardXMax]
            如果useXRange = false, 那么使用输入数据的量化区间
         * useICFORange: ICFO数据是否使用给定量化区间
            如果useICFORange = true, 那么两个量化区间分别为[forwardICFOMin, forwardICFOMax], [backwardICFOMin, backwardICFOMax]
            如果useICFORange = false, 那么使用X（全连接后得到的数据)的量化区间
         * forwardCast: 前向LSTM的全连接权重
         * forwardWicfo: 前向LSTM的ICFO权重
         * forwardBicfo: 前向LSTM的ICFO偏置
         * forwardWf: 前向LSTM中peephole的wf权重
         * forwardWi: 前向LSTM中peephole的wi权重
         * forwardWo: 前向LSTM中peephole的wo权重
         * backwardCast: 后向LSTM的全连接权重
         * backwardWicfo: 后向LSTM的ICFO权重
         * backwardBicfo: 后向LSTM的ICFO偏置
         * backwardWf: 后向LSTM中peephole的wf权重
         * backwardWi: 后向LSTM中peephole的wi权重
         * backwardWo: 后向LSTM中peephole的wo权重
         */
        void AddBiLSTM(TFData *input, TFDataFloat *lens, TFData *output, int inputSize, int outputSize, float forgetBias,
                       bool useXRange, float forwardXMin, float forwardXMax, float backwardXMin, float backwardXMax,
                       bool useICFORange, float forwardICFOMin, float forwardICFOMax,
                       float backwardICFOMin, float backwardICFOMax,
                       TFDataInt8 *forwardCast, TFDataInt8 *forwardWicfo, TFDataFloat *forwardBicfo,
                       TFDataFloat *forwardWf, TFDataFloat *forwardWi, TFDataFloat *forwardWo,
                       TFDataInt8 *backwardCast, TFDataInt8 *backwardWicfo, TFDataFloat *backwardBicfo,
                       TFDataFloat *backwardWf, TFDataFloat *backwardWi, TFDataFloat *backwardWo,
                       int tfaccNum = 1, int threadNum = 1, string layerName = "");

        // Add Convolution Layer
        void AddConvolution(TFData *input, TFData *output,
                            TFData *weight, TFData *bias,
                            int outputChannels,
                            int kernel, int stride, int pad, int group = 1,
                            string layerName = "");

        void AddEltwise(TFData *input0, TFData *input1, TFData *output, string type, string layerName = "");

        void AddGelu(TFData *input, TFData *output, string layerName = "");

        void AddInnerProduct(TFData *input, TFData *output,
                             TFData *weight, TFData *bias,
                             int outputChannels, int axis = 1, bool relu = false, string layerName = "");

        void AddLayerNorm(TFData *input, TFData *output, TFData *gamma, TFData *beta, int axis, string layerName = "");

        void AddPermute(TFData *input, TFData *output, const vector <int> &orders, string layerName = "");

        void AddRelu(TFData *input, TFData *output, string layerName = "");

        void AddReshape(TFData *input, TFData *output, const vector <int> &shapes, string layerName = "");

        void AddSoftMaxTopK(TFData *input, TFData *output, int top = 40, int axis = 1, string layerName = "");

        void AddSoftMaxWithMask(TFData *input0, TFData *input1, TFData *output, int axis = 1, string layerName = "");
    private:
        set<string> layerNameSet;
        map<TFData *, int> dataIdMap;

        vector<TFData *> inputs;
        vector<TFData *> outputs;
        vector<TFDLLayerBuilder *> layers;

        void ResetLayerId();
        string GetLayerId();
        void ResetDataId();
        string GetDataId(TFData *data);
        json11::Json ToJson(bool hybridMode);
        long long CalcFloatWeightLength();
        void FillFloatWeight(char *buffer);
    };
}

#endif //PROJECT_TFDLBUILDER_H_H
