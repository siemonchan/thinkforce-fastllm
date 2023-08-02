//
// Created by siemon on 7/6/23.
//

#include "devices/tfacc/tfaccdevice.h"
#include "devices/tfacc/fastllm-tfacc.h"

#include <cstring>
#include <thread>
#include <numa.h>

#include <cfloat>
#include <cmath>

#ifdef __aarch64__
#include <arm_neon.h>
#include "armMath.h"
#endif

#include "utils.h"

namespace fastllm {
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
        for (int i = 0; i < TF_TFNN_GetChipNum() * 8; i++) {
            if (TF_TFNN_CheckDeviceId(i) != TF_RET::TF_RET_SUCC) {
                ErrorInFastLLM("Create TfaccDevice error.");
                return;
            }
        }
        
        this->deviceType = "tfacc";
        this->ops["Linear"] = (BaseOperator *) (new TfaccLinearOp());

        // disable all tfacc cache
        int numaMemMask = numa_get_membind_compat().n[0];
        for (int i = 0; i < TF_TFNN_GetChipNum(); i++) {
            if (!(numaMemMask & (1 << i))) {
                continue;
            }
            for (int j = 0; j < 8; j++) {
                tfacc40t::BlasopList *blasopList = new tfacc40t::BlasopList(TF_TFNN_GetChipId(i * 8 + j), 100);
                AddTFACCRegisterBlasop(blasopList, CACHE_INIT_REQ, 1);
                AddTFACCRegisterBlasop(blasopList, CACHE_ADDR_MAP0_LOW_0_31, 0);
                AddTFACCRegisterBlasop(blasopList, CACHE_ADDR_MAP0_LOW_32_63, 0);
                AddTFACCRegisterBlasop(blasopList, CACHE_ADDR_MAP0_HI_0_31, 0);
                AddTFACCRegisterBlasop(blasopList, CACHE_ADDR_MAP0_HI_32_63, 0);
                AddTFACCRegisterBlasop(blasopList, UNCACHE_ADDR_MAP0_LOW_0_31, 0);
                AddTFACCRegisterBlasop(blasopList, UNCACHE_ADDR_MAP0_LOW_32_63, 0);
                AddTFACCRegisterBlasop(blasopList, UNCACHE_ADDR_MAP0_HI_0_31, 0xffffffff);
                AddTFACCRegisterBlasop(blasopList, UNCACHE_ADDR_MAP0_HI_32_63, 0xffffffff);
                blasopList->Run(i * 8 + j);
                delete blasopList;
            }
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
        Data &input = *(datas.find("input")->second);
        Data &output = *(datas.find("output")->second);
        Data &weight = *(datas.find("weight")->second);
        return input.dataType == DataType::FLOAT32 && output.dataType == DataType::FLOAT32 && 
               (weight.dataType == DataType::FLOAT32 || weight.dataType == DataType::INT8);
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
            
            auto pool = GetPool();
            FastllmTfaccLinearMultiCoreFloat(inputData, outputData, weightData, biasData, n, m, k, pool);
        } else if (weight.dataType == DataType::INT8) {
            float *inputData = (float *) input.cpuData;
            uint8_t *weightData = (uint8_t *) weight.cpuData;
            float *outputData = (float *) output.cpuData;
            float *biasData = bias.dims.size() > 0 ? (float *) bias.cpuData : nullptr;

            AssertInFastLLM(weight.tfWeightConfig.axis == 0, 
                            "Think Force`s TFACC only support per channel config on axis 0.");
            AssertInFastLLM(weight.tfWeightConfig.configs.size() == k, 
                            "Linear`s weight config size doesn`t match the requirement of Think Force`s TFACC");

            auto pool = GetPool();
            FastllmTfaccLinearMultiCoreInt8(inputData, outputData, weightData, biasData, n, m, k, weight.tfWeightConfig, pool);
        } else {
            ErrorInFastLLM("TFACC Linear error: unsupport weight's dataType.\n");
        }
    }

    long long int TfaccLinearOp::Ops(const std::string &opType, const DataDict &datas, const FloatDict &floatParams, const IntDict &intParams) {
        Data &input = *(datas.find("input")->second);
        Data &output = *(datas.find("output")->second);
        Data &weight = *(datas.find("weight")->second);

        int n = input.Count(0) / input.dims.back();
        int m = input.dims.back();
        int k = output.dims.back();

        return (long long int) n * m * k;
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

    bool TfaccMatMulTransBOp::CanRun(const std::string &opType, const DataDict &datas, const FloatDict &floatParams, 
                                     const IntDict &intParams) {
        Data &input0 = *(datas.find("input0")->second);
        Data &input1 = *(datas.find("input1")->second);
        Data &output = *(datas.find("output")->second);

        int batch = input0.dims[0];
        int n = input0.dims[1];
        int m = input0.dims[2];
        int k = input1.dims[1];
        return batch >= 16 && m >= 64;
    }

    void TfaccMatMulTransBOp::Reshape(const std::string &opType, const DataDict &datas, const FloatDict &floatParams, 
                                      const IntDict &intParams) {
        Data &input0 = *(datas.find("input0")->second);
        Data &input1 = *(datas.find("input1")->second);
        Data &output = *(datas.find("output")->second);

        AssertInFastLLM(input0.dataDevice == input1.dataDevice, "MatMulTransB error: inputs should use same device.\n");
        AssertInFastLLM(input0.dataType == DataType::FLOAT32 && input1.dataType == DataType::FLOAT32,
                        "MatMulTransB's input's type should be float32.\n");
        AssertInFastLLM(input0.dims.size() >= 2 && input1.dims.size() >= 2,
                        "MatMulTransB's input's shape's size should be >= 2.\n");
        AssertInFastLLM(input0.dims.back() == input1.dims.back(),
                        "MatMulTransB's shape error.\n");
        int input0Spatial = input0.Count(input0.dims.size() - 2);
        int input1Spatial = input1.Count(input1.dims.size() - 2);
        int batch0 = input0.Count(0) / input0Spatial;
        int batch1 = input1.Count(0) / input1Spatial;
        AssertInFastLLM(batch0 == batch1, "MatMulTransB's shape error.\n");

        std::vector <int> dims = input0.dims;
        dims.back() = input1.dims[input1.dims.size() - 2];
        output.dataType = input0.dataType;
        output.Resize(dims);
    }

    void TfaccMatMulTransBSingle(float *input, float *weight, float *output, int input0Spatial, int input1Spatial, int outputSpatial, 
                                 int input0Stride, int input1Stride, int n, int m, int k, int cur, int end, float alpha, int core) {
        for (int b = cur; b < end; b++) {
            float *input_walk = input + b * input0Spatial;
            float *weight_walk = weight + b * input1Spatial;
            float *output_walk = output + b * outputSpatial;
            for (int i = 0; i < n; i++) {
                tfacc40t::DotProd(input_walk + i * input0Stride, weight_walk, output_walk + i * k, k, m, input1Stride, core);
            }
        }
        int i = 0;
        float *output_walk = output + cur * outputSpatial;
#ifdef __aarch64__
        float32x4_t alphax4 = vdupq_n_f32(alpha);
        for (; i + 3 < (end - cur) * n * k; i += 4) {
            vst1q_f32(output_walk + i, vmulq_f32(vld1q_f32(output_walk + i), alphax4));
        }
#endif
        for (; i < (end - cur) * n * k; i++) {
            output_walk[i] *= alpha;
        }
    }

    void TfaccMatMulTransBOp::Run(const std::string &opType, const DataDict &datas, const FloatDict &floatParams, 
                                  const IntDict &intParams) {
        Data &input0 = *(datas.find("input0")->second);
        Data &input1 = *(datas.find("input1")->second);
        Data &output = *(datas.find("output")->second);

        output.Allocate();

        int batch = input0.dims[0];
        int n = input0.dims[input0.dims.size() - 2];
        int m = input0.dims.back();
        int k = input1.dims[input1.dims.size() - 2];
        int input0Spatial = input0.Count(input0.dims.size() - 2);
        int input1Spatial = input1.Count(input1.dims.size() - 2);
        int input0Stride = input0.strides[input0.dims.size() - 2];
        int input1Stride = input1.strides[input1.dims.size() - 2];
        int outputSpatial = output.Count(output.dims.size() - 2);
        float alpha = floatParams.find("alpha") != floatParams.end() ? floatParams.find("alpha")->second : 1.0;

        vector<int> cores;
        for (int i = 0; i < TF_TFNN_GetChipNum(); i++) {
            cores.push_back(8 * i + 0);
            cores.push_back(8 * i + 1);
            cores.push_back(8 * i + 2);
            cores.push_back(8 * i + 3);
        }

        float *input = (float *) input0.cpuData;
        float *weight = (float *) input1.cpuData;
        float *out = (float *) output.cpuData;

        int threadNum = min((int) cores.size(), GetThreads());
        if (batch * n * m * k < 64 * 4096) {
            threadNum = 1;
        }
        int per = batch / threadNum;
        int cur = 0;
        auto pool = GetPool();
        std::vector <std::future <void> > futures;
        for (int i = 0; i < threadNum - 1; i++) {
            int end = cur + per + (cur + per * (threadNum - i) < batch);
            futures.push_back(pool->Submit(TfaccMatMulTransBSingle,
                                           input, weight, out, input0Spatial, input1Spatial, outputSpatial,
                                           input0Stride, input1Stride, n, m, k, cur, end, alpha, cores[i]));
            cur = end;
        }
        TfaccMatMulTransBSingle(input, weight, out, input0Spatial, input1Spatial, outputSpatial,
                                input0Stride, input1Stride, n, m, k, cur, batch, alpha, cores[threadNum - 1]);
        for (int i = 0; i < futures.size(); i++) {
            futures[i].get();
        }
    }

    float GeluNewKernel(float x) {
        return 0.5f * x * (1.0f + tanhf(0.7978845608028654f * x * (1.0f + 0.044715f * x * x)));
    }

    void TfaccGeluNewOp::Run(const std::string &opType, const DataDict &datas, const FloatDict &floatParams, const IntDict &intParams) {
        Data &input = *(datas.find("input")->second);
        Data &output = *(datas.find("output")->second);
        output.Allocate();
        AssertInFastLLM(input.dataType == DataType::FLOAT32, "GeluNew error: Data's type should be float32.\n");

        float *inputData = (float*)input.cpuData;
        float *outputData = (float*)output.cpuData;
        int len = input.Count(0);
        int threadNum = min(4, GetThreads());
        vector<int> avail_cores = {0, 1, 8, 9};
        // printf("tfacc gelu, len: %d\n", len);

        int per = 4096;
        int cores = min(len / per, threadNum);
        per = len / cores;
        int last = len - (cores - 1) * per;

        auto pool = GetPool();
        std::vector <std::future <void> > futures;
        for (int i = 0; i < cores - 1; i++) {
            futures.push_back(pool->Submit([](float *inputData, float *outputData, int len, int deviceId){
                tfdl::TFDataFloat *input_tf = new tfdl::TFDataFloat({len}, inputData);
                tfdl::TFDataFloat *output_tf = new tfdl::TFDataFloat({len}, outputData);
                tfacc40t::MapTable(input_tf, output_tf, GeluNewKernel, deviceId);
                delete input_tf;
                delete output_tf;
            }, inputData + i * per, outputData + i * per, per, avail_cores[i]));
        }
        tfdl::TFDataFloat *input_tf = new tfdl::TFDataFloat({last}, inputData + (cores - 1) * per);
        tfdl::TFDataFloat *output_tf = new tfdl::TFDataFloat({last}, outputData + (cores - 1) * per);
        tfacc40t::MapTable(input_tf, output_tf, GeluNewKernel, cores);
        delete input_tf;
        delete output_tf;
        for (int i = 0; i < futures.size(); i++) {
            futures[i].get();
        }
    }
}