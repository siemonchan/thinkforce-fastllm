//
// Created by root on 3/28/23.
//

#ifndef TFDL_LOGSOFTMAXLAYER_H
#define TFDL_LOGSOFTMAXLAYER_H

#include "Layer.h"

namespace tfdl {
    template<typename T>
    class LogSoftMaxLayer : public Layer<T> {
    public:
        LogSoftMaxLayer(const json11::Json &config, string dataType) : Layer<T>(config, dataType) {
            auto param = config["param"];
            axis = param["axis"].int_value();
        }

        void Forward();

        void ForwardTF();

        void Prepare();

        string ToJsonParams();

        json11::Json ToJson();

        void FloatCpuLogSoftmax(float *input, float *output, int outer, int channel, int inner);

        void SetMode(int mode);

        int GetMode();
    private:
        int axis;
        int mode = 0;
    };
}

#endif //TFDL_LOGSOFTMAXLAYER_H
