//
// Created by siemon on 5/10/19.
//

#ifndef PROJECT_HARDSWISHLAYER_H
#define PROJECT_HARDSWISHLAYER_H

#include "Layer.h"
#include "MapTableLayer.h"

namespace tfdl {
    template <typename T>
    class SwishLayer : public Layer<T> {
    public:
        SwishLayer(const json11::Json &config, string dataType) : Layer<T>(config, dataType) {
            auto param = config["param"];
            hard = param["hard"].int_value();
        }

        ~SwishLayer(){

        }

        void Prepare();

        float Sigmoid(float x);
        float Swish(float x);
        void Forward();
        void ForwardTF();
        json11::Json ToJson();
        string ToJsonParams();
        bool Hard();

        bool IsActivationLayer();
        void GetMapTable(vector <uint8_t> &mapTable);

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

#endif //PROJECT_HARDSWISHLAYER_H
