//
// Created by feng.jianhao on 18-7-25.
//

#ifndef PROJECT_UPSAMPLELAYER_H
#define PROJECT_UPSAMPLELAYER_H

#include "Layer.h"

namespace tfdl{
    enum UpsampleOp {
        DUP = 0,
        BILINEAR = 1,
        NEAREST = 2
    };

    template<typename T>
    class UpsampleLayer : public Layer<T>{
    public :
        UpsampleLayer(const json11::Json &config,string type):Layer<T>(config,type){
            auto param=config["param"];
            stride=(param["stride"].is_null())?(0):(param["stride"].int_value());
            std::string operate = param["Operation"].string_value();
            if(operate=="DUP") {
                op = UpsampleOp::DUP;
            }
            if(operate=="BILINEAR" || operate == "linear" || operate == "LINEAR"){
                op = UpsampleOp::BILINEAR;
            }
            if(operate=="NEAREST") {
                op = UpsampleOp::NEAREST;
            }
            outwidth = param["output_width"].int_value();
            outheight = param["output_height"].int_value();
            height_scale = static_cast<float>(param["height_scale"].number_value());
            width_scale = static_cast<float>(param["width_scale"].number_value());
            align_corners = param["align_corners"].int_value();
            pytorch_half_pixel = param["pytorch_half_pixel"].int_value();
            preserve_aspect_ratio = param["preserve_aspect_ratio"].int_value();
        };

        ~UpsampleLayer() {
            if (temp != nullptr) {
                delete[] temp;
            }
        }
        void Forward();
        void ForwardTF();
        void Reshape();
        void Prepare();

        void Dup();
        void BiLinear();
        void Nearest();

        string ToJsonParams();
        json11::Json ToJson();
        void SaveMiniNet(std::ofstream &outputFile);
    private:
        int stride=0;
        UpsampleOp op = UpsampleOp::DUP;
        int outwidth;
        int outheight;
        int align_corners = 0;
        int pytorch_half_pixel = 0;
        int preserve_aspect_ratio= 0;
        uint16_t *temp = nullptr;

        float height_scale;
        float width_scale;

        string OpString() {
            if (this->op == UpsampleOp::DUP) {
                return "DUP";
            } else if (this->op == UpsampleOp::BILINEAR) {
                return "BILINEAR";
            } else if (this->op == UpsampleOp::NEAREST) {
                return "NEAREST";
            }
            return "DUP";
        }
    };

}

#endif //PROJECT_UPSAMPLELAYER_H
