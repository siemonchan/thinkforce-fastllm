//
// Created by siemon on 8/21/19.
//

#ifndef PROJECT_CHANNELNORMLAYER_H
#define PROJECT_CHANNELNORMLAYER_H

#include "Layer.h"

namespace tfdl {
    template <typename T>
    class ChannelNormLayer : public Layer<T> {
        ChannelNormLayer(const json11::Json &config, string dataType) : Layer<T>(config, dataType) {

        }
        ~ChannelNormLayer() {

        }

        void Forward();
        void ForwardTF();
        void Reshape();
    };
}

#endif //PROJECT_CHANNELNORMLAYER_H
