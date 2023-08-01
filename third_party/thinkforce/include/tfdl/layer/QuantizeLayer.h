//
// Created by siemon on 7/26/19.
//

#ifndef PROJECT_QUANTIZELAYER_H
#define PROJECT_QUANTIZELAYER_H

#include "Layer.h"

namespace tfdl {
    template <typename T>
    class QuantizeLayer : public Layer<T> {
    public:
        QuantizeLayer(const json11::Json &config, const string &dataType) : Layer<T>(config, dataType) {
            assert(this->inputDataType == "float");
            assert(this->outputDataType == "int8");
        }

        void Forward();
        void ForwardTF();

        json11::Json ToJson();
    };
}

#endif //PROJECT_QUANTIZELAYER_H
