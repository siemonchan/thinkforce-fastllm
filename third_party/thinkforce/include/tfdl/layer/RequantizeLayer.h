//
// Created by siemon on 7/26/19.
//

#ifndef PROJECT_REQUANTIZELAYER_H
#define PROJECT_REQUANTIZELAYER_H

#include "Layer.h"
#include "utils.h"

namespace tfdl {
    template <typename T>
    class RequantizeLayer : public Layer<T> {
    public:
        RequantizeLayer(const json11::Json &config, const string &dataType) : Layer<T>(config, dataType) {
            auto param = config["param"];
            zero = param["zeroPoint"].int_value();
            scale = float(param["scale"].number_value());
        }

        void Forward();
        void ForwardTF();
        void Prepare();
        string ToJsonParams();
        json11::Json ToJson();
    private:
        int zero;
        float scale;
    };
}

#endif //PROJECT_REQUANTIZELAYER_H
