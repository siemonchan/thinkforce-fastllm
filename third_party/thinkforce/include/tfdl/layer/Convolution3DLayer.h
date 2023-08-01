//
// Created by huangyuyang on 4/14/22.
//

#ifndef PROJECT_CONVOLUTION3DLAYER_H
#define PROJECT_CONVOLUTION3DLAYER_H

#include "../Layer.h"

namespace tfdl{
    template <typename T>
    class Convolution3DLayer : public Layer<T> {
    public:
        Convolution3DLayer(const json11::Json &config, string dataType) : Layer<T>(config, dataType) {
            auto &param = config["param"];
            for (auto var :param["strides"].array_items()) {
                strides.push_back(var.int_value());
            }
            for (auto var :param["pads"].array_items()) {
                pads.push_back(var.int_value());
            }
            for (auto var :param["kernels"].array_items()) {
                kernels.push_back(var.int_value());
            }
            outputChannels = param["outputChannels"].int_value();
            dilation = param["dilation"].int_value();
            group = param["group"].int_value();
            hasBias = param["hasBias"].int_value();
            relu = param["relu"].int_value();
            this->weights.push_back(new Data<T>());

            if (hasBias) {
                this->weights.push_back(new Data<T>);
            }
        }

        ~Convolution3DLayer() {

        }

        json11::Json ToJson();
        void Forward();
        void ForwardTF();
        void Reshape();

        void Im2col(T *image, T *col);

        bool SplitToConvolution(json11::Json &reshape0Config,
                                json11::Json &convolutionConfig,
                                json11::Json &reshape1Config,
                                vector <float> &weight, vector <float> &bias);
    private:
        vector <int> pads;
        vector <int> strides;
        vector <int> kernels;
        int outputChannels;
        int dilation;
        int group;
        int hasBias;
        int relu;
        float negative_slope;
    };
}

#endif //PROJECT_CONVOLUTION3DLAYER_H
