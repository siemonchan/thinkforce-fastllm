//
// Created by siemon on 11/27/20.
//

#ifndef TFDL_SELULAYER_H
#define TFDL_SELULAYER_H

#include "Layer.h"
#include "MapTableLayer.h"

namespace tfdl {
    template<typename T>
    class SeluLayer : public Layer<T> {
    public:
        SeluLayer(const json11::Json &config, string dataType) : Layer<T>(config, dataType) {
            auto param = config["param"];
            if (!param["alpha"].is_null()) {
                this->alpha = (float) param["alpha"].number_value();
            }
            if (!param["gamma"].is_null()) {
                this->gamma = (float) param["gamma"].number_value();
            }
        }

        void Forward();

        void ForwardTF();

        void Prepare();

        json11::Json ToJson();

#ifdef USE_TFACC40T
        void TFACCInit();

        void TFACCMark();

        bool TFACCSupport();

        void TFACCRelease();
#endif

    private:
        float alpha = 1.67326;
        float gamma = 1.0507;

        float selu(float x) {
            return x <= 0 ? alpha * expf(x) - alpha : gamma * x;
        }

#ifdef USE_TFACC40T
        MapTableLayer<T> *mapTableLayer = nullptr;
#endif
    };
}

#endif //TFDL_SELULAYER_H
