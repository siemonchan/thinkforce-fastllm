//
// Created by test on 19-4-28.
//

#ifndef PROJECT_MULTILAYER_H
#define PROJECT_MULTILAYER_H
#include "../Layer.h"

namespace tfdl {
    template<typename T>
    class Multilayer : public Layer<T> {
    public:
        Multilayer(string layerName, string layerType,vector<Layer<T>*>layerstusk) : Layer<T>(layerName, layerType) {
            LayersTusk = layerstusk;
        }

        ~Multilayer() {
            for(auto layer : LayersTusk){
                delete layer;
            }
        }

        void Forward();
        void ForwardTF();
    private:
        vector<Layer<T>*>LayersTusk;
    };

}
#endif //PROJECT_MULTILAYER_H
