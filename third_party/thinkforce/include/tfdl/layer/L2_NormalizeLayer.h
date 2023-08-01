//
// Created by test on 17-12-29.
//

#ifndef PROJECT_L2_NORMALIZELAYER_H
#define PROJECT_L2_NORMALIZELAYER_H
#include "../Layer.h"

namespace tfdl {
    template<typename T>
    class L2_NormalizeLayer : public Layer<T> {
    public:
        L2_NormalizeLayer(const json11::Json &config, string dataType) : Layer<T>(config, dataType) {

        }
        ~L2_NormalizeLayer() {

        }

        void cpuFloatL2(float *input, float *output, int num, int spatial, int channels);
        void Forward();
        void ForwardTF();

        json11::Json ToJson();

        void SaveMiniNet(std::ofstream &outputFile);
    };


}
#endif //PROJECT_L2_NORMALIZELAYER_H
