//
// Created by huangyuyang on 1/4/23.
//

#ifndef TFDL_CONSTANT_H
#define TFDL_CONSTANT_H

#include "../Layer.h"

namespace tfdl {
    template<typename T>
    class ConstantLayer : public Layer<T> {
    public:
        ConstantLayer(const json11::Json &config, string dataType) : Layer<T>(config, dataType) {
            auto param = config["param"];
            value = !param["value"].is_null() ? param["value"].number_value() : 0;
        }

        ~ConstantLayer() {
        }

        void Forward();

        void ForwardTF();

        void Reshape();

        json11::Json ToJson();
    private:
        float value;
    };
}

#endif //TFDL_CONSTANT_H
