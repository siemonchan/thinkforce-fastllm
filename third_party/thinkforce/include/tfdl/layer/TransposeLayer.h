//
// Created by root on 3/21/23.
//

#ifndef TFDL_TRANSPOSELAYER_H
#define TFDL_TRANSPOSELAYER_H

#include "Layer.h"

namespace tfdl {
    template<typename T>
    class TransposeLayer : public Layer<T> {
    public:
        TransposeLayer(const json11::Json &config, string dataType) : Layer<T>(config, dataType) {
            auto &param = config["param"];
            outputDims.clear();
            for (auto &id : param["outputDims"].array_items()) {
                outputDims.push_back(id.int_value());
            }

            this->weightDataType = "float";
        }

        void Forward();

        void ForwardTF();

        void Reshape();

        string outputDimsString();

        string ToJsonParams();

        json11::Json ToJson();

    private:
        vector<int> outputDims;
    };
}

#endif //TFDL_TRANSPOSELAYER_H
