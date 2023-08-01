//
// Created by siemon on 4/24/20.
//

#ifndef ZYNQ7020_MISHLAYER_H
#define ZYNQ7020_MISHLAYER_H

#include "Layer.h"
#include "MapTableLayer.h"

namespace tfdl {
    template<typename T>
    class MishLayer : public Layer<T> {
    public:
        MishLayer(const json11::Json &config, string dataType) : Layer<T>(config, dataType) {

        }

        ~MishLayer() {

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
        bool IsActivationLayer();
        void GetMapTable(vector <uint8_t> &mapTable);
    private:
        float mish(float x) {
            return x * tanh(log(1 + exp(x)));
        }

#ifdef USE_TFACC40T
        MapTableLayer<T> *mapTableLayer = nullptr;
#endif
    };
}

#endif //ZYNQ7020_MISHLAYER_H
