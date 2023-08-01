//
// Created by huangyuyang on 4/14/22.
//

#ifndef PROJECT_POOLING3DLAYER_H
#define PROJECT_POOLING3DLAYER_H

#include "../Layer.h"

namespace tfdl{
    template <typename T>
    class Pooling3DLayer : public Layer<T> {
    public:
        Pooling3DLayer(const json11::Json &config, string dataType) : Layer<T>(config, dataType) {
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
            poolingType = param["poolingType"].string_value();
            ceilMode = !(param["floor_mode"].bool_value());
        }

        ~Pooling3DLayer() {

        }

        json11::Json ToJson();
        void Forward();
        void ForwardTF();
        void Reshape();
    private:
        vector <int> kernels;
        vector <int> strides;
        vector <int> pads;

        bool ceilMode;
        string poolingType;
    };
}


#endif //PROJECT_POOLING3DLAYER_H
