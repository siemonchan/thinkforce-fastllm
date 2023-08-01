//
// Created by yuyang.huang on 17-11-22.
//

#ifndef TFDL_SIGMOIDLAYER_H
#define TFDL_SIGMOIDLAYER_H

#include "../Layer.h"
#include "MapTableLayer.h"

namespace tfdl {
    template<typename T>
    class SigmoidLayer : public Layer<T> {
    public:
        SigmoidLayer(const json11::Json &config, string dataType) : Layer<T>(config, dataType) {
            auto param = config["param"];
            hard = param["hard"].int_value();
        }

        ~SigmoidLayer() {

        }

        bool IsHard();

        float sigmoid(float x);

        void Forward();
        void ForwardTF();
        void Prepare();

        string ToJsonParams();
        json11::Json ToJson();
        void SaveMiniNet(std::ofstream &outputFile);

#ifdef USE_TFACC40T
        void TFACCInit();

        void TFACCMark();

        bool TFACCSupport();

        void TFACCRelease();
#endif

    private:
        bool hard = false;

#ifdef USE_TFACC40T
        MapTableLayer<T> *mapTableLayer = nullptr;
#endif
    };


}

#endif //CAFFE_SIGMOIDLAYER_H
