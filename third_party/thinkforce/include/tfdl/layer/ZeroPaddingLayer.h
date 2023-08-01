//
// Created by siemon on 1/28/19.
//

#ifndef PROJECT_ZEROPADDINGLAYER_H
#define PROJECT_ZEROPADDINGLAYER_H

#include "../Layer.h"

namespace tfdl{
    template <typename T>
    class ZeroPaddingLayer : public Layer<T> {
    public:
        ZeroPaddingLayer(const json11::Json &config, string dataType) : Layer<T>(config, dataType) {
            auto &param = config["param"];
            if(param["pad"].is_null()){
                pad_t = param["pad_t"].int_value();
                pad_l = param["pad_l"].int_value();
                pad_b = param["pad_b"].int_value();
                pad_r = param["pad_r"].int_value();
                pad_cb = param["pad_cb"].int_value();
                pad_ca = param["pad_ca"].int_value();
                pad = -1;
            } else {
                pad = param["pad"].int_value();
            }
        }

        ZeroPaddingLayer(string layerName, int pad) : Layer<T>(layerName, "ZeroPadding"){
            this->pad = pad;
        }

        ~ZeroPaddingLayer() {

        }

        string ToJsonParams();
        json11::Json ToJson();
        void Forward();
        void ForwardTF();
        void Reshape();

        void ZeroPadding(uint8_t *src, uint8_t *dist, int input_height, int input_width, int output_height, int output_width);
        int GetPad();
        vector<int> GetPads();

    private:
        int pad = 0;
        int pad_t = 0, pad_l = 0, pad_b = 0, pad_r = 0, pad_cb = 0, pad_ca = 0;
    };
}

#endif //PROJECT_ZEROPADDINGLAYER_H
