//
// Created by huangyuyang on 3/24/21.
//

#ifndef PROJECT_ARGMAXMINLAYER_H
#define PROJECT_ARGMAXMINLAYER_H

#include "Layer.h"

namespace tfdl {
    template<typename T>
    class ArgMaxMinLayer : public Layer<T> {
    public:
        ArgMaxMinLayer(const json11::Json &config, string dataType) : Layer<T>(config, dataType) {
            auto param = config["param"];
            type = param["type"].string_value();
            axis = param["axis"].int_value();
            keep_dims = bool(param["keepdims"].int_value());
            select_last_index = param["select_last_index"].is_null() ? 0 : param["select_last_index"].int_value();
        }

        void Forward();

        void ForwardTF();

        void Reshape();

        json11::Json ToJson();

    private:
        string type;
        bool keep_dims = true;
        int axis = 0;
        int select_last_index = false;
    };
}

#endif //PROJECT_ARGMAXMINLAYER_H
