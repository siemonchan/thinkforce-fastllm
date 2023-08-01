//
// Created by huangyuyang on 11/22/19.
//

#ifndef PROJECT_ARANGELAYER_H
#define PROJECT_ARANGELAYER_H

#include "../Layer.h"

namespace tfdl {
    template<typename T>
    class ArangeLayer : public Layer<T> {
    public:
        ArangeLayer(const json11::Json &config, string dataType) : Layer<T>(config, dataType) {
            auto &param = config["param"];
            start = param["start"].is_null() ? 0.f : param["start"].number_value();
            stop = param["stop"].is_null() ? 1.f : param["stop"].number_value();
            step = param["step"].is_null() ? 1.f : param["step"].number_value();
            repeat = param["repeat"].is_null() ? 1 : param["repeat"].int_value();
        }

        ~ArangeLayer() {
        };

        void Forward();

        void ForwardTF();

        void Reshape();

        string ToJsonParams();

        json11::Json ToJson();

    private:
        float start, stop, step;
        int repeat;
    };
}

#endif //PROJECT_ARANGELAYER_H
