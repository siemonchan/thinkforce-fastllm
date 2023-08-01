//
// Created by huangyuyang on 1/4/23.
//

#ifndef TFDL_GATHER_H
#define TFDL_GATHER_H

#include "../Layer.h"

namespace tfdl {
    template<typename T>
    class GatherLayer : public Layer<T> {
    public:
        GatherLayer(const json11::Json &config, string dataType) : Layer<T>(config, dataType) {
            auto param = config["param"];
            axis = !param["axis"].is_null() ? param["axis"].int_value() : 0;
            indices.clear();
            for (auto &it : param["indices"].array_items()) {
                indices.push_back(it.int_value());
            }
        }

        ~GatherLayer() {
        }

        void Forward();

        void ForwardTF();

        void Reshape();

        json11::Json ToJson();

    private:
        int axis;
        vector <int> indices;
    };
}

#endif //TFDL_GATHER_H
