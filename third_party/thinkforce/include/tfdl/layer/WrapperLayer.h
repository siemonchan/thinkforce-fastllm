//
// Created by siemon on 4/22/20.
//

#ifndef PROJECT_WRAPPERLAYER_H
#define PROJECT_WRAPPERLAYER_H

#include "Interpreter/Attrs.h"
#include "WrapperFunctions.h"
#include "Layer.h"

namespace tfdl {
    using namespace attr;
    /*
     *  Wrapper Layer wraps simple function to a layer object with function pointer
     *  in purpose of iteration rapidly
     */
    template<typename T>
    class WrapperLayer : public Layer<T> {
    public:
        // Function that tells wrapper how to reshape outputs due to it`s inputs and parameters
        typedef void (*WrapperReshape)(vector<Data<T> *> inputs, vector<Data<T> *> outputs, vector<Data<T> *> weights,
                                       INT8Config *int8Config, ParamType param);

        // Function that defines computational process of this wrapper
        typedef void (*WrapperForward)(vector<Data<T> *> inputs, vector<Data<T> *> outputs, vector<Data<T> *> weights,
                                       INT8Config *int8Config, ParamType param);

        WrapperLayer(const json11::Json &config, const string &dataType) : Layer<T>(config, dataType) {
            jsonParam = config["param"];

            // according to the type of layerType, set corresponding parameters and functions
            if (this->layerType == "Tile") {
                vector<int> multiples;
                for (auto &i : jsonParam["multiples"].array_items()) {
                    multiples.push_back(i.int_value());
                }
                this->param = reinterpret_cast<void *>(new TileParam(multiples));
                this->wrapperReshape = wrapper::TileReshape;
                this->wrapperForward = wrapper::TileForward;
            }
            if (this->layerType == "Sqrt") {
                this->wrapperReshape = nullptr;
                this->wrapperForward = wrapper::SqrtForward;
            }
            if (this->layerType == "Rsqrt") {
                this->wrapperReshape = nullptr;
                this->wrapperForward = wrapper::RsqrtForward;
            }
            if (this->layerType == "Less") {
                this->wrapperReshape = nullptr;
                this->wrapperForward = wrapper::LessForward;
            }
            if (this->layerType == "Shape") {
                this->wrapperReshape = wrapper::ShapeReshape;
                this->wrapperForward = nullptr;
            }
            // TODO: implement more wrapper function
        }

        WrapperLayer(const json11::Json &config, const string &dataType, WrapperReshape reshape, WrapperForward forward,
                     ParamType param) : Layer<T>(config, dataType) {
            this->jsonParam = config["param"];
            this->param = param;
            this->wrapperReshape = reshape;
            this->wrapperForward = forward;
        }

        void Forward();

        void ForwardTF();

        void Reshape();

        json11::Json ToJson();

        string ToJsonParams();

        void SetParam(ParamType _param);

        void SetWrapperReshape(WrapperReshape function);

        void SetWrapperForward(WrapperForward function);

    private:
        json11::Json jsonParam;
        ParamType param = nullptr;
        WrapperReshape wrapperReshape = nullptr;
        WrapperForward wrapperForward = nullptr;
    };
}

#endif //PROJECT_WRAPPERLAYER_H
