//
// Created by yuyang.huang on 17-11-9.
//

#ifndef TFDL_RELULAYER_H
#define TFDL_RELULAYER_H

#include "../Layer.h"
#include "MapTableLayer.h"
#ifdef ARM
#include <arm_neon.h>
#endif

namespace tfdl {
    template<typename T>
    class ReluLayer : public Layer<T> {
    public:
        ReluLayer(const json11::Json &config, string dataType) : Layer<T>(config, dataType) {
            auto &param = config["param"];
            negative_slope = param["negative_slope"].number_value();
        }

        ~ReluLayer() {
        }

        string ToJsonParams();
        json11::Json ToJson();
        void Forward();
        void ForwardTF();
        void Prepare();
        void cpuFloatRelu(float* input, float* output);
        float getNegativeSlope();
        void SaveMiniNet(std::ofstream &outputFile);


        bool IsActivationLayer();
        void GetMapTable(vector <uint8_t> &mapTable);

#ifdef USE_TFACC40T
        void TFACCInit();

        void TFACCMark();

        bool TFACCSupport();

        void TFACCRelease();
#endif
    private:
        float negative_slope;
#ifdef USE_TFACC40T
        MapTableLayer<T> *mapTableLayer = nullptr;
#endif
    };

}

#endif //CAFFE_RELULAYER_H
