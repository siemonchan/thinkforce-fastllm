//
// Created by siemon on 4/29/20.
//

#ifndef ZYNQ7020_ELULAYER_H
#define ZYNQ7020_ELULAYER_H

#include "Layer.h"
#include "MapTableLayer.h"

namespace tfdl {
    template <typename T>
    class EluLayer : public Layer<T> {
    public:
        EluLayer(const json11::Json config, string dataType) : Layer<T>(config, dataType) {
            auto param = config["param"];
            if (!param["alpha"].is_null()) {
                alpha = float(param["alpha"].number_value());
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
        float alpha = 1.f;

        float elu(float x) {
            return x >= 0 ? x : alpha * (exp(x) - 1);
        }

#ifdef USE_TFACC40T
        MapTableLayer<T> *mapTableLayer = nullptr;
#endif
    };
}

#endif //ZYNQ7020_ELULAYER_H
