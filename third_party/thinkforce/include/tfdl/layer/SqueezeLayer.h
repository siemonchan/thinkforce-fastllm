//
// Created by siemon on 10/22/20.
//

#ifndef TFDL_SQUEEZELAYER_H
#define TFDL_SQUEEZELAYER_H

#include "Layer.h"

namespace tfdl {
    template<typename T>
    class SqueezeLayer : public Layer<T> {
    public:
        SqueezeLayer(const json11::Json &config, string dataType) : Layer<T>(config, dataType) {
            auto param = config["param"];
            axis.clear();
            for (auto &item : param["axis"].array_items()) {
                axis.push_back(item.int_value());
            }
        }

        void Forward();

        void ForwardTF();

        void Reshape();

        string AxisString();

        string ToJsonParams();

        json11::Json ToJson();

    private:
        vector<int> axis;
    };
}

#endif //TFDL_SQUEEZELAYER_H
