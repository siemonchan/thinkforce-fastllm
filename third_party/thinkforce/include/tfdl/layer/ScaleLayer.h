//
// Created by osboxes on 13/12/17.
//

#ifndef TFDL_SCALELAYER_H
#define TFDL_SCALELAYER_H
#include "../Layer.h"
#include "layer/BatchNormLayer.h"

namespace tfdl {
    template<typename T>
    class ScaleLayer : public Layer<T> {
    public:
        ScaleLayer(const json11::Json &config, string dataType) : Layer<T>(config, dataType) {
            auto &param = config["param"];
            _bias_term = param["bias_term"].int_value();
            this->weights.push_back(new Data<T>);
            if(_bias_term) this->weights.push_back(new Data<T>);
            fixZero = param["fixZero"].is_null() ? 0 : param["fixZero"].int_value();
            relu = param["relu"].bool_value();
            axis = param["axis"].is_null() ? 1 : param["axis"].int_value();
        }

        ~ScaleLayer() {
            if (!this->ShareAttr) {
                for (Data<T> *d : this->weights) {
                    delete d;
                }
            }
        }

        string ToJsonParams();
        json11::Json ToJson();
        void Forward();
        void ForwardTF();
        void cpuFloatScale(float *input, float *output);
        void SetFixZero(int fixZero);

        void Prepare();
        bool MergePreBatchNorm(BatchNormLayer<T> *batchNormLayer);

        void EnableReLU();

        int _outer_dim, _scale_dim, _inner_dim;
        int _bias_term;
        int fixZero;
        int axis;
        bool relu = false;
#ifdef USE_TFACC
#ifdef _USE_NEON
        int32_t* newscale = nullptr;
        int32_t* newbias = nullptr;
        uint8x8_t zeros;
#endif
#endif
    };
}

#endif //CAFFE_SCALELAYER_H
