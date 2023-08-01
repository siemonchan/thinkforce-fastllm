//
// Created by yuyang.huang on 17-11-9.
//

#ifndef TFDL_POOLINGLAYER_H
#define TFDL_POOLINGLAYER_H

#include "../Layer.h"
#ifdef ARM
#include <arm_neon.h>
#endif
#ifdef USE_TFACC
#include <tfblas_api.hpp>
#include <common.hpp>
#endif

namespace tfdl {
    template<typename T>
    class PoolingLayer : public Layer<T> {
    public:
        PoolingLayer (const json11::Json &config, string dataType) : Layer<T>(config, dataType) {
            auto &param = config["param"];
            kernelSize = param["kernelSize"].int_value();
            if ((!param["kernel_h"].is_null()) && (!param["kernel_w"].is_null())) {
                kernel_w = param["kernel_w"].int_value();
                kernel_h = param["kernel_h"].int_value();
                //if(kernel_h != kernel_w)throw_exception("the kernel_w need be equal to kernel_h");
                //kernelSize = kernel_w;
            } else {
                kernel_w = kernelSize;
                kernel_h = kernelSize;
            }

            pad = param["pad"].int_value();
            if ((!param["pad_h"].is_null()) && (!param["pad_w"].is_null())) {
                pad_w = param["pad_w"].int_value();
                pad_h = param["pad_h"].int_value();
            } else {
                pad_w = pad_h = pad;
            }
            stride = param["stride"].int_value();
            if (!stride) {
                stride = 1;
            }
            if ((!param["stride_h"].is_null()) && (!param["stride_w"].is_null())) {
                stride_w = param["stride_w"].int_value();
                stride_h = param["stride_h"].int_value();
                if(stride_h == stride_w)stride = stride_w;
            } else {
                stride_w = stride_h = stride;
            }

            poolingType = param["poolingType"].string_value();
            ceil_mode = !(param["floor_mode"].bool_value());
            reverseBack = param["reverseBack"].int_value();
            tf_mode = param["tf_mode"].bool_value();
            global_pooling = (param["global_pooling"].is_null() ? false : param["global_pooling"].bool_value());
        }

        ~PoolingLayer() {

        }

        string ToJsonParams();
        json11::Json ToJson();
        void Forward();
        void ForwardTF();
        void Reshape();
        bool get_global_pooling();
        int getKernelH();
        int getKernelW();
        int getStride();

        static void cpuPooling(T *input, T *output, int inputSpatial, int outputSpatial,
                               int channels, int height, int width,
                               int kernel_h,int kernel_w, int stride_h,int stride_w, int pad_h, int pad_w,
                               string layerDataType, string poolingType,
                               QuantizationConfig *inputConfig, QuantizationConfig *outputConfig, bool ceil_mode = true ,bool global_pooling=false,
                               bool tf_mode = false);
        static void tfaccPooling(T *input, T *output, int inputAddressBase, int outputAddressBase,
                                 int channels, int height, int width,
                                 int kernel_h,int kernel_w, int stride, int pad,
                                 string layerDataType, string poolingType, bool isGlobal,
                                 QuantizationConfig *inputConfig, QuantizationConfig *outputConfig, bool ceil_mode = true ,bool global_pooling=false,bool tf_mode = false,bool Ispost = false, PostCmds* postCmds1= nullptr);
        bool IsHardware();
        void SaveMiniNet(std::ofstream &outputFile);
#ifdef USE_TFACC
        void TFACCInit();
#endif
#ifdef USE_TFACC40T
        void TFACCInit();
        void TFACCMark();
        bool TFACCSupport();
#endif
    private:
        int kernelSize;
        int kernel_h;
        int kernel_w;
        int pad;
        int pad_h;
        int pad_w;

        int stride;
        int stride_h;
        int stride_w;
        string poolingType;
        bool ceil_mode;
        bool tf_mode;
        bool global_pooling;
        bool isHardware = true;
        bool reverseBack = false;
    };


}


#endif //CAFFE_POOLINGLAYER_H
