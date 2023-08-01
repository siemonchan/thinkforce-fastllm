//
// Created by huangyuyang on 2/21/23.
//

#ifndef TFDL_DECONVDILATIONLAYER_H
#define TFDL_DECONVDILATIONLAYER_H

#include <Layer.h>
#include <common.h>

namespace tfdl{
    template<typename T>
    class DeconvDilationLayer: public Layer<T>{

    public:
        DeconvDilationLayer(const json11::Json &config, string dataType):Layer<T>(config,dataType) {
            auto &param = config["param"];
            int kernelSize = param["kernelSize"].int_value();
            kernel_h = param["kernel_h"].int_value();
            kernel_w = param["kernel_w"].int_value();
            if (kernel_h == 0 && kernel_w == 0) {
                kernel_h = kernelSize;
                kernel_w = kernelSize;
            }
            if (param["output_padding"].is_null()) {
                outpadH = 0;
                outpadW = 0;
            } else {
                assert(param["output_padding"].array_items().size() == 2);
                outpadH = param["output_padding"].array_items()[0].int_value();
                outpadW = param["output_padding"].array_items()[1].int_value();
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
        int stride;
        int dilation;
        int outpadW,outpadH;
    };
}

#endif //TFDL_DECONVDILATIONLAYER_H
