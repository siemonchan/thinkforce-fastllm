//
// Created by huangyuyang on 1/4/23.
//

#ifndef TFDL_CONSTANTOFSHAPE_H
#define TFDL_CONSTANTOFSHAPE_H

#include "../Layer.h"

namespace tfdl {
    template<typename T>
    class ConstantOfShapeLayer : public Layer<T> {
    public:
        ConstantOfShapeLayer(const json11::Json &config, string dataType) : Layer<T>(config, dataType) {
            auto param = config["param"];
            value = !param["value"].is_null() ? param["value"].number_value() : 0;
        }

        ~ConstantOfShapeLayer() {
        }

        void Forward();

        void ForwardTF();

        void Reshape();

        json11::Json ToJson();
    private:
        float value;
    };
}

#endif //TFDL_CONSTANTOFSHAPE_H
