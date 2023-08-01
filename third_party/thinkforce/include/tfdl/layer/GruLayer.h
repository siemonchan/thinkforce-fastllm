//
// Created by siemon on 11/29/18.
//

#ifndef PROJECT_GRULAYER_H
#define PROJECT_GRULAYER_H

#include "Layer.h"
#ifdef ARM
#include <arm_neon.h>
#include "armFunctions.h"
#endif
#ifdef USE_TFACC
#include <tfblas_api.hpp>
#include <common.hpp>
#endif

namespace tfdl{
    template <typename T>
    class GruLayer : public Layer<T> {
    public:
        GruLayer(const json11::Json &config, const string &dataType) : Layer<T>(config, dataType){
            auto &param = config["param"];
            numOutput = param["numOutput"].int_value();
            returnSequence = param["returnSequence"].int_value();
            recurrentActivation = param["recurrentActivation"].string_value();

            //W weight
            this->weights.push_back(new Data<T>);
            //U weight
            this->weights.push_back(new Data<T>);
            //bias
            this->weights.push_back(new Data<T>);
        }
        ~GruLayer(){

        }

        void Forward();
        void ForwardTF();
        void cpuFloatGru(float* input, float* output, int timeSteps, int inputDim, int numOutput);
        void Reshape();
        float sigmoid(float x);
        void bias_add(float* x, const float* b, int len);
        float dot(const float* x, const float* y, int len);
        string ToJsonParams();
        json11::Json ToJson();

    private:
        int numOutput;
        bool returnSequence;
        string recurrentActivation;
    };
}

#endif //PROJECT_GRULAYER_H
