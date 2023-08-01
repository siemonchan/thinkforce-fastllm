//
// Created by siemon on 8/7/19.
//

#ifndef PROJECT_SHUFFLEPIXEL_H
#define PROJECT_SHUFFLEPIXEL_H

#include "Layer.h"

namespace tfdl {
    template <typename T>
    class ShufflePixel : public Layer<T> {
    public:
        ShufflePixel(const json11::Json &config, string dataType) : Layer<T>(config, dataType) {
            auto param = config["param"];
            upsacleFactor = param["scale"].int_value();
        }

        ~ShufflePixel() {
            delete[] oldPos;
        }

        void Forward();
        void ForwardTF();
        void Reshape();

        string ToJsonParams();
        json11::Json ToJson();

    private:
        int upsacleFactor;
        int* oldPos = nullptr;
    };
}

#endif //PROJECT_SHUFFLEPIXEL_H
