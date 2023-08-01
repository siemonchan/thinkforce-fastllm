//
// Created by huangyuyang on 8/7/20.
//

#ifndef PROJECT_SOFTMAXMASKLAYER_H
#define PROJECT_SOFTMAXMASKLAYER_H

#include "../Layer.h"

namespace tfdl {
    template<typename T>
    class SoftmaxMaskLayer : public Layer<T> {
    public:
        SoftmaxMaskLayer(const json11::Json &config, string dataType) : Layer<T>(config, dataType) {
            auto param = config["param"];
            axis = param["axis"].int_value();
        }
        ~SoftmaxMaskLayer() {
        }

        json11::Json ToJson();
        void Forward();
        void ForwardTF();
        void SaveMiniNet(std::ofstream &outputFile);
    private:
        int axis;
    };
}

#endif //PROJECT_SOFTMAXMASKLAYER_H
