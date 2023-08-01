//
// Created by test on 18-9-4.
//

#ifndef PROJECT_DeconvolutionLayer_H
#define PROJECT_DeconvolutionLayer_H

//#include <Net.h>
#include <Layer.h>
#include <layer/BatchNormLayer.h>
#include <layer/ScaleLayer.h>
#include <layer/ReluLayer.h>
#ifdef WITH_BLAS
#include <cblas.h>
#include "../frogHeart/include/common.hpp"
#endif

namespace tfdl {

    template<typename T>
    class DeconvolutionLayer : public Layer<T> {

    public:
        DeconvolutionLayer(const json11::Json &config, string dataType) : Layer<T>(config, dataType) {
            auto &param = config["param"];
            outputChannels = param["outputChannels"].int_value();
            int kernelSize = param["kernelSize"].int_value();
            kernel_h = param["kernel_h"].int_value();
            kernel_w = param["kernel_w"].int_value();
            if (kernel_h == 0 && kernel_w == 0) {
                kernel_h = kernelSize;
                kernel_w = kernelSize;
            }
            if(param["output_padding"].is_null ()){
                outpadH=0;
                outpadW=0;
              } else{
                assert(param["output_padding"].array_items ().size () ==2);
                outpadH=param["output_padding"].array_items ()[0].int_value ();
                outpadW=param["output_padding"].array_items ()[1].int_value ();
              }
            int pad = param["pad"].int_value();
            pad_h = param["pad_h"].int_value();
            pad_w = param["pad_w"].int_value();
            if (pad_h == 0 && pad_w == 0) {
                pad_h = pad;
                pad_w = pad;
            }
            stride = ((param["stride"].is_null()) ? 1 : param["stride"].int_value());
            dilation = ((param["dilation"].is_null()) ? 1 : param["dilation"].int_value());
            group = ((param["group"].is_null()) ? 1 : param["group"].int_value());
            hasBias = param["hasBias"].int_value();
            int32Bias = param["int32Bias"].int_value();
            relu = param["relu"].int_value();
            pooling = param["pooling"].int_value();
            negative_slope = param["negative_slope"].number_value();
            fixZero = param["fixZero"].int_value();

            this->weights.push_back(new Data<T>);
            if (hasBias) {
                this->weights.push_back(new Data<T>);
            }
            isHWIm2Col = false;
        }

        ~DeconvolutionLayer() {
            if (!this->ShareAttr) {
                for (Data<T> *d : this->weights) {
                    delete d;
                }
            }
            delete[] col;
        }

        string ToJsonParams();
        json11::Json ToJson();

        void SplitToDilationAndConvolution(json11::Json &deconvDilation, json11::Json &convolution);

        void SaveMiniNet(std::ofstream &outputFile);

        bool groupMode();

        void Forward();

        void ForwardTF();

        void update_config(bool ignore = false);

        void Prepare();
        void Reshape();

#ifdef USE_TFACC
        void fillBiasData(uint8_t* biasData);
        void TFACCInit();
        void TFACCInit(int TFACCId);
        void refreshTFACCBias();
        void TFACCRelease();
#endif
        void loadWeightConfig();

        int GetWeightChannels();

        void ResetInt8Config();

        long long GetOperationTimes();

        void cpuGemm(T *input, T *weight, T *output);

        void cpuBias(T *output, T *bias);

        void cpuRelu(T *output, int cnt);

        void TFACCGemm(T *input, T *weight, T *output, int inputOffset, int outputOffset);

        void cpuMatrixMultiply(int n, int m, int k, T *a, T *b, T *c, int biasStart);

        void col2im(T *col, T *image);

        void im2col(T *image, T *col,T zero, bool rev, int width,int height,int channels,int output_W,int output_H,int pad_W,int pad_H,int kernel_W,int kernel_H,int stride_W,int stride_H,int dilation_W,int dilation_H);

        void diation_im(T *img,T *dst);

        string GetParams();

        void createBias();

        long long GetWeightDataAmount();

        void setFixZero(int fixZero);

        void checkOutput();
        void setOriginalInputScale(float scale);
        float getOriginalInputScale();

        int GetKernelH();
        int GetKernelW();
        int GetPadH();
        int GetPadW();
        int GetStride();
        int GetDilation();
        int GetGroup();
        int GetOutPadH();
        int GetOutPadW();

        bool IfReLU();
        bool IfHasBias();
        bool IfInt32Bias();

#ifdef USE_TFACC40T

        long long weightHash = 0;

        void TFACCInit();

        void TFACCReleaseDDR();

        void TFACCMark();

        void TFACCRelease();

        bool TFACCSupport();

        void SetDeviceId(int deviceId);

        void FillBlasopLists(int maus);

        tfacc40t::ConvolutionLayer convolutionLayer;
#endif
    private:
        int pooling;
        //only support max pooling
        //pooling = 2: {kernelSize = 2, stride = 2, pad = 0}
        //pooling = 3: {kernelSize = 3, stride = 2, pad = 0}

        int outputChannels;
        int kernel_h;
        int kernel_w;
        int pad_h;
        int pad_w;
        int stride;
        int dilation;
        int group, tfaccGroup;
        bool hasBias; //if hasbia, bias = weights[1] (weight = weights[0])
        bool int32Bias = false;
        bool relu;
        float negative_slope;
        bool isHWIm2Col;
        int fixZero = 0; //fill inputZero to col fixZero.
        int outpadW,outpadH;
        std::tuple<unsigned int, unsigned int, unsigned int> TFBLASTuple; //for TFACC
        T *col = nullptr;
        //T *biasData = nullptr;
        //T *newWeight = nullptr;
        set <long long> hashs;
        short* realWeight = nullptr;

        vector <QuantizationConfig*> weightConfigs;
        vector <QuantizationConfig*> biasConfigs;
        float originalInputScale = 0.0f; //存储原始的inputScale，如果inputScale发生变化，且使用int32bias，那么bias的值相应也要变化
    };

};


#endif //PROJECT_DeconvolutionLayer_H
