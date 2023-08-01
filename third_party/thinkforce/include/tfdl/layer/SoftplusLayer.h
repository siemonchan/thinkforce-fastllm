//
// Created by xinliang.wang on 9/17/20.
//

#ifndef PROJECT_SOFTPLUS_H
#define PROJECT_SOFTPLUS_H

#include "../Layer.h"

namespace tfdl {
    template<typename T>
    class SoftplusLayer : public Layer<T> {
    public:
        SoftplusLayer(const json11::Json &config, string dataType) : Layer<T>(config, dataType) {
        }
        ~SoftplusLayer() {
        }

        json11::Json ToJson();
        void Forward();
        void ForwardTF();
    private:
    
    };
}

#endif //PROJECT_SOFTPLUS_H

