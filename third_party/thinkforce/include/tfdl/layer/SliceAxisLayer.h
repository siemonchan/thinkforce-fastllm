//
// Created by huangyuyang on 11/21/19.
//

#ifndef PROJECT_SLICEAXISLAYER_H
#define PROJECT_SLICEAXISLAYER_H

#include "BasePermuteLayer.h"

namespace tfdl {
    template<typename T>
    class SliceAxisLayer : public BasePermuteLayer<T> {
    public:
        SliceAxisLayer(const json11::Json &config, string dataType) : BasePermuteLayer<T>(config, dataType) {
            auto &param = config["param"];

            if (!param["axis"].is_null()) {
                // deprecated
                axes.push_back(param["axis"].int_value());
                starts.push_back(param["begin"].int_value());
                ends.push_back(param["end"].int_value());
            } else {
                for (auto&& i : param["axes"].array_items()) {
                    axes.push_back(i.int_value());
                }
                for (auto&& i : param["starts"].array_items()) {
                    starts.push_back(i.int_value());
                }
                for (auto&& i : param["ends"].array_items()) {
                    ends.push_back(i.int_value());
                }
                for (auto&& i : param["steps"].array_items()) {
                    steps.push_back(i.int_value());
                }
            }
        }

        ~SliceAxisLayer() {
        }

        void Forward();

        json11::Json ToJson();
        void Reshape();
    private:
        vector<int> axes, starts, ends, steps;
    };
}

#endif //PROJECT_SLICEAXISLAYER_H
