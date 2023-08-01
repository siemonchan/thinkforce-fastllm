//
// Created by yuyang.huang on 17-11-14.
//

#ifndef TFDL_NORMALIZELAYER_H
#define TFDL_NORMALIZELAYER_H

#include "../Layer.h"

namespace tfdl {
    template<typename T>
    class NormalizeLayer : public Layer<T> {
    public:
        NormalizeLayer(const json11::Json &config, string dataType) : Layer<T>(config, dataType) {

            this->weights.push_back(new Data<T>);
        }
        ~NormalizeLayer() {
            if (!this->ShareAttr) {
                for (Data<T> *d : this->weights) {
                    delete d;
                }
            }
        }

        void Forward();
        void ForwardTF();

        json11::Json ToJson();
    };


}

#endif //CAFFE_NORMALIZELAYER_H
