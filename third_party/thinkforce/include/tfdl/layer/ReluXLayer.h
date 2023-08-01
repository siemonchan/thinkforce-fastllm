//
// Created by siemon on 1/14/19.
//

#ifndef PROJECT_RELUXLAYER_H
#define PROJECT_RELUXLAYER_H

#include "../Layer.h"
#include <float.h>
#ifdef ARM
#include <arm_neon.h>
#endif

namespace tfdl {
    template <typename T>
    class ReluXLayer : public Layer<T> {
    public:
        ReluXLayer(const json11::Json &config, string dataType) : Layer<T>(config, dataType) {
            auto &param = config["param"];
            threshold = param["threshold"].number_value();
        }

        ~ReluXLayer() {
        }

        string ToJsonParams();
        json11::Json ToJson();
        void Forward();
        void ForwardTF();
        void Prepare();
        void cpuFloatReluX(float* input, float* output);
        float getThreshold();

    private:
        float threshold = -1;
    };
}

#endif //PROJECT_RELUXLAYER_H
