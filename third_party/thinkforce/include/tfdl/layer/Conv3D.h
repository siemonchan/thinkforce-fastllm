//
// Created by test on 18-7-11.
//

#ifndef PROJECT_CONV3D_H
#define PROJECT_CONV3D_H

#include "../Layer.h"
#include <common.h>
#include <layer/ConvolutionLayer.h>
#include <layer/ConcatLayer.h>
#include <layer/EltwiseLayer.h>
#include "../Data.h"
#ifdef ARM
#include <arm_neon.h>
#endif
#ifdef USE_TFACC
#include <tfblas_api.hpp>
#endif

#ifdef WITH_BLAS
#include <cblas.h>
#endif

namespace tfdl {
    template<typename T>
    class Conv3D : public Layer<T> {
    public:
        Conv3D(const json11::Json &config, std::string dataType) : Layer<T>(config, dataType) {
            auto param=config["param"];
            for(auto var :param["stride"].array_items()){
                stride.push_back(var.int_value());
            }
            for(auto var :param["pad"].array_items()){
                pad.push_back(var.int_value());
            }
            for(auto var :param["kernel"].array_items()){
                kernel.push_back(var.int_value());
            }
            outputChannels=param["outputChannels"].int_value();
            dilation=param["dilation"].int_value();
            group=param["group"].int_value();
            hasBias=param["hasBias"].int_value();
            relu=param["relu"].int_value();

            /*
             *  build new conv config json
             */
            json11::Json conv_json=json11::Json::object{
                    {"layerName",this->layerName},
                    {"layerType","Convolution"},
                    {"param",json11::Json::object{
                    {"kernel_w",       kernel[0]},
                    {"kernel_h",       kernel[1]},
                    {"pad_w",          pad[0]},
                    {"pad_h",          pad[1]},
                    {"group",          group},
                    {"dilation",       dilation},
                    {"hasBias",        0},
                    {"outputChannels", outputChannels},
                    {"relu",           0}
            }
                                                        }
            };
            this->Conv=new ConvolutionLayer<T>(conv_json,dataType);
            this->weights.push_back(new Data<T>());

            if (hasBias) {
                this->weights.push_back(new Data<T>);
            }
            /*
            std::map<string,string> Sum_config;
            Sum_config["Operation"]="SUM";
            json11::Json Sum_json=json11::Json::object{
                    {"param",json11::Json::object{
                            {"Operation","SUM"}

                    }
                    }
            };
            this->Sum=new EltwiseLayer<T>(Sum_json,dataType);
*/
        }
        ~Conv3D(){
            if (!this->ShareAttr) {
                for (Data<T> *d : this->weights) {
                    delete d;
                }
            }
        }
        void Reshape();
        void Forward();
        void ForwardTF();
#ifdef USE_TFACC
        void TFACCInit();
#endif
    private:
        /*
         * the sequence is w,h,d
         */
        std::vector<int> stride;
        std::vector<int> pad;
        std::vector<int> kernel;
        int outputDepth;
        int outputChannels;
        int dilation;
        int group;
        bool hasBias; //if hasbia, bias = weights[1] (weight = weights[0])
        bool relu;
        float negative_slope=0.0;
        T* pad_data= nullptr;
        /*
         * this code will split 3D input into many 2D input based depth of stride
         * and will split the 3D Conv into (2D Conv + Eltwise) based on depth of kernel
         *
         */
        ConvolutionLayer<T>*  Conv;
        std::vector<Data<T>*>split_input;
        //std::vector<T*>split_output;


    };
}

#endif //PROJECT_CONV3D_H
