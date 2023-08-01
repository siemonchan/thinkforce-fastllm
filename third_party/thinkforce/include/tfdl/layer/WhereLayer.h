//
// Created by huangyuyang on 1/4/23.
//

#ifndef TFDL_WHERE_H
#define TFDL_WHERE_H

#include "../Layer.h"

namespace tfdl {
    template<typename T>
    class WhereLayer : public Layer<T> {
    public:
        WhereLayer(const json11::Json &config, string dataType) : Layer<T>(config, dataType) {
        }

        ~WhereLayer() {
        }

        void Forward();

        void ForwardTF();

        void Reshape();
    private:
    };
}

#endif //TFDL_WHERE_H
