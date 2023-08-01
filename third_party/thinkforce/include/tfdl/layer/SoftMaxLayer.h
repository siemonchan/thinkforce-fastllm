//
// Created by yuyang.huang on 17-11-10.
//

#ifndef TFDL_SOFTMAXLAYER_H
#define TFDL_SOFTMAXLAYER_H

#include "../Layer.h"
#ifdef ARM
#include "armFunctions.h"
#endif

namespace tfdl {
    template<typename T>
    class SoftMaxLayer : public Layer<T> {
    public:
        SoftMaxLayer(const json11::Json &config, string dataType) : Layer<T>(config, dataType) {
            auto &param = config["param"];
            axis = ((param["axis"].is_null())?1:param["axis"].number_value());
        }
        ~SoftMaxLayer() {

        }

        string ToJsonParams();
        json11::Json ToJson();
        void Forward();
        void ForwardTF();
        void Prepare();
        void SetMode(int mode);
        int GetMode();
        void cpuFloatSoftMax(float *input, float *output, int outer, int channels, int inner);
        void SaveMiniNet(std::ofstream &outputFile);

#ifdef USE_TFACC40T
        bool TFACCSupport();
        void TFACCInit();
        void TFACCMark();
        void TFACCRelease();

        int maxBatch = 64;
        tfacc40t::MEM_U8 *tfaccInput = nullptr;
        tfacc40t::MEM_U8 *tfaccOutput = nullptr;
        tfacc40t::MEM_FLOAT *table = nullptr;
        tfacc40t::MEM_FLOAT *factor = nullptr;
        tfacc40t::MEM_U32 *lut = nullptr;
#endif
    private:
        int axis;
        int mode = 0;
    };


}

#endif //CAFFE_SOFTMAXLAYER_H
