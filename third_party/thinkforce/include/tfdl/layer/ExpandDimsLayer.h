//
// Created by huangyuyang on 11/24/19.
//

#ifndef PROJECT_EXPANDDIMSLAYER_H
#define PROJECT_EXPANDDIMSLAYER_H

#include "../Layer.h"

namespace tfdl {
    template<typename T>
    class ExpandDimsLayer : public Layer<T> {
    public:
        ExpandDimsLayer(const json11::Json &config, string dataType) : Layer<T>(config, dataType) {
            auto &param = config["param"];
            axis = param["axis"].int_value();
            axisArray.clear();
            for (const auto &id : param["axisArray"].array_items()) {
                axisArray.push_back(id.int_value());
            }
        }

        ~ExpandDimsLayer() {
        };

        void Forward();

        void ForwardTF();

        void Reshape();

        json11::Json ToJson();
    private:
        int axis; // 仅当axisArray.size() == 0时生效
        vector <int> axisArray; // 如果存在axisArray，那么以array为准
    };
}

#endif //PROJECT_EXPANDDIMSLAYER_H
