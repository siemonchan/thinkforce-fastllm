//
// Created by yuyang.huang on 17-11-14.
//

#ifndef TFDL_SPLITLAYER_H
#define TFDL_SPLITLAYER_H

#include "../Layer.h"

namespace tfdl {
    template<typename T>
    class SplitLayer : public Layer<T> {
    public:
        SplitLayer(const json11::Json &config, string dataType) : Layer<T>(config, dataType) {

        }
        ~SplitLayer() {

        }

        void Forward();
        void ForwardTF();
        void Reshape();
    };


}


#endif //CAFFE_SPLITLAYER_H
