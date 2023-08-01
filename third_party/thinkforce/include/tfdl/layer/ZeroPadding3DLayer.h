//
// Created by huangyuyang on 4/14/22.
//

#ifndef PROJECT_ZEROPADDING3DLAYER_H
#define PROJECT_ZEROPADDING3DLAYER_H

#include "../Layer.h"

namespace tfdl{
    template <typename T>
    class ZeroPadding3DLayer : public Layer<T> {
    public:
        ZeroPadding3DLayer(const json11::Json &config, string dataType) : Layer<T>(config, dataType) {
            auto &param = config["param"];
            pads.clear();
            for (auto &it : param["pads"].array_items()) {
                pads.push_back(it.int_value());
            }
        }

        ~ZeroPadding3DLayer() {

        }

        json11::Json ToJson();
        void Forward();
        void ForwardTF();
        void Reshape();
    private:
        vector <int> pads;
    };
}

#endif //PROJECT_ZEROPADDING3DLAYER_H
