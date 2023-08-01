//
// Created by siemon on 12/11/19.
//

#ifndef PROJECT_FLIPLAYER_H
#define PROJECT_FLIPLAYER_H

#include "Net.h"

namespace tfdl {
    template<typename T>
    class FlipLayer : public Layer<T> {
    public:
        FlipLayer(const json11::Json &config, const string &dataType) : Layer<T>(config, dataType) {
            auto &param = config["param"];
        }

        void Forward();

        void ForwardTF();

        json11::Json ToJson();
    };
}

#endif //PROJECT_FLIPLAYER_H
