//
// Created by siemon on 11/4/20.
//

#ifndef TFDL_LEAKYRELULAYER_H
#define TFDL_LEAKYRELULAYER_H

#include "Layer.h"
#include "MapTableLayer.h"

namespace tfdl {
    template<typename T>
    class LeakyReluLayer : public Layer<T> {
    public:
        LeakyReluLayer(const json11::Json &config, string dataType) : Layer<T>(config, dataType) {
            auto param = config["param"];
            if (!param["alpha"].is_null()) {
                this->alpha = (float) param["alpha"].number_value();
            } else if (!param["negative_slope"].is_null()) {
                this->alpha = (float) param["negative_slope"].number_value();
            }
        }

        void Forward();

        void ForwardTF();

        void Prepare();

        string ToJsonParams();

        json11::Json ToJson();


        bool IsActivationLayer();
        void GetMapTable(vector <uint8_t> &mapTable);

#ifdef USE_TFACC40T
        void TFACCInit();

        void TFACCMark();

        bool TFACCSupport();

        void TFACCRelease();
#endif

    private:
        float alpha = 1.f;

        float leaky_relu(float x) {
            return x > 0 ? x : x * alpha;
        }

#ifdef USE_TFACC40T
        MapTableLayer<T> *mapTableLayer = nullptr;
#endif
    };
}

#endif //TFDL_LEAKYRELULAYER_H
