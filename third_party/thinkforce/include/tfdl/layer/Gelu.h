//
// Created by huangyuyang on 8/7/20.
//

#ifndef PROJECT_GELU_H
#define PROJECT_GELU_H

#include "../Layer.h"

namespace tfdl {
    template<typename T>
    class GeluLayer : public Layer<T> {
    public:
        GeluLayer(const json11::Json &config, string dataType) : Layer<T>(config, dataType) {
            auto param = config["param"];
            if (!param["approximation"].is_null()) {
                this->approximation = param["approximation"].string_value();
            }
        }
        ~GeluLayer() {
        }

        json11::Json ToJson();
        void Forward();
        void ForwardTF();
        void SaveMiniNet(std::ofstream &outputFile);

        bool IsActivationLayer();
        void GetMapTable(vector <uint8_t> &mapTable);

        string ToJsonParams();

#ifdef USE_TFACC40T
        bool TFACCSupport();
#endif
    private:
        string approximation = "tanh";
    };
}

#endif //PROJECT_GELU_H
