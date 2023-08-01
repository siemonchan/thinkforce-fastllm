//
// Created by huangyuyang on 10/22/21.
//

#ifndef TFACC_FCLAYER_H
#define TFACC_FCLAYER_H

#include "blasop.h"

namespace tfacc40t {
    struct FCLayer {
        //io
        MEM_U8 *input;
        MEM_U8 *output;
        MEM_U8 *weight;
        MEM_U8 *bias;
        MEM_U32 *lutIndex;

        MEM_U64 *topk_tail;
        MEM_U8 *activation;

        //size param
        //result[N, K] = input[N, M] * output[M, K]
        int N, M, K;

        //cale param
        int quantizedMultiplier;
        int rightShift, outputZero, weightZero, inputZero;

        //helper
        int maus; // = 16;
        int single;
        int biasCnt; // = 16;
        int dsp; // = 256;
        int lut_stride;
        bool rqtz_bypass;
        bool relu;
        bool topK;
        bool int16;
        bool dual_int16;
        bool weight_direct;

        int crossSpatial;

        FCLayer();

        ~FCLayer();

        void AddFCBlasop(struct BlasopList *blasopList);
    };
}

#endif //TFACC_FCLAYER_H
