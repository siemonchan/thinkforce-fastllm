//
// Created by siemon on 4/7/21.
//

#ifndef TFDL_SPACETODEPTHLAYER_H
#define TFDL_SPACETODEPTHLAYER_H

#include "Layer.h"

namespace tfdl {
    template<typename T>
    class SpaceToDepthLayer : public Layer<T> {
    public:
        SpaceToDepthLayer(const json11::Json &config, string dataType) : Layer<T>(config, dataType) {
            auto param = config["param"];
            blockSize = param["blockSize"].int_value();
            focus = param["focus"].int_value();
        }

        ~SpaceToDepthLayer() {
            delete[] oldPos;
        }

        void Forward();

        void ForwardTF();

        void Reshape();

        string ToJsonParams();

        json11::Json ToJson();

    private:
        int blockSize;
        // for focus operation in yolov5
        bool focus = false;
        int* oldPos = nullptr;
    };
}

#endif //TFDL_SPACETODEPTHLAYER_H
