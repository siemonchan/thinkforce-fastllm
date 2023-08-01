//
// Created by huangyuyang on 1/4/23.
//

#ifndef TFDL_SHAPE_H
#define TFDL_SHAPE_H

#include "../Layer.h"

namespace tfdl {
    template<typename T>
    class ShapeLayer : public Layer<T> {
    public:
        ShapeLayer(const json11::Json &config, string dataType) : Layer<T>(config, dataType) {
            auto param = config["param"];
            start = !param["start"].is_null() ? param["start"].int_value() : 0;
            end = !param["end"].is_array() ? param["end"].int_value() : 0;
        }

        ~ShapeLayer() {
        }

        void Forward();

        void ForwardTF();

        void Reshape();

        json11::Json ToJson();

    private:
        int start, end;
    };
}

#endif //TFDL_SHAPE_H
