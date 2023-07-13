//
// Created by siemon on 7/6/23.
//

#include "devices/tfacc/tfaccdevice.h"

#include <cstring>
#include <thread>

#include <cfloat>
#include <cmath>

#ifdef __aarch64__
#include <arm_neon.h>
#include "armMath.h"
#endif

#include "utils.h"

namespace fastllm {
    // extern void MultiplyMultiThread(uint8_t *a, uint8_t *b, int32_t *c, int n, int m, int k, int threadNum);
    // double ComputeCosSim(float *a, float *b, int len) {
    //     long double sab = 0.0f, saa = 0.0f, sbb = 0.0f;
    //     for (int i = 0; i < len; i++) {
    //         sab += (long double)a[i] * b[i];
    //         saa += (long double)a[i] * a[i];
    //         sbb += (long double)b[i] * b[i];
    //     }
    //     if (fabs(saa + sbb) < 1e-9) {
    //         return 1.0;
    //     }
    //     return (double)(sab / sqrt(saa * sbb));
    // }

    TfaccDevice::TfaccDevice() {
        this->deviceType = "tfacc";
        this->ops["Linear"] = (BaseOperator *) (new TfaccLinearOp());
        // this->ops["MatMul"] = (BaseOperator *) (new TfaccMatMulOp()); // there is no point using tfacc to do bmm

        // disable all tfacc cache
        for (int i = 0; i < 8 * TF_TFNN_GetChipNum(); i++) {
            tfacc40t::BlasopList *blasopList = new tfacc40t::BlasopList(TF_TFNN_GetChipId(i), 100);
            AddTFACCRegisterBlasop(blasopList, CACHE_INIT_REQ, 1);
            AddTFACCRegisterBlasop(blasopList, CACHE_ADDR_MAP0_LOW_0_31, 0);
            AddTFACCRegisterBlasop(blasopList, CACHE_ADDR_MAP0_LOW_32_63, 0);
            AddTFACCRegisterBlasop(blasopList, CACHE_ADDR_MAP0_HI_0_31, 0);
            AddTFACCRegisterBlasop(blasopList, CACHE_ADDR_MAP0_HI_32_63, 0);
            AddTFACCRegisterBlasop(blasopList, UNCACHE_ADDR_MAP0_LOW_0_31, 0);
            AddTFACCRegisterBlasop(blasopList, UNCACHE_ADDR_MAP0_LOW_32_63, 0);
            AddTFACCRegisterBlasop(blasopList, UNCACHE_ADDR_MAP0_HI_0_31, 0xffffffff);
            AddTFACCRegisterBlasop(blasopList, UNCACHE_ADDR_MAP0_HI_32_63, 0xffffffff);
            blasopList->Run(i);
            delete blasopList;
        }
    }

    bool TfaccDevice::Malloc(void **ret, size_t size) {
        *ret = (void*)new uint8_t [size];
        return true;
    }

    bool TfaccDevice::Free(void *ret) {
        delete[] (uint8_t *)ret;
        return true;
    }

    bool TfaccDevice::CopyDataToCPU(void *dst, void *src, size_t size) {
        return true;
    }
    
    bool TfaccDevice::CopyDataFromCPU(void *dst, void *src, size_t size) {
        return true;
    }

    bool TfaccLinearOp::CanRun(const std::string &opType, const DataDict &datas, const FloatDict &floatParams, const IntDict &intParams) {
        return true;
    }

    void TfaccLinearOp::Reshape(const std::string &opType, const DataDict &datas, const FloatDict &floatParams, const IntDict &intParams) {
        Data &input = *(datas.find("input")->second);
        Data &output = *(datas.find("output")->second);
        Data &weight = *(datas.find("weight")->second);

        AssertInFastLLM(weight.dims.size() == 2, "Linear's weight's shape's size should be 2.\n");
        AssertInFastLLM(input.dims.back() == weight.dims[1], "Linear's weight's shape error.\n");

        weight.weightType = WeightType::LINEAR;
        std::vector <int> dims = input.dims;
        dims.back() = weight.dims[0];

        output.dataType = DataType::FLOAT32;
        output.Resize(dims);
    }

    void TfaccLinearOp::Run(const std::string &opType, const DataDict &datas, const FloatDict &floatParams, const IntDict &intParams) {
        Data &input = *(datas.find("input")->second);
        Data &output = *(datas.find("output")->second);
        Data &weight = *(datas.find("weight")->second);
        Data &bias = *(datas.find("bias")->second);

        output.Allocate(0.0f);
        int n = input.Count(0) / input.dims.back();
        int m = input.dims.back();
        int k = output.dims.back();

        if (weight.dataType == DataType::FLOAT32) {
            float *inputData = (float *) input.cpuData;
            float *weightData = (float *) weight.cpuData;
            float *outputData = (float *) output.cpuData;
            float *biasData = bias.dims.size() > 0 ? (float *) bias.cpuData : nullptr;
            
            tfdl::TFDataFloat *tf_input = new tfdl::TFDataFloat({n, m}, inputData);
            tfdl::TFDataFloat *tf_weight = new tfdl::TFDataFloat({k, m}, weightData);
            tfdl::TFDataFloat *tf_output = new tfdl::TFDataFloat({n, k}, outputData);
            tfdl::TFDataFloat *tf_bias = biasData ? new tfdl::TFDataFloat({k}, biasData) : nullptr;
            
            int threadNum = min(TF_TFNN_GetChipNum() * 8, GetThreads());
            tfacc40t::LinearMultiCore(tf_input, tf_output, tf_weight, tf_bias, threadNum);

            delete tf_input;
            delete tf_output;
            delete tf_weight;
            delete tf_bias;
        } else if (weight.dataType == DataType::INT8) {
            float *inputData = (float *) input.cpuData;
            uint8_t *weightData = (uint8_t *) weight.cpuData;
            float *outputData = (float *) output.cpuData;
            float *biasData = bias.dims.size() > 0 ? (float *) bias.cpuData : nullptr;

            tfdl::TFDataFloat *tf_input = new tfdl::TFDataFloat({n, m}, inputData);
            tfdl::TFDataInt8 *tf_weight = new tfdl::TFDataInt8(0.f, 0.f, {k, m}, weightData);
            tfdl::TFDataFloat *tf_output = new tfdl::TFDataFloat({n, k}, outputData);
            tfdl::TFDataFloat *tf_bias = biasData ? new tfdl::TFDataFloat({k}, biasData) : nullptr;

            AssertInFastLLM(weight.tfWeightConfig.axis == 0, 
                            "Think Force`s TFACC only support per channel config on axis 0.");
            AssertInFastLLM(weight.tfWeightConfig.configs.size() == k, 
                            "Linear`s weight config size doesn`t match the requirement of Think Force`s TFACC");
            tf_weight->SetPerChannelConfig(weight.tfWeightConfig);

            int threadNum = min(TF_TFNN_GetChipNum() * 8, GetThreads());
            tfacc40t::LinearMultiCore(tf_input, tf_output, tf_weight, tf_bias, threadNum);

            delete tf_input;
            delete tf_output;
            delete tf_weight;
            delete tf_bias;
        } else {
            ErrorInFastLLM("TFACC Linear error: unsupport weight's dataType.\n");
        }
    }

    bool TfaccMatMulOp::CanRun(const std::string &opType, const DataDict &datas, const FloatDict &floatParams, const IntDict &intParams) {
        Data &input0 = *(datas.find("input0")->second);
        Data &input1 = *(datas.find("input1")->second);
        Data &output = *(datas.find("output")->second);
        float alpha = floatParams.find("alpha") != floatParams.end() ? floatParams.find("alpha")->second : 1.0;

        int n = input0.dims[input0.dims.size() - 2];
        int m = input0.dims.back();
        int k = input1.dims[input1.dims.size() - 1];
        int batch = input0.Count(0) / n / m;

        return alpha == 1.0 && n > 8 && k > 32;
    }

    void TfaccMatMulOp::Reshape(const std::string &opType, const DataDict &datas, const FloatDict &floatParams, const IntDict &intParams) {
        Data &input0 = *(datas.find("input0")->second);
        Data &input1 = *(datas.find("input1")->second);
        Data &output = *(datas.find("output")->second);

        AssertInFastLLM(input0.dataDevice == input1.dataDevice, "MatMul error: inputs should use same device.\n");
        AssertInFastLLM(input0.dataType == DataType::FLOAT32 && input1.dataType == DataType::FLOAT32,
                        "MatMul's input's type should be float32.\n");
        AssertInFastLLM(input0.dims.size() >= 2 && input1.dims.size() >= 2,
                        "MatMul's input's shape's size should be >= 2.\n");
        AssertInFastLLM(input0.dims.back() == input1.dims[input1.dims.size() - 2],
                        "MatMul's shape error.\n");
        int input0Spatial = input0.Count(input0.dims.size() - 2);
        int input1Spatial = input1.Count(input1.dims.size() - 2);
        int batch0 = input0.Count(0) / input0Spatial;
        int batch1 = input1.Count(0) / input1Spatial;
        AssertInFastLLM(batch0 == batch1, "MatMul's shape error.\n");

        std::vector <int> dims = input0.dims;
        dims.back() = input1.dims[input1.dims.size() - 1];

        output.dataType = input0.dataType;
        output.Resize(dims);
    }

    void TfaccMatMulOp::Run(const std::string &opType, const DataDict &datas, const FloatDict &floatParams, const IntDict &intParams) {
        Data &input0 = *(datas.find("input0")->second);
        Data &input1 = *(datas.find("input1")->second);
        Data &output = *(datas.find("output")->second);

        output.Allocate();

        int n = input0.dims[input0.dims.size() - 2];
        int m = input0.dims.back();
        int k = input1.dims[input1.dims.size() - 1];
        int batch = input0.Count(0) / n / m;
        
        float *input0Data = (float *) input0.cpuData;
        float *input1Data = (float *) input1.cpuData;
        float *outputData = (float *) output.cpuData;
        tfdl::TFDataFloat *x = new tfdl::TFDataFloat({batch, n, m}, input0Data);
        tfdl::TFDataFloat *y = new tfdl::TFDataFloat({batch, m, k}, input1Data);
        tfdl::TFDataFloat *out = new tfdl::TFDataFloat({batch, n, k}, outputData);

        int threadNum = min(TF_TFNN_GetChipNum() * 8, GetThreads());
        tfacc40t::BatchMatrixMulMultiCore(x, y, out, threadNum);

        delete x;
        delete y;
        delete out;
    }
}