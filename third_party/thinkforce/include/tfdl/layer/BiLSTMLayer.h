//
// Created by huangyuyang on 12/2/20.
//

#ifndef PROJECT_BILSTMLAYER_H
#define PROJECT_BILSTMLAYER_H

#include "../Layer.h"

namespace tfdl {
    template<typename T>
    class BiLSTMLayer : public Layer<T> {
    public:
        BiLSTMLayer(const json11::Json &config, string dataType);

        ~BiLSTMLayer();

        void Forward();

        void ForwardTF();

        void Reshape();

        void Prepare();

        json11::Json ToJson();
    private:
        void *forwardLSTM;
        void *backwardLSTM;
        TFDataInt8 *fwResult = nullptr;
        TFDataInt8 *bwResult = nullptr;
        TFDataInt8 *h = nullptr, *hBack = nullptr;
        TFDataFloat *c = nullptr, *cBack = nullptr;

        int inputSize, outputSize;
        float forgetBias;
        bool useXRange, useICFORange;
        float forwardXMin, forwardXMax;
        float backwardXMin, backwardXMax;
        float forwardICFOMin, forwardICFOMax;
        float backwardICFOMin, backwardICFOMax;
        float ICFOMin, ICFOMax;

        int tfaccNum = 1;
        int threadNum = 1;

        json11::Json param;
    };
}

#endif //PROJECT_BILSTMLAYER_H
