//
// Created by huangyuyang on 8/7/20.
//

#ifndef PROJECT_BATCHMATRIXMUL_H
#define PROJECT_BATCHMATRIXMUL_H

#include "../Layer.h"

namespace tfdl {
    template<typename T>
    class BatchMatrixMulLayer : public Layer<T> {
    public:
        BatchMatrixMulLayer(const json11::Json &config, string dataType) : Layer<T>(config, dataType) {
            auto &param = config["param"];
            alpha = param["alpha"].number_value();
            transpose0 = param["transpose0"].int_value();
            transpose1 = param["transpose1"].int_value();
        }

        ~BatchMatrixMulLayer() {
        }

        void Forward();
        void ForwardTF();

        void Reshape();

        json11::Json ToJson();

        string GetParams();

        void SaveMiniNet(std::ofstream &outputFile);

        long long GetOperationTimes();

#ifdef USE_TFACC40T
        bool TFACCSupport();
        void TFACCInit();
        void TFACCMark();
        void TFACCRelease();

        bool tfacc_align = false;
        tfacc40t::ConvolutionLayer fcLayer;
#endif
    private:
        float alpha;
        bool transpose0, transpose1;

        // 后两维时实际相乘的大小, 前面所有维度看做batch
        int n, m, k;
        int batch0, batch1;
    };
}

#endif //PROJECT_BATCHMATRIXMUL_H
