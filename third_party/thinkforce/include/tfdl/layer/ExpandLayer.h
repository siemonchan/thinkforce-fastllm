//
// Created by root on 3/10/23.
//

#ifndef TFDL_EXPANDLAYER_H
#define TFDL_EXPANDLAYER_H

#include "Layer.h"

namespace tfdl {
    template <typename T>
    class ExpandLayer : public Layer<T> {
    public:
        ExpandLayer(const json11::Json &config, string dataType) : Layer<T>(config, dataType) {
            auto &param = config["param"];
            if (!param["dims"].is_null()) {
                this->dims.clear();
                for (auto &i : param["dims"].array_items()) {
                    this->dims.push_back(i.int_value());
                }
            }
            oldIndex.clear();
        }

        void Forward();

        void ForwardTF();

        void Reshape();

    private:
        vector<int> dims;

        vector <int> oldIndex;
    };
}

#endif //TFDL_EXPANDLAYER_H
