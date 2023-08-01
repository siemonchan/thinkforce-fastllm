//
// Created by siemon on 10/27/20.
//

#ifndef TFDL_REPLICATIONPADDINGLAYER_H
#define TFDL_REPLICATIONPADDINGLAYER_H

#include "Layer.h"

namespace tfdl {
    template<typename T>
    class ReplicationPaddingLayer : public Layer<T> {
    public:
        ReplicationPaddingLayer(const json11::Json &config, string dataType) : Layer<T>(config, dataType) {
            auto &param = config["param"];
            if (param["pad"].is_null()) {
                pad_t = param["pad_t"].int_value();
                pad_l = param["pad_l"].int_value();
                pad_b = param["pad_b"].int_value();
                pad_r = param["pad_r"].int_value();
                pad = -1;
            } else {
                pad = param["pad"].int_value();
                pad_t = pad_l = pad_b = pad_r = pad;
            }
        }

        void Forward();

        void ForwardTF();

        void Prepare();

        void Reshape();

        string ToJsonParams();

        json11::Json ToJson();

    private:
        void replication_padding_one_spatial_gen_pos(int *pos, int input_h, int input_w, int output_h, int output_w);

        void replication_padding_one_spatial_apply_pos(uint8_t *input, uint8_t *output, int input_h, int input_w, int output_h, int output_w);

        void replication_padding_one_spatial(float *input, float *output, int input_h, int input_w, int output_h, int output_w);

        int pad = 0;
        int pad_t = 0, pad_l = 0, pad_b = 0, pad_r = 0;
    };
}

#endif //TFDL_REPLICATIONPADDINGLAYER_H
