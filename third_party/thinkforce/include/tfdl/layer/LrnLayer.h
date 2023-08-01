//
// Created by root on 17-12-12.
//

#ifndef PROJECT_LRNLAYER_H
#define PROJECT_LRNLAYER_H

#include "../Layer.h"
#ifdef ARM
#include <arm_neon.h>
#include "armFunctions.h"
#endif

namespace tfdl {
    template<typename T>
    class LrnLayer : public Layer<T> {
    public:
        LrnLayer(const json11::Json &config, const string &dataType) : Layer<T>(config, dataType) {
            auto &param = config["param"];
            size = param["size"].int_value();
            pad = (size - 1) / 2;
            alpha = param["alpha"].number_value();
            beta = param["beta"].number_value();
            k = 1;
        }

        ~LrnLayer() {
        }

        string ToJsonParams();
        json11::Json ToJson();
        void Forward();
        void ForwardTF();

        void Prepare();
        void Reshape();
    private:
        float alpha;
        float beta;
        float k;
        int size;
        int pad;

        //Data<float>* scale = new Data<float>();

        //Data<float> *padded_squares;
        //Data<float> *input_fp;

        //magicType* ans = nullptr;
        //float* realValue = nullptr;
        //float* powAns = nullptr;
        int limit;
    };
}


#endif //PROJECT_LRNLAYER_H
