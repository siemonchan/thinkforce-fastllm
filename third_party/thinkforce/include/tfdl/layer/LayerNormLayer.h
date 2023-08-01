//
// Created by huangyuyang on 8/7/20.
//

#ifndef PROJECT_LAYERNORMLAYER_H
#define PROJECT_LAYERNORMLAYER_H

#include "../Layer.h"

namespace tfdl {
    template<typename T>
    class LayerNormLayer : public Layer<T> {
    public:
        LayerNormLayer(const json11::Json &config, string dataType) : Layer<T>(config, dataType) {
            auto &param = config["param"];
            this->weights.push_back(new Data<T>);
            this->weights.push_back(new Data<T>);
            axis = ((param["axis"].is_null()) ? -1 : param["axis"].int_value());
        }
        ~LayerNormLayer() {
        }

        json11::Json ToJson();
        void Forward();
        void ForwardTF();
        void SaveMiniNet(std::ofstream &outputFile);
    private:
        int axis;
    };
}

#endif //PROJECT_LAYERNORMLAYER_H
