//
// Created by yuyang.huang on 17-11-14.
//

#ifndef TFDL_CONCATLAYER_H
#define TFDL_CONCATLAYER_H

#include "../Layer.h"

namespace tfdl {
    template<typename T>
    class ConcatLayer : public Layer<T> {
    public:
        ConcatLayer(const json11::Json &config, string dataType) : Layer<T>(config, dataType) {
            auto &param = config["param"];
            axis = param["axis"].int_value();
        }
        ~ConcatLayer() {

        }

        string ToJsonParams();
        json11::Json ToJson();
        void Forward();
        void ForwardTF();
        bool wrong();
        void Reshape();
        int GetAxis();
        void Prepare();
        void SaveMiniNet(std::ofstream &outputFile);

#ifdef USE_TFACC40T
        void TFACCInit();
        bool TFACCSupport();
#endif

    private:
        int axis;
        bool mapping = false;
    };


}

#endif //CAFFE_CONCATLAYER_H
