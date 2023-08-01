//
// Created by siemon on 5/20/19.
//

#ifndef PROJECT_CHANNELWISELAYER_H
#define PROJECT_CHANNELWISELAYER_H

#include "Layer.h"
#include "utils.h"

namespace tfdl {
    enum ChannelWiseOp {
        CW_PROD = 0,
        CW_SUM = 1
    };

    template <typename T>
    class ChannelWiseLayer : public Layer<T> {
    public:
        ChannelWiseLayer(const json11::Json &config, string dataType) : Layer<T>(config, dataType) {
            auto &param = config["param"];
            string operate = param["Operation"].string_value();
            if (operate == "PROD") {
                op = ChannelWiseOp::CW_PROD;
            }
            if (operate == "SUM") {
                op = ChannelWiseOp::CW_SUM;
            }
        }

        ~ChannelWiseLayer() {
            if(m != nullptr) {
                delete [] m;
            }
        }

        void Forward();
        void ForwardTF();

        void SUM();
        void PROD();

        void Reshape();
        void Prepare();

        string ToJsonParams();

        json11::Json ToJson();

        void SaveMiniNet(std::ofstream &outputFile);

#ifdef USE_TFACC40T
        void TFACCInit();

        void TFACCMark();

        bool TFACCSupport();

        void TFACCRelease();
#endif

    private:
        ChannelWiseOp op = CW_PROD;
        bool brodcast = false;

        ArithmeticParams param;

        #define CW_SHIFT 16
        #define CW_ROUND 0.5

        uint8_t z1, z2, z3;
        float s1, s2, s3;
        int spatial;
        int32_t* m = nullptr;

#ifdef USE_TFACC40T
        tfacc40t::MEM_U8 *broadcast_mem = nullptr;
#endif

#ifdef ARM
        uint8x8_t zero1;
        uint8x8_t zero2;
        int16x8_t zero3;
        int32x4_t m1, m2, b;
        int16x8_t qmax;
        int16x8_t qmin;
#endif // ARM
        int32_t multiplier1, multiplier2, offset;
    };
}

#endif //PROJECT_CHANNELWISELAYER_H
