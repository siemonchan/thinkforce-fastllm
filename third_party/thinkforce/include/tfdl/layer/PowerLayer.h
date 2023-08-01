//
// Created by yuyang.huang on 17-11-22.
//

#ifndef TFDL_POWERLAYER_H
#define TFDL_POWERLAYER_H

#include "../Layer.h"
#include "layer/MapTableLayer.h"

namespace tfdl {
    template<typename T>
    class PowerLayer : public Layer<T> {
    public:
        PowerLayer(const json11::Json &config, string dataType) : Layer<T>(config, dataType) {
            auto &param = config["param"];
            shift = param["shift"].number_value();
            scale = param["scale"].number_value();
            power = param["power"].number_value();
        }

        ~PowerLayer() {

        }

        string ToJsonParams();
        json11::Json ToJson();
        void Forward();
        void ForwardTF();
        void Prepare();

#ifdef USE_TFACC40T
        void TFACCInit();

        void TFACCMark();

        bool TFACCSupport();

        void TFACCRelease();

        MapTableLayer<T> *mapTableLayer = nullptr;
#endif

        float shift, scale, power;
        bool isEqual = false; //恒等映射
    };


}

#endif //CAFFE_POWERLAYER_H
