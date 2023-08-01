//
// Created by siemon on 1/6/20.
//

#ifndef PROJECT_REDUCEARGSLAYER_H
#define PROJECT_REDUCEARGSLAYER_H

#include "Layer.h"

namespace tfdl {
    enum ReduceType {
        ReduceMax = 0,
        ReduceMin = 1,
        ReduceMean = 2,
        ReduceSum = 3,
        ReduceProd = 4,
        ReduceLogSum = 5,
        ReduceLogSumExp = 6,
        ReduceSumSquare = 7,
        ReduceL1,
        ReduceL2
    };

    template<typename T>
    class ReduceArgsLayer : public Layer<T> {
    public:
        ReduceArgsLayer(const json11::Json &config, string dataType) : Layer<T>(config, dataType) {
            auto param = config["param"];
            string op = param["Operation"].string_value();
            if (op == "max") {
                operation = ReduceType::ReduceMax;
            } else if (op == "min") {
                operation = ReduceType::ReduceMin;
            } else if (op == "mean") {
                operation = ReduceType::ReduceMean;
            } else if (op == "sum") {
                operation = ReduceType::ReduceSum;
            } else if (op == "prod") {
                operation = ReduceType::ReduceProd;
            } else if (op == "logsum") {
                operation = ReduceType::ReduceLogSum;
            } else if (op == "logsumexp") {
                operation = ReduceType::ReduceLogSumExp;
            } else if (op == "sumsquare") {
                operation = ReduceType::ReduceSumSquare;
            } else if (op == "l1") {
                operation = ReduceType::ReduceL1;
            } else if (op == "l2") {
                operation = ReduceType::ReduceL2;
            }
            axis = param["axis"].int_value();
            keep_dims = bool(param["keepdims"].int_value());
            axisArray.clear();
            for (const auto &id : param["axisArray"].array_items()) {
                axisArray.push_back(id.int_value());
            }
        }

        void Forward();

        void ForwardTF();

        void Reshape();

        string OperationString();

        json11::Json ToJson();

        string GetPrintType();

    private:
        ReduceType operation;
        bool keep_dims = true;
        int axis;
        vector <int> axisArray;
    };
}

#endif //PROJECT_REDUCEARGSLAYER_H
