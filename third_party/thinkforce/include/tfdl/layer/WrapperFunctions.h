//
// Created by siemon on 4/22/20.
//

#ifndef PROJECT_WRAPPERFUNCTIONS_H
#define PROJECT_WRAPPERFUNCTIONS_H

#include "Interpreter/Attrs.h"
#include "INT8Config.h"
#include "Data.h"

namespace tfdl {
    using namespace attr;
    namespace wrapper {
        template<typename T>
        void TileReshape(vector<Data<T> *> inputs, vector<Data<T> *> outputs, vector<Data<T> *> weights,
                         INT8Config *int8Config, ParamType param) {
            vector<int> multiples = reinterpret_cast<TileParam *>(param)->multiples;
            vector<int> outputShape;
            for (int i = 0; i < multiples.size(); i++) {
                outputShape.push_back(multiples[i] * inputs[0]->GetDim(i));
            }
            outputs[0]->Resize(outputShape);
            outputs[0]->Reallocate(0);
        }

        template<typename T>
        void TileForward(vector<Data<T> *> inputs, vector<Data<T> *> outputs, vector<Data<T> *> weights,
                         INT8Config *int8Config, ParamType param) {
            vector<int> multiples = reinterpret_cast<TileParam *>(param)->multiples;
            int outputNum = outputs[0]->GetDim(0);
            int outputChannel = outputs[0]->GetDim(1);
            int outputHeight = outputs[0]->GetDim(2);
            int outputWidth = outputs[0]->GetDim(3);
            int mul_n = multiples[0];
            int mul_c = multiples.size() > 1 ? multiples[1] : 1;
            int mul_h = multiples.size() > 2 ? multiples[2] : 1;
            int mul_w = multiples.size() > 3 ? multiples[3] : 1;
            for (int n = 0; n < outputNum; n++) {
                for (int c = 0; c < outputChannel; c++) {
                    for (int h = 0; h < outputHeight; h++) {
                        for (int w = 0; w < outputWidth; w++) {
                            outputs[0]->Set(n, c, h, w, inputs[0]->Get(n / mul_n, c / mul_c, h / mul_h, w / mul_w));
                        }
                    }
                }
            }
        }

        template<typename T>
        void SqrtForward(vector<Data<T> *> inputs, vector<Data<T> *> outputs, vector<Data<T> *> weights,
                         INT8Config *int8Config, ParamType param) {
            float *floatInput = nullptr;
            float *floatOutput = nullptr;
            if (inputs[0]->GetDataType() == "int8") {
                floatInput = new float[inputs[0]->Count(0)];
                auto inputConfig = int8Config->GetDataConfig(inputs[0]->GetName());
                auto int8Input = inputs[0]->GetCpuData();
                for (int i = 0; i < inputs[0]->Count(0); i++) {
                    floatInput[i] = inputConfig->invQuantization(int8Input[i]);
                }
            } else if (inputs[0]->GetDataType() == "float") {
                floatInput = reinterpret_cast<float*>(inputs[0]->GetCpuData());
            } else {
                return;
            }

            if (outputs[0]->GetDataType() == "int8") {
                floatOutput = new float[outputs[0]->Count(0)];
            } else if (outputs[0]->GetDataType() == "float") {
                floatOutput = reinterpret_cast<float*>(outputs[0]->GetCpuData());
            } else {
                return;
            }

            for (int i = 0; i < outputs[0]->Count(0); i++) {
                floatOutput[i] = sqrtf(floatInput[i]);
            }

            if (outputs[0]->GetDataType() == "int8") {
                auto outputConfig = int8Config->GetDataConfig(outputs[0]->GetName());
                auto int8Output = outputs[0]->GetCpuData();
                for (int i = 0; i < outputs[0]->Count(0); i++) {
                    int8Output[i] = outputConfig->quantization(floatOutput[i]);
                }
                delete[] floatOutput;
            }
            if (inputs[0]->GetDataType() == "int8") {
                delete[] floatInput;
            }
        }

        template<typename T>
        void RsqrtForward(vector<Data<T> *> inputs, vector<Data<T> *> outputs, vector<Data<T> *> weights,
                          INT8Config *int8Config, ParamType param) {
            float *floatInput = nullptr;
            float *floatOutput = nullptr;
            if (inputs[0]->GetDataType() == "int8") {
                floatInput = new float[inputs[0]->Count(0)];
                auto inputConfig = int8Config->GetDataConfig(inputs[0]->GetName());
                auto int8Input = inputs[0]->GetCpuData();
                for (int i = 0; i < inputs[0]->Count(0); i++) {
                    floatInput[i] = inputConfig->invQuantization(int8Input[i]);
                }
            } else if (inputs[0]->GetDataType() == "float") {
                floatInput = reinterpret_cast<float*>(inputs[0]->GetCpuData());
            } else {
                return;
            }

            if (outputs[0]->GetDataType() == "int8") {
                floatOutput = new float[outputs[0]->Count(0)];
            } else if (outputs[0]->GetDataType() == "float") {
                floatOutput = reinterpret_cast<float*>(outputs[0]->GetCpuData());
            } else {
                return;
            }

            for (int i = 0; i < outputs[0]->Count(0); i++) {
                floatOutput[i] = 1 / sqrtf(floatInput[i]);
            }

            if (outputs[0]->GetDataType() == "int8") {
                auto outputConfig = int8Config->GetDataConfig(outputs[0]->GetName());
                auto int8Output = outputs[0]->GetCpuData();
                for (int i = 0; i < outputs[0]->Count(0); i++) {
                    int8Output[i] = outputConfig->quantization(floatOutput[i]);
                }
                delete[] floatOutput;
            }
            if (inputs[0]->GetDataType() == "int8") {
                delete[] floatInput;
            }
        }

        template <typename T>
        void LessForward(vector<Data<T> *> inputs, vector<Data<T> *> outputs, vector<Data<T> *> weights,
                         INT8Config *int8Config, ParamType param) {
            auto input1 = inputs[0]->GetCpuData();
            auto input2 = inputs[1]->GetCpuData();
            auto output = outputs[0]->GetCpuData();
            for (int i = 0; i < outputs[0]->Count(0); i++) {
                output[i] = T(input1[i] < input2[i]);
            }
        }

        template <typename T>
        void LessEqualForward(vector<Data<T> *> inputs, vector<Data<T> *> outputs, vector<Data<T> *> weights,
                              INT8Config *int8Config, ParamType param) {
            auto input1 = inputs[0]->GetCpuData();
            auto input2 = inputs[1]->GetCpuData();
            auto output = outputs[0]->GetCpuData();
            for (int i = 0; i < outputs[0]->Count(0); i++) {
                output[i] = T(input1[i] <= input2[i]);
            }
        }

        template <typename T>
        void GreaterForward(vector<Data<T> *> inputs, vector<Data<T> *> outputs, vector<Data<T> *> weights,
                            INT8Config *int8Config, ParamType param) {
            auto input1 = inputs[0]->GetCpuData();
            auto input2 = inputs[1]->GetCpuData();
            auto output = outputs[0]->GetCpuData();
            for (int i = 0; i < outputs[0]->Count(0); i++) {
                output[i] = T(input1[i] > input2[i]);
            }
        }

        template <typename T>
        void GreaterEqualForward(vector<Data<T> *> inputs, vector<Data<T> *> outputs, vector<Data<T> *> weights,
                               INT8Config *int8Config, ParamType param) {
            auto input1 = inputs[0]->GetCpuData();
            auto input2 = inputs[1]->GetCpuData();
            auto output = outputs[0]->GetCpuData();
            for (int i = 0; i < outputs[0]->Count(0); i++) {
                output[i] = T(input1[i] >= input2[i]);
            }
        }

        template <typename T>
        void ShapeReshape(vector<Data<T> *> inputs, vector<Data<T> *> outputs, vector<Data<T> *> weights,
                          INT8Config *int8Config, ParamType param) {
            reinterpret_cast<Data<int> *>(outputs[0])->Resize({inputs[0]->GetDims().size()});
            reinterpret_cast<Data<int> *>(outputs[0])->Reallocate(0);
            for (int i = 0; i < inputs[0]->GetDims().size(); i++) {
                reinterpret_cast<int *>(outputs[0]->GetCpuData())[i] = inputs[0]->GetDims()[i];
            }
        }
    }
}

#endif //PROJECT_WRAPPERFUNCTIONS_H
