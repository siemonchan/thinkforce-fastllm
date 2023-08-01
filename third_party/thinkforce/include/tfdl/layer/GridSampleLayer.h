//
// Created by siemon on 8/20/21.
//

#ifndef TFDL_GRIDSAMPLELAYER_H
#define TFDL_GRIDSAMPLELAYER_H

#include "Layer.h"
#include "transforms.hpp"

namespace tfdl {
    template<typename T>
    class GridSampleLayer : public Layer<T> {
    public:
        GridSampleLayer(const json11::Json &config, string dataType) : Layer<T>(config, dataType) {
            auto param = config["param"];
            interp_mode = param["interp_mode"].is_null() ? interp_mode: param["interp_mode"].string_value();
            padding_mode = param["padding_mode"].is_null() ? padding_mode : param["padding_mode"].string_value();
            align_corners = param["align_corners"].is_null() ? align_corners : (bool) param["align_corners"].int_value();
        }

        void Forward();

        void ForwardTF();

        void Prepare();

    private:
        string interp_mode = "BILINEAR";
        string padding_mode = "ZERO";
        bool align_corners = true;

        Data<float> *grid_h = nullptr;
        Data<float> *grid_w = nullptr;
    };
}

#endif //TFDL_GRIDSAMPLELAYER_H
