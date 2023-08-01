//
// Created by feng.jianhao on 18-8-13.
//

#ifndef PROJECT_PRELULAYER_H
#define PROJECT_PRELULAYER_H

#include "Layer.h"
#include "common.h"

namespace tfdl {

    template<typename T>
    class PReLULayer : public Layer<T> {

    public:
        PReLULayer(const json11::Json &config, std::string datatype) : Layer<T>(config, datatype) {
            auto param = config["param"];
            shared_channel = param["channel_shared"].bool_value();
            this->weights.push_back(new Data<T>);
        }

        ~PReLULayer(){
            if (!this->ShareAttr) {
                for (Data<T> *d : this->weights) {
                    delete d;
                }
            }
        }
        void Forward();

        void ForwardTF();

        json11::Json ToJson();

        void Prepare();
    private:
        bool shared_channel;
    };
}

#endif //PROJECT_PRELULAYER_H
