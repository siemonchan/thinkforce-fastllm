//
// Created by root on 3/17/23.
//

#ifndef TFDL_GATHERINDICESLAYER_H
#define TFDL_GATHERINDICESLAYER_H

#include "Layer.h"

namespace tfdl {
    template<typename T>
    class GatherIndicesLayer : public Layer<T> {
    public:
        GatherIndicesLayer(const json11::Json &config, string dataType) : Layer<T>(config, dataType) {
            auto param = config["param"];
            this->weightDims.clear();
            for (int i = 0; i < param["weight_dims"].array_items().size(); i++) {
                this->weightDims.push_back(param["weight_dims"].array_items()[i].int_value());
            }
            this->axis = param["axis"].int_value();
        }

        void Forward();

        void ForwardTF();

        void Reshape();

        json11::Json ToJson();

    private:
        int axis;
        vector<int> weightDims;
    };
}

#endif //TFDL_GATHERINDICESLAYER_H