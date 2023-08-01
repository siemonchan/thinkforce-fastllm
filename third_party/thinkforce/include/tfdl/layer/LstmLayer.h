//
// Created by cjq on 18-11-22.
//

#ifndef PROJECT_LSTMLAYER_H
#define PROJECT_LSTMLAYER_H

#include "Layer.h"
#ifdef ARM
#include <arm_neon.h>
#include "armFunctions.h"
#endif
#ifdef USE_TFACC
#include <tfblas_api.hpp>
#include <common.hpp>
#endif

namespace tfdl {
    template<typename T>
    class LstmLayer : public Layer<T> {
    public:
        LstmLayer(const json11::Json &config, const string &dataType) : Layer<T>(config, dataType){
            auto &param = config["param"];
            numOutput = param["numOutput"].int_value();
            returnSequence = param["returnSequence"].int_value();
            recurrentActivation = param["recurrentActivation"].string_value();

            //kernel
            this->weights.push_back(new Data<T>);
            //recurrentKernel
            this->weights.push_back(new Data<T>);
            //bias
            this->weights.push_back(new Data<T>);
        }
        ~LstmLayer(){

        }

        void Forward();
        void ForwardTF();
        void cpuFloatLstm(float* input, float*output, int hc_w, int xc_w, int t, int size, int numOutput);
        void cpuFloatLstmK(float *input, float *output, int t, int size, int numOutput);
        void cpuInt8Lstm(magicType* input, magicType* output, int t, int size, int numOutput);
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


#endif //PROJECT_LSTMLAYER_H