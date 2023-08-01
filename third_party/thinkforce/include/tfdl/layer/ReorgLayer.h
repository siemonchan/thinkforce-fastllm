//
// Created by root on 18-5-9.
//

#ifndef PROJECT_REORGLAYER_H
#define PROJECT_REORGLAYER_H

#include "../Layer.h"

namespace tfdl {
    template<typename T>
    class ReorgLayer : public Layer<T> {
    public:
        ReorgLayer(const json11::Json &config, string dataType) : Layer<T>(config, dataType) {
            auto &param = config["param"];
            stride = param["stride"].int_value();
            reverse = param["reverse"].bool_value();

        }
        ~ReorgLayer() {

        }

        string ToJsonParams();
        json11::Json ToJson();
        void Forward();
        void ForwardTF();
        void Reshape();
        void reorg(T *x, int w, int h, int c, int batch, int stride, int Forward, T *out);
    private:
        bool reverse;
        int stride;
        int batchNum;
        int channels;
        int reorgedChannels;
        int height, width;
        int reorgedHeight, reorgedWidth;
    };
}

#endif //PROJECT_REORGLAYER_H
