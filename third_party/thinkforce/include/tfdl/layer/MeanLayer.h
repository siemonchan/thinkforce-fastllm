//
// Created by siemon on 9/19/19.
//

#ifndef PROJECT_MEANLAYER_H
#define PROJECT_MEANLAYER_H

#include "Layer.h"

namespace tfdl {
    template <typename T>
    class MeanLayer : public Layer<T> {
    public:
        MeanLayer(const json11::Json &config, const string &dataType) : Layer<T>(config, dataType) {
            auto param = config["param"];
            for(auto &item : param["axis"].array_items()) {
                axis.push_back(item.int_value());
            }
        }

        void Forward();
        void ForwardTF();
        void Reshape();
        void Prepare();
        string axisString();
        string ToJsonParams();
        json11::Json ToJson();

    private:
        vector<int> axis;
    };
}

#endif //PROJECT_MEANLAYER_H
