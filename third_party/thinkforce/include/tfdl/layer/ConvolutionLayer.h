//
// Created by yuyang.huang on 17-11-7.
//

#ifndef TFDL_CONVOLUTIONLAYER_H
#define TFDL_CONVOLUTIONLAYER_H

#include "../Layer.h"
#include "../layer/SliceLayer.h"

#ifdef USE_TFACC
#include <tfblas_api.hpp>
#include <common.hpp>
#endif

#ifdef USE_TFCore
#include <tfblas_api.hpp>
#endif

#include <layer/BatchNormLayer.h>
#include <layer/ScaleLayer.h>
#include <layer/ReluLayer.h>
#include <layer/ReluXLayer.h>
#include <float.h>

#ifdef USE_TFACC40T
#include "tfacc40t.h"
#endif

namespace tfdl {
    template<typename T>
    class ConvolutionLayer : public Layer<T> {
    public:
        ConvolutionLayer(const json11::Json &config, string dataType) : Layer<T>(config, dataType) {
            auto &param = config["param"];
            outputChannels = param["outputChannels"].int_value();
            outputChannels1 = param["outputChannels1"].int_value();
            outputChannels2 = param["outputChannels1"].int_value();
            if (!outputChannels && outputChannels1 && outputChannels2) {
                outputChannels = outputChannels1 + outputChannels1;
            }
            int kernelSize = param["kernelSize"].int_value();
            kernel_h = param["kernel_h"].int_value();
            kernel_w = param["kernel_w"].int_value();
            if (kernel_h == 0 && kernel_w == 0) {
                kernel_h = kernelSize;
                kernel_w = kernelSize;
            }

            int pad = param["pad"].int_value();
            int pad_h = param["pad_h"].int_value();
            int pad_w = param["pad_w"].int_value();
            if (pad_h == 0 && pad_w == 0) {
                pad_h = pad;
                pad_w = pad;
            }
            pad_u = param["pad_u"].int_value();
            pad_d = param["pad_d"].int_value();
            pad_l = param["pad_l"].int_value();
            pad_r = param["pad_r"].int_value();
            if (pad_u == 0 && pad_d == 0 && pad_l == 0 && pad_r == 0) {
                pad_u = pad_h;
                pad_d = pad_h;
                pad_l = pad_w;
                pad_r = pad_w;
            }

            int stride = ((param["stride"].is_null()) ? 1 : param["stride"].int_value());
            stride_h = ((param["stride_h"].is_null()) ? stride : param["stride_h"].int_value());
            stride_w = ((param["stride_w"].is_null()) ? stride : param["stride_w"].int_value());

            dilation = ((param["dilation"].is_null()) ? 1 : param["dilation"].int_value());
            group = ((param["group"].is_null()) ? 1 : param["group"].int_value());
            crossGroup = ((param["crossGroup"].is_null()) ? 1 : param["crossGroup"].int_value());
            hasBias = param["hasBias"].int_value();
            int32Bias = param["int32Bias"].int_value();
            reverseBack = param["reverseBack"].int_value();
            relu = param["relu"].int_value();
            negative_slope = param["negative_slope"].number_value();
            if (!(param["threshold"].is_null())) {
                threshold = param["threshold"].number_value();
            }
            fixZero = param["fixZero"].int_value();
            originalHeight = param["originalHeight"].int_value();
            batchPadding = param["batchPadding"].int_value();

            this->weights.push_back(new Data<T>);
            if (hasBias) {
                this->weights.push_back(new Data<T>);
            }
            isHWIm2Col = false;
        }
        ~ConvolutionLayer();

        string ToJsonParams();
        json11::Json ToJson();

        bool groupMode();

        void Forward();
        void ForwardTF();
        void Prepare();
        void Reshape();

#if defined(USE_TFACC) || defined(USE_TFCore)
        void fillHighAccuracyBiasData(uint8_t* biasData);
        void fillBiasData(uint8_t* biasData);
        void TFACCInit();
        void TFACCInit(int TFACCId);
        void refreshTFACCBias();
        void GetBlasops(vector <pair <void*, int> > &blasops);
        void TFACCRelease();
        void SetSlicePoints(vector <tfblas::ConvolutionSlicePoint> &slicePoints);
#endif

#ifdef USE_TFACC40T
        long long weightHash = 0;
        void TFACCInit();
        void TFACCReleaseDDR();
        void TFACCMark();
        void TFACCReshape();
        void TFACCRelease();
        bool TFACCSupport();
        void SetDeviceId(int deviceId);
        void FillBlasopLists(int maus);
        void SetMultiDeviceId(set <int> deviceIds);
        bool CanRunOnMultiDevice();

        tfacc40t::ConvolutionLayer convolutionLayer;
        tfacc40t::MEM_U8 *map_table = nullptr;
        std::vector <tfacc40t::MEM_U8*> tempBuffers;
#endif

#ifdef USE_TFCore
        void GetBlasops(vector <tfblas::CommonBlasop> &blasops);
        tfblas::Block block;
#endif

        void loadWeightConfig();
        int GetWeightChannels();
        void ResetInt8Config();
        long long GetOperationTimes();
        double GetExceptedTime(std::string device, int version);

        void cpuGemm(T* input, T* weight, T* output);
        void cpuBias(T* output, T* bias);
        void cpuRelu(T* output, int cnt);

        void PrepareTFACCGemm();
        void TFACCGemm(T* input, T* weight, T* output, int inputOffset, int outputOffset);
        void HighAccuracyTFACCGemm(T* input, T* weight, float* output, int inputOffset, int outputOffset);

        void cpuMatrixMultiply(int n, int m, int k, T* a, T* b, T* c, int biasStart);

        void im2col(T* image, T* col, bool rev);
        string GetParams();
        int isNoPadding();

        void createBias();
        bool mergePreBatchNormLayer(BatchNormLayer<T> *batchNormLayer);
        bool mergeBatchNormLayer(BatchNormLayer<T> *batchNormLayer);
        bool mergePreScaleLayer(ScaleLayer<T> *scaleLayer);
        bool mergeScaleLayer(ScaleLayer<T> *scaleLayer);
        bool mergeReluLayer(ReluLayer<T> *reluLayer);
        bool mergeReluXLayer(ReluXLayer<T> *reluXLayer);

        long long GetWeightDataAmount();

        int getFixZero();
        void setFixZero(int fixZero);
        void setFixCntW(int fixCntW);
        void setFixCntH(int fixCntH);
        void setOriginalHeight(int originalHeight);
        void setBatchPadding(int batchPadding);
        void setAsymmetricPadding(int pad_u, int pad_d, int pad_l, int pad_r);

        void fix(T *output);

        int getKernelH();
        int getKernelW();
        int getPadH();
        int getPadW();
        int setPadH(int padH);
        int setPadW(int padW);
        int getStrideH();
        int getStrideW();
        int getDilation();
        int getGroup();
        vector<int> getAsymmetricPadding();

        bool ifReLU();
        bool ifHasBias();
        bool ifInt32Bias();
        bool ifReverseBack();

        void checkOutput();

        void useReverseIn();
        void useReverseOut();

        vector <QuantizationConfig*> weightConfigs;

        void setOriginalInputScale(float scale);
        float getOriginalInputScale();

        bool isHardWareIm2col();
        bool isGlobalConv();
        bool Prelayer = false;
        void SetActivationLayer(Layer<T>*layer);
        void UnSetActivationLayer();
        void SetSliceLayer(SliceLayer<T> *layer);
        void UnSetSliceLayer();
        void UseHighAccuracyDoubleMultiplier();
        void FreeSingleTFACC(int tfaccId);

        void SaveMiniNet(std::ofstream &outputFile);

        void SetMapTable(vector <uint8_t> &mapTable);

        bool SplitToIm2colAndConvolution(json11::Json &deconvDilation, json11::Json &convolution);
    private:
        int num;
        int channels;
        int height;
        int width;
        int spatial;
        int outputChannels;
        int outputChannels1;
        int outputChannels2;
        int outputHeight;
        int outputWidth;
        int outputSpatial;
        int inputOffset;
        int outputOffset;
        bool isInt8Layer = false;
        bool isFirstTFACCGemm = true;
        bool highAccDoubleMultiplier = false;

        int kernel_h;
        int kernel_w;
        int pad_u, pad_d, pad_l, pad_r;
        int stride_h, stride_w;
        int dilation;
        int group, tfaccGroup, crossGroup = 1;
        bool hasBias; //if hasbia, bias = weights[1] (weight = weights[0])
        bool int32Bias = false; //for using int32 for bias directly in int8 network, default false
        bool reverseBack = false;
        bool condConv = false;
        bool relu;
        float negative_slope;
        float threshold = -1;
        bool isHWIm2Col;
        int fixZero; //fill inputZero to col fixZero.
        int fixCntW = (1 << 30); //fill how many points
        int fixCntH = (1 << 30); //fill how many points
        int originalHeight; //original height
        int batchPadding; //padding for batch
        std::tuple<unsigned int, unsigned int, unsigned int> TFBLASTuple; //for TFACC
        //T* col = nullptr;
        //T* biasData = nullptr;
        //T* newWeight = nullptr;
        T* weight;
        T* bias;

        QuantizationConfig *inputConfig;
        QuantizationConfig *outputConfig;
        magicType inputZero;
        //short* realWeight = nullptr;

        vector <QuantizationConfig*> biasConfigs;
        vector <float> scales;
        Layer<T>*ActivationLayer = nullptr;
        SliceLayer<T> *sliceLayer = nullptr;
        float originalInputScale = 0.0f; //存储原始的inputScale，如果inputScale发生变化，且使用int32bias，那么bias的值相应也要变化

        std::set <long long> hashs; //保留ConvolutionParams的hash，用于析构时删除命令字

        vector <uint8_t> mapTable; // 映射表，为空代表不做激活

        double GetDoubleMultiplier(float inputScale, float weightScale, float outputScale);
    };

}


#endif //CAFFE_CONVOLUTIONLAYER_H
