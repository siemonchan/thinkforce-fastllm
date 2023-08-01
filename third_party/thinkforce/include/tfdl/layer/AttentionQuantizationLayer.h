//
// Created by huangyuyang on 12/24/19.
//

#ifndef PROJECT_ATTENTIONQUANTIZATIONLAYER_H
#define PROJECT_ATTENTIONQUANTIZATIONLAYER_H

#include "../Layer.h"

namespace tfdl {
    template<typename T>
    class AttentionQuantizationLayer : public Layer<T> {
    public:
        AttentionQuantizationLayer(const json11::Json &config, string dataType) : Layer<T>(config, dataType) {
            auto &param = config["param"];
            useAttention = param["useAttention"].is_null() ? true : param["useAttention"].int_value();
            centers.clear();
            for (const auto &id : param["centers"].array_items()) {
                centers.push_back(id.number_value());
            }
        }

        ~AttentionQuantizationLayer() {

        }

        string centersString();

        string ToJsonParams();

        json11::Json ToJson();

        void Attention(float *input, float *output);

        void Forward();

        void ForwardTF();

        void Reshape();

        void MakeTable();
    private:
        bool useAttention;
        vector<float> centers;
    };
}

#endif //PROJECT_ATTENTIONQUANTIZATIONLAYER_H
