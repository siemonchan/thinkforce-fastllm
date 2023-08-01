//
// Created by hyy on 18-8-9.
//

#ifndef PROJECT_CONVOLUTIONLIB_H
#define PROJECT_CONVOLUTIONLIB_H

#include <string>
#include "INT8Config.h"

namespace tfdl {
    template <typename T>
    class ConvolutionClass {
        string dataType;

        T *weight, *bias, *input, *output;
        short *realWeight;
        vector <QuantizationConfig*> *weightConfigs, *biasConfigs;
        QuantizationConfig *inputConfig, *outputConfig;

        int inputChannels, inputHeight, inputWidth;
        int outputChannels, outputHeight, outputWidth;

        int kernelH, kernelW, stride, padH, padW, group;

        int inputOffset;
        int outputOffset;
        int singleGroupInputChannels;
        int singleGroupOutputChannels;
        int kernelDim;

        bool int32Bias;
    public:
        ConvolutionClass(string dataType, T* weight, short* realWeight, T* bias, T* input, T* output,
                         vector <QuantizationConfig*> *weightConfigs, vector <QuantizationConfig*> *biasConfigs, QuantizationConfig *inputConfig, QuantizationConfig *outputConfig,
                         int inputChannels, int inputHeight, int inputWidth,
                         int outputChannels, int outputHeight, int outputWidth,
                         int kernelH, int kernelW, int stride, int padH, int padW, int group, bool int32Bias) :
                dataType(dataType), weight(weight), realWeight(realWeight), bias(bias), input(input), output(output),
                weightConfigs(weightConfigs), biasConfigs(biasConfigs), inputConfig(inputConfig), outputConfig(outputConfig),
                inputChannels(inputChannels), inputHeight(inputHeight), inputWidth(inputWidth),
                outputChannels(outputChannels), outputHeight(outputHeight), outputWidth(outputWidth),
                kernelH(kernelH), kernelW(kernelW), stride(stride), padH(padH), padW(padW), group(group), int32Bias(int32Bias)
        {
            inputOffset = inputHeight * inputWidth;
            outputOffset = outputHeight * outputWidth;
            singleGroupInputChannels = inputChannels / group;
            singleGroupOutputChannels = outputChannels / group;
            kernelDim = kernelH * kernelW * singleGroupInputChannels;
        }

        void NextBatch();
        void Convolution();
        void SingleConvolution(int groupID); //convolution for one group

        //int8 convolution
        void ConvolutionNormalInt8(int groupID); //normal, int8
        void Convolution31SingleChannelInt8(int groupID); //kernel = 3, stride = 1, int8
        void Convolution41SingleChannelInt8(int groupID); //kernel = 4, pad = 1, stride = 1, int8
        void ConvolutionFC(int groupID); //kernelSize = inputSize, stride = 1, pad = 0, int8
        void ConvolutionFC4(int groupID); //kernel = 4, stride = 1, pad = 0, height = width = 4, int8
    };
}

#endif //PROJECT_CONVOLUTIONLIB_H
