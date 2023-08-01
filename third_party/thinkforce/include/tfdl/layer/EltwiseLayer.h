//
// Created by test on 17-12-7.
//

#ifndef PROJECT_ELTWISELAYER_H
#define PROJECT_ELTWISELAYER_H

#include "../Layer.h"

namespace tfdl{
    enum EltwiseOp {
        PROD = 0,
        SUM = 1,
        MAX = 2,
        SUB = 3
    };
    template<typename T>
    class EltwiseLayer:public Layer<T>{
    public:
        EltwiseLayer(const json11::Json &config, string dataType) : Layer<T>(config, dataType) {
            auto &param = config["param"];
            std::string operate = param["Operation"].string_value();
            if (operate == "SUM") {
                Op = EltwiseOp::SUM;
            }
            if (operate == "MAX") {
                Op = EltwiseOp::MAX;
            }
            if (operate == "PROD") {
                Op = EltwiseOp::PROD;
            }
            if (operate == "SUB") {
                Op = EltwiseOp::SUB;
            }

            if (!param["coeffs"].is_null()) {
                for (int i = 0; i < param["coeffs"].array_items().size(); i++) {
                    coeffs_.push_back(param["coeffs"].array_items()[i].number_value());
                }
                iscoeffs = false;
                for (int i = 0; i < coeffs_.size(); i++) {
                    if (fabs(coeffs_[i] - 1.0) > 1e-9) {
                        iscoeffs = true;
                    }
                }
            } else {
                iscoeffs = false;
            }
            relu = param["relu"].bool_value();
            reluX = param["reluX"].bool_value();
            if (reluX) {
                reluMax = param["reluMax"].int_value();
            }
        }
        ~EltwiseLayer(){
        }
        void Prepare();
        void Forward();
        void ForwardTF();
        void Reshape();
        void cpuFloatEltwise(vector <float*> inputs, float* output);
        bool wrong();
        void EnableReLU();
        void DisableReLU();
        void EnableReLUX(int max);
        void DisableReLUX();

        void SetMode(int mode);
        int GetMode();

        double GetExceptedTime(std::string device, int version);

        string ToJsonParams();
        json11::Json ToJson();
        string GetPrintType();

        string coeffsString();
        void SaveMiniNet(std::ofstream &outputFile);

#ifdef USE_TFACC40T
        void TFACCInit();
        bool TFACCSupport();
#endif

    private:
        vector<float> coeffs_;
        EltwiseOp Op;
        bool iscoeffs;
        //uint8_t* sumTable = nullptr;
        //vector<float> floatSumTable;
        bool sameConfig = false;
        bool relu = false;
        bool reluX = false;
        int reluMax = 0;
        int mode = 0;
        int rightShift;
        int m0, m1, offset;
    };
}

#endif //PROJECT_ELTWISELAYER_H
