//
// Created by siemon on 10/23/20.
//

#ifndef TFDL_INSTANCENORMALIZATIONLAYER_H
#define TFDL_INSTANCENORMALIZATIONLAYER_H

#include "Layer.h"

namespace tfdl {
    template<typename T>
    class InstanceNormalizationLayer : public Layer<T> {
    public:
        InstanceNormalizationLayer(const json11::Json &config, string dataType) : Layer<T>(config, dataType) {
            auto param = config["param"];
            if (!param["eps"].is_null()) {
                this->var_eps = static_cast<float>(param["eps"].number_value());
            }
        }

        ~InstanceNormalizationLayer() {
            delete[] float_mean;
            delete[] float_variance;
        }

        void Forward();

        void ForwardTF();

        void Prepare();

        string ToJsonParams();

        json11::Json ToJson();

    private:

        void ColMeanAndVariance(const float *input, uint32_t rows, uint32_t cols, float *mean, float *variance);

        void InstanceNorm(const float *input, uint32_t rows, uint32_t cols, float *output);

        void QuantizedInstanceNorm(const uint8_t *input, int outer, int inner, QuantizationConfig *input_config,
                                   QuantizationConfig *output_config, uint8_t *output);

        float var_eps = 1e-5f;
        float *float_mean = nullptr;
        float *float_variance = nullptr;
    };
}

#endif //TFDL_INSTANCENORMALIZATIONLAYER_H
