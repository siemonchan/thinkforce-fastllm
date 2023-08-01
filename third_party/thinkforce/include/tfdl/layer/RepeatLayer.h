//
// Created by huangyuyang on 11/25/19.
//

#ifndef PROJECT_REPEATLAYER_H
#define PROJECT_REPEATLAYER_H

#include "../Layer.h"

namespace tfdl {
    template<typename T>
    class RepeatLayer : public Layer<T> {
    public:
        RepeatLayer(const json11::Json &config, string dataType) : Layer<T>(config, dataType) {
            auto &param = config["param"];
            axis = param["axis"].is_null() ? INT32_MAX : param["axis"].int_value();
            repeats = param["repeats"].int_value();
        }
        ~RepeatLayer() {
        }

        string ToJsonParams();
        json11::Json ToJson();
        void Forward();
        void ForwardTF();
        void Reshape();
    private:
        int axis;
        int realAxis;
        int repeats;
    };
}

#endif //PROJECT_REPEATLAYER_H
