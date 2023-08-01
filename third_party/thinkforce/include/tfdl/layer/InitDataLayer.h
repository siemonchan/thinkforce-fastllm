//
// Created by huangyuyang on 1/10/23.
//

#ifndef TFDL_INITDATA_H
#define TFDL_INITDATA_H

#include "../Layer.h"

namespace tfdl {
    template<typename T>
    class InitDataLayer : public Layer<T> {
    public:
        InitDataLayer(const json11::Json &config, string dataType) : Layer<T>(config, dataType) {
            auto param = config["param"];
            dType = !param["dataType"].is_null() ? param["dataType"].int_value() : 0;
            dims.clear();
            for (auto &it : param["dims"].array_items()) {
                dims.push_back(it.int_value());
            }
            ints.clear();
            floats.clear();
            if (!param["ints"].is_null()) {
                for (auto &it : param["ints"].array_items()) {
                    ints.push_back(it.int_value());
                }
            }
            if (!param["floats"].is_null()) {
                for (auto &it : param["floats"].array_items()) {
                    floats.push_back(it.number_value());
                }
            }
        }

        ~InitDataLayer() {
        }

        void Forward();

        void ForwardTF();

        void Reshape();

        json11::Json ToJson();
    private:
        vector <int> dims;
        int dType; // 参数中的数据类型，0为float，1为ints
        vector <float> floats;
        vector <int> ints;
    };
}

#endif //TFDL_INITDATA_H
