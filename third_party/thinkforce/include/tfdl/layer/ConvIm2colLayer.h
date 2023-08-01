//
// Created by huangyuyang on 3/15/23.
//

#ifndef TFDL_CONVIM2COLLAYER_H
#define TFDL_CONVIM2COLLAYER_H

#include <Layer.h>
#include <common.h>

namespace tfdl{
    template<typename T>
    class ConvIm2colLayer: public Layer<T>{

    public:
        ConvIm2colLayer(const json11::Json &config, string dataType):Layer<T>(config,dataType) {
            auto &param = config["param"];
            int kernelSize = param["kernelSize"].int_value();
            kernel_h = param["kernel_h"].int_value();
            kernel_w = param["kernel_w"].int_value();
            if (kernel_h == 0 && kernel_w == 0) {
                kernel_h = kernelSize;
                kernel_w = kernelSize;
            }

            int pad = param["pad"].int_value();
            pad_h = param["pad_h"].int_value();
            pad_w = param["pad_w"].int_value();
            if (pad_h == 0 && pad_w == 0) {
                pad_h = pad;
                pad_w = pad;
            }
            pad_u = param["pad_u"].int_value();
            pad_d = param["pad_d"].int_value();
            pad_l = param["pad_l"].int_value();
            pad_r = param["pad_r"].int_value();
            if (pad_u == 0 && pad_d == 0 && pad_l == 0 && pad_r == 0) {
                pad_u = pad_h;
                pad_d = pad_h;
                pad_l = pad_w;
                pad_r = pad_w;
            }

            int stride = ((param["stride"].is_null()) ? 1 : param["stride"].int_value());
            stride_h = ((param["stride_h"].is_null()) ? stride : param["stride_h"].int_value());
            stride_w = ((param["stride_w"].is_null()) ? stride : param["stride_w"].int_value());
            dilation = ((param["dilation"].is_null()) ? 1 : param["dilation"].int_value());
        }

        void Forward();

        void ForwardTF();

        void Reshape();

        json11::Json ToJson();
    private:
        int kernel_h;
        int kernel_w;
        int pad_h;
        int pad_w;
        int pad_u, pad_d, pad_l, pad_r;
        int stride_h, stride_w;
        int dilation;
    };
}

#endif //TFDL_CONVIM2COLLAYER_H
