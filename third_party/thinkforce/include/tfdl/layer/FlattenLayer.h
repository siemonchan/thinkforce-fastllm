//
// Created by yuyang.huang on 17-11-14.
//

#ifndef TFDL_FLATTENLAYER_H
#define TFDL_FLATTENLAYER_H

#include "../Layer.h"

namespace tfdl {
    template<typename T>
    class FlattenLayer : public Layer<T> {
    public:
        FlattenLayer(const json11::Json &config, string dataType) : Layer<T>(config, dataType) {
            auto &param = config["param"];
            startAxis = param["startAxis"].int_value();
            endAxis = param["endAxis"].int_value();
        }
        ~FlattenLayer() {

        }

        string ToJsonParams();
        json11::Json ToJson();
        void SaveMiniNet(std::ofstream &outputFile);
        void Forward();
        void ForwardTF();
        void Reshape();
    private:
        int startAxis;
        int endAxis;
    };


}


#endif //CAFFE_FLATTENLAYER_H
