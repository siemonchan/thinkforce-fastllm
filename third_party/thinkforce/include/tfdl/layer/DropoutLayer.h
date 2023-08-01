//
// Created by yuyang.huang on 17-11-9.
//

#ifndef TFDL_DROPOUTLAYER_H
#define TFDL_DROPOUTLAYER_H

#include "../Layer.h"

namespace tfdl {
    template<typename T>
    class DropoutLayer : public Layer<T> {
    public:
        DropoutLayer(const json11::Json &config, string dataType) : Layer<T>(config, dataType) {

        }
        ~DropoutLayer() {

        }

        void Forward();
        void ForwardTF();
        json11::Json ToJson();
        void SaveMiniNet(std::ofstream &outputFile);
    };


}


#endif //CAFFE_DROPOUTLAYER_H
