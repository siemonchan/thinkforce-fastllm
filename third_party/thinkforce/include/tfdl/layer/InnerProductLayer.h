//
// Created by yuyang.huang on 17-11-9.
//

#ifndef TFDL_INNERPRODUCTLAYER_H
#define TFDL_INNERPRODUCTLAYER_H

#include "../Layer.h"
#ifdef USE_TFACC
#include <tfblas_api.hpp>
#include <common.hpp>
#endif

namespace tfdl {
    template<typename T>
    class InnerProductLayer : public Layer<T> {
    public:
        InnerProductLayer(const json11::Json &config, string dateType) : Layer<T>(config, dateType) {
            auto param = config["param"];
            outputChannels = param["outputChannels"].int_value();
            hasBias = param["hasBias"].int_value();
            int32Bias = param["int32Bias"].int_value();
            relu = param["relu"].bool_value();
            fromConvolution = param["fromConvolution"].bool_value();
            axis = param["axis"].is_null() ? 1 : param["axis"].int_value();

            if (this->weightDataType == "float") {
                this->weights.push_back((Data<T>*)(new Data<float>("float")));
                if (hasBias) {
                    this->weights.push_back((Data<T>*)(new Data<float>("float")));
                }
            } else if (this->weightDataType == "int8") {
                this->weights.push_back(new Data<T>("int8"));
                if (hasBias) {
                    this->weights.push_back(new Data<T>("int8"));
                }
            } else if (this->weightDataType == "int16") {
                this->weights.push_back((Data<T>*)(new Data<float>("float")));
                if (hasBias) {
                    this->weights.push_back((Data<T>*)(new Data<float>("float")));
                }
            }
        }
        ~InnerProductLayer() {
            if (!this->ShareAttr) {
                for (Data<T> *d : this->weights) {
                    delete d;
                }
            }
        }

        int GetWeightChannels();

        json11::Json ToJson();
        void Forward();
        void ForwardTF();
        void Prepare();
        void Reshape();
        void loadWeightConfig();
        void EnableReLU();
        void DisableReLU();
        long long GetOperationTimes();
        int GetOutputChannels();
        void setOriginalInputScale(float scale);
        float getOriginalInputScale();
        void SetMode(int mode);
        void OnlySetMode(int mode);
        int GetMode();
        void SetFromConvolution(bool fromConvolution);
        bool GetFromConvolution();
//#ifdef USE_TFACC
        void TFACCInit();
        void SetStartChannel(int i){StartChannel = i;};
        string GetParams();
//#endif
#ifdef USE_TFACC
        tfblas::MmapBuf<magicType>* tfaccWeight = nullptr;
#endif
        double GetExceptedTime(std::string device, int version);
        void SaveMiniNet(std::ofstream &outputFile);
#ifdef USE_TFACC40T
        long long weightHash = 0;
        void TFACCReleaseDDR();
        void TFACCMark();
        void TFACCRelease();
        bool TFACCSupport();
        void Launch();
        bool IsFinish();
        void Wait();
        tfacc40t::ConvolutionLayer fcLayer;
#endif
        bool SplitToConvolution(json11::Json &beforePermuteConfig,
                                json11::Json &beforeReshapeConfig,
                                json11::Json &convolutionConfig,
                                json11::Json &afterPermuteConfig,
                                json11::Json &afterReshapeConfig,
                                vector <float> &weight, vector <float> &bias);
    private:
        int outputChannels;
        bool hasBias = false; //if hasbia, bias = weights[1] (weight = weights[0])
        bool int32Bias = false;
        int axis = 1;
        bool relu = false;
        bool weightsumshare = false;
        int mode = 0;

        float originalInputScale = 0.0f; //存储原始的inputScale，如果inputScale发生变化，且使用int32bias，那么bias的值相应也要变化
        int StartChannel = 0;
        vector <QuantizationConfig*> weightConfigs;
        vector <QuantizationConfig*> biasConfigs;

        Int16QuantizationConfig int16WeightConfig; //用于半高精度模式
        uint16_t * int16Weights = nullptr;
        bool fromConvolution = false;
    };
}

#endif //CAFFE_INNERPRODUCTLAYER_H
