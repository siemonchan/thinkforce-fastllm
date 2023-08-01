//
// Created by huangyuyang on 10/28/20.
//

#ifndef PROJECT_SOFTMAXTOPKLAYER_H
#define PROJECT_SOFTMAXTOPKLAYER_H

#include "../Layer.h"

namespace tfdl {
    struct SoftMaxTopKElement {
        float score;
        int index;
    };

    template<typename T>
    class SoftMaxTopKLayer : public Layer<T> {
    public:
        SoftMaxTopKLayer(const json11::Json &config, string dataType) : Layer<T>(config, dataType) {
            auto &param = config["param"];
            top = ((param["top"].is_null()) ? 40 : param["top"].number_value());
            axis = ((param["axis"].is_null()) ? 1 : param["axis"].number_value());
        }

        ~SoftMaxTopKLayer() {
        }

        json11::Json ToJson();
        int SoftMaxTopKOpt(const uint8_t *input, float *output, int M, int N, int K);
        int SoftMaxTopKBase(const uint8_t *input, float *output, int M, int N, int K);
        int SoftMaxTopKFloat(const float *input, float *output, int M, int N, int K);
        void Forward();
        void ForwardTF();
        void Prepare();
        void Reshape();

        void cpuFloatSoftMax(float *input, float *output, int outer, int channels, int inner);
    private:
        int axis;
        int top;
    };
}

#endif //PROJECT_SOFTMAXTOPKLAYER_H
