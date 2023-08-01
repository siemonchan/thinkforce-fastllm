//
// Created by siemon on 7/26/19.
//

#ifndef PROJECT_DEQUANTIZELAYER_H
#define PROJECT_DEQUANTIZELAYER_H

#include "Layer.h"

namespace tfdl {
    template <typename T>
    class DequantizeLayer : public Layer<T> {
    public:
        DequantizeLayer(const json11::Json &config, const string &dataType) : Layer<T>(config, dataType) {
            assert(this->inputDataType == "int8");
            assert(this->outputDataType == "float");
        }

        void Forward();
        void ForwardTF();

        json11::Json ToJson();
    };
}

#endif //PROJECT_DEQUANTIZELAYER_H
