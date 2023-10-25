//
// Created by huangyuyang on 6/13/23.
//

#include "devices/cpu/cpudevice.h"

#include <cstring>
#include <thread>

#include <cfloat>
#include <cmath>
#include <random>

#ifdef __aarch64__
#include <arm_neon.h>
#include "armMath.h"
#endif

#include "utils.h"

namespace fastllm {
    CpuDevice::CpuDevice() {
        this->deviceType = "cpu";
        this->ops["ToFloat16"] = (BaseOperator*)(new CpuToFloat16());
        this->ops["ToFloat32"] = (BaseOperator*)(new CpuToFloat32());
        this->ops["Attention"] = (BaseOperator*)(new CpuAttention());
        this->ops["CopyKVCache"] = (BaseOperator*)(new CpuCopyKVCacheOp());
        this->ops["Embedding"] = (BaseOperator*)(new CpuEmbedding());
        this->ops["LayerNorm"] = (BaseOperator*)(new CpuLayerNormOp());
        this->ops["GroupNorm"] = (BaseOperator*)(new CpuGroupNormOp());
        this->ops["RMSNorm"] = (BaseOperator*)(new CpuRMSNormOp());
        this->ops["Linear"] = (BaseOperator*)(new CpuLinearOp());
        this->ops["Conv2d"] = (BaseOperator*)(new CpuConv2dOp());
        this->ops["Interpolate"] = (BaseOperator*)(new CpuInterpolateOp());
        this->ops["Split"] = (BaseOperator*)(new CpuSplitOp());
        this->ops["Cat"] = (BaseOperator*)(new CpuCatOp());
        this->ops["CatDirect"] = (BaseOperator*)(new CpuCatDirectOp());
        this->ops["MatMul"] = (BaseOperator*)(new CpuMatMulOp());
        this->ops["MatMulTransB"] = (BaseOperator*)(new CpuMatMulTransBOp());
        this->ops["SoftMax"] = (BaseOperator*)(new CpuSoftMaxOp());
        this->ops["Silu"] = (BaseOperator*)(new CpuSiluOp());
        this->ops["GeluNew"] = (BaseOperator*)(new CpuGeluNewOp());
        this->ops["Swiglu"] = (BaseOperator*)(new CpuSwigluOp());
        this->ops["Mul"] = (BaseOperator*)(new CpuMulOp());
        this->ops["MulTo"] = (BaseOperator*)(new CpuMulToOp());
        this->ops["Scaling"] = (BaseOperator*)(new CpuScalingOp());
        this->ops["AddTo"] = (BaseOperator*)(new CpuAddToOp());
        this->ops["AttentionMask"] = (BaseOperator*)(new CpuAttentionMaskOp());
        this->ops["AlibiMask"] = (BaseOperator*)(new CpuAlibiMaskOp());
        this->ops["TopK"] = (BaseOperator*)(new CpuTopKOp());
        this->ops["Permute"] = (BaseOperator*)(new CpuPermuteOp());
        this->ops["PermuteSelf"] = (BaseOperator*)(new CpuPermuteSelfOp());
        this->ops["RotatePosition2D"] = (BaseOperator*)(new CpuRotatePosition2DOp());
        this->ops["NearlyRotatePosition2D"] = (BaseOperator*)(new CpuNearlyRotatePosition2DOp());
        this->ops["LlamaRotatePosition2D"] = (BaseOperator*)(new CpuLlamaRotatePosition2DOp());
        this->ops["RepeatPenalty"] = (BaseOperator*)(new CpuRepeatPenaltyOp());
        this->ops["RepeatKV"] = (BaseOperator*)(new CpuRepeatKVOp());
        this->ops["Repeat"] = (BaseOperator*)(new CpuRepeatOp());
        this->ops["ApplyLognAttn"] = (BaseOperator*)(new CpuApplyLognAttnOp());
        this->ops["Linspace"] = (BaseOperator*)(new CpuLinspaceOp());
        this->ops["Pow"] = (BaseOperator*)(new CpuPowOp());
        this->ops["CumProd"] = (BaseOperator*)(new CpuCumProdOp());
        this->ops["Unique"] = (BaseOperator*)(new CpuUniqueOp());
        this->ops["Randn"] = (BaseOperator*)(new CpuRandnOp());
        this->ops["ImageNormalize"] = (BaseOperator*)(new CpuImageNormalizeOp());

        this->ops["SplitBatch"] = (BaseOperator*)(new CpuSplitBatchOp());
        this->ops["CatBatch"] = (BaseOperator*)(new CpuCatBatchOp());
        this->ops["MulBatch"] = (BaseOperator*)(new CpuMulBatchOp());
        this->ops["MatMulBatch"] = (BaseOperator*)(new CpuMatMulBatchOp());
        this->ops["MatMulTransBBatch"] = (BaseOperator*)(new CpuMatMulTransBBatchOp());
        this->ops["SoftMaxBatch"] = (BaseOperator*)(new CpuSoftmaxBatchOp());
        this->ops["CatDirectBatch"] = (BaseOperator*)(new CpuCatDirectBatchOp());
        this->ops["AttentionBatch"] = (BaseOperator*)(new CpuAttentionBatchOp());
    }

    bool CpuDevice::Malloc(void **ret, size_t size) {
        *ret = (void*)new uint8_t [size];
        return true;
    }

    bool CpuDevice::Free(void *ret) {
        delete[] (uint8_t*)ret;
        return true;
    }

    bool CpuDevice::CopyDataFromCPU(void *dst, void *src, size_t size) {
        return true;
    }

    bool CpuDevice::CopyDataToCPU(void *dst, void *src, size_t size) {
        return true;
    }


#ifdef __AVX2__
    int DotU8U8(uint8_t *a, uint8_t *b, int n) {
        __m256i acc = _mm256_setzero_si256();
        int i = 0;
        int ans = 0;
        const __m256i lowMask = _mm256_set1_epi8(0xf);
        const __m256i ones = _mm256_set1_epi16(1);
        const __m256i ones8 = _mm256_set1_epi8(1);
        const __m256i xors = _mm256_set1_epi8(-128);
        for (; i + 31 < n; i += 32) {
            __m256i bx = _mm256_loadu_si256((const __m256i *) (a + i));
            __m256i by = _mm256_loadu_si256((const __m256i *) (b + i));

            by = _mm256_xor_si256(by, xors);
            by = _mm256_add_epi8(by, _mm256_and_si256(_mm256_cmpeq_epi8(by, xors), ones8));

            by = _mm256_sign_epi8(by, bx);
            bx = _mm256_sign_epi8(bx, bx);

            acc = _mm256_add_epi32(acc, _mm256_madd_epi16(_mm256_maddubs_epi16(bx, by), ones));
        }
        for (; i < n; i++) {
            ans += ((int8_t*)a)[i] * ((int)b[i] - 128);
        }

        return ans + I32sum(acc);
    };
//#else
//    int DotU8U8(uint8_t *a, uint8_t *b, int n) {
//        __m256i acc = _mm256_setzero_si256();

//        int i = 0;
//        int ans = 0;
//        for (; i + 31 < n; i += 32) {
//            __m256i bx = _mm256_loadu_si256((const __m256i *) (a + i));
//            __m256i by = _mm256_loadu_si256((const __m256i *) (b + i));

//            __m256i mx0 = _mm256_cvtepu8_epi16(_mm256_extractf128_si256(bx, 0));
//            __m256i mx1 = _mm256_cvtepu8_epi16(_mm256_extractf128_si256(bx, 1));

//            __m256i my0 = _mm256_cvtepu8_epi16(_mm256_extractf128_si256(by, 0));
//            __m256i my1 = _mm256_cvtepu8_epi16(_mm256_extractf128_si256(by, 1));

//            acc = _mm256_add_epi32(acc, _mm256_madd_epi16(mx0, my0));
//            //acc = _mm256_add_epi32(acc, _mm256_madd_epi16(mx1, my1));
//        }
//        for (; i < n; i++) {
//            ans += a[i] * b[i];
//        }

//        return ans + I32sum(acc);
//    };
    int DotU4U8(uint8_t *a, uint8_t *b, int n) {
        __m256i acc = _mm256_setzero_si256();

        int i = 0;
        int ans = 0;
        const __m256i lowMask = _mm256_set1_epi8(0xf);
        const __m256i ones = _mm256_set1_epi16(1);
        for (; i + 31 < n; i += 32) {
            __m128i orix = _mm_loadu_si128((const __m128i *) (a + i / 2));
            __m256i bytex = _mm256_set_m128i(_mm_srli_epi16(orix, 4), orix);
            __m256i bx = _mm256_and_si256(lowMask, bytex);
            __m256i by = _mm256_loadu_si256((const __m256i *) (b + i));
            acc = _mm256_add_epi32(acc, _mm256_madd_epi16(_mm256_maddubs_epi16(by, bx), ones));
        }
        for (; i < n; i++) {
            ans += a[i] * b[i];
        }

        return ans + I32sum(acc);
    };
#endif

    struct FP16ToFP32Manager {
        float dict[65536];

        FP16ToFP32Manager() {
            for (uint16_t i = 0; i < 65535; i++) {
                dict[i] = half_to_float(i);
            }
        }
    } fp16tofp32;

    void Transpose(float *pDst, float *pSrc, int dstStride, int srcStride, int n, int m);

    void CpuToFloat16::Run(const std::string &opType, const fastllm::DataDict &datas,
                           const fastllm::FloatDict &floatParams, const fastllm::IntDict &intParams) {
        Data &data = *(datas.find("input")->second);
        if (data.dataType == DataType::FLOAT16) {
            return;
        }
        if (data.dataType == DataType::FLOAT32) {
            float *old = (float*)data.cpuData;
            data.dataType = DataType::FLOAT16;
            data.cpuData = new uint8_t[data.GetBytes()];
            uint16_t *cur = (uint16_t*)data.cpuData;
            int len = data.Count(0);
            for (int i = 0; i < len; i++) {
                cur[i] = float_to_half(old[i]);
            }
            delete[] old;
        } else {
            ErrorInFastLLM("ToFloat16: unsupport dataType.\n");
        }
    }

    void CpuToFloat32::Run(const std::string &opType, const fastllm::DataDict &datas,
                           const fastllm::FloatDict &floatParams, const fastllm::IntDict &intParams) {
        Data &data = *(datas.find("input")->second);
        if (data.dataType == DataType::FLOAT32) {
            return;
        }
        if (data.dataType == DataType::FLOAT16) {
            uint16_t *old = (uint16_t*)data.cpuData;
            data.dataType = DataType::FLOAT32;
            data.UpdateUnitSize();
            data.cpuData = new uint8_t[data.GetBytes()];
            float *cur = (float*)data.cpuData;
            int len = data.Count(0);
            for (int i = 0; i < len; i++) {
                cur[i] = fp16tofp32.dict[old[i]];
            }
            delete[] old;
        } else {
            ErrorInFastLLM("ToFloat32: unsupport dataType.\n");
        }
    }

    void CpuAttention::Reshape(const std::string &opType, const fastllm::DataDict &datas,
                               const fastllm::FloatDict &floatParams, const fastllm::IntDict &intParams) {
        Data &q = *(datas.find("q")->second);
        Data &k = *(datas.find("k")->second);
        Data &v = *(datas.find("v")->second);
        Data &output = *(datas.find("output")->second);
        int group = intParams.find("group") != intParams.end() ? intParams.find("group")->second : 1;

        AssertInFastLLM(q.dims.size() == 3 && k.dims.size() == 3 && v.dims.size() == 3, "Attention: dims of q, k, v should be 3.\n");
        AssertInFastLLM(q.dims[2] == k.dims[2], "Attention: q.dims[2] should be equal to k.dims[2].\n");
        AssertInFastLLM(k.dims[1] == v.dims[1], "Attention: k.dims[1] should be equal to v.dims[1].\n");
        AssertInFastLLM(k.dims[0] == v.dims[0], "Attention: k.dims[0] should be equal to v.dims[0].\n");
        AssertInFastLLM(q.dims[0] == k.dims[0] * group, "Attention: q.dims[0] should be equal to k.dims[0] * group.\n");

        AssertInFastLLM(q.dataType == k.dataType && q.dataType == v.dataType,
                        "Attention: q, k, v's datatype should be same.\n");
        AssertInFastLLM(q.dataType == DataType::FLOAT32, "Attention's input's type should be float32.\n");

        std::vector <int> dims = {q.dims[0], q.dims[1], v.dims[2]};
        output.dataType = q.dataType;
        output.Resize(dims);
    }

    void SingleAttention(float *qd, float *kd, float *vd, float *maskd, float *od,
                         float scale, int q1, int q2, int k1, int v2) {
        float *qk = new float[k1];
        float *temp = new float[k1];
        for (int i = 0; i < q1; i++) {
            float maxValue = -10000, sum = 0.0;
            for (int j = 0; j < k1; j++) {
                if (maskd && maskd[i * k1 + j] > 0.99) {
                    qk[j] = -10000;
                    continue;
                }
                float sum = 0.0f;
                for (int l = 0; l < q2; l++) {
                    sum += qd[i * q2 + l] * kd[j * q2 + l];
                }
                qk[j] = sum * scale;
                maxValue = std::max(maxValue, sum * scale);
            }
            for (int j = 0; j < k1; j++) {
                temp[j] = expf(qk[j] - maxValue);
                sum += temp[j];
            }
            sum = std::max(sum, 0.1f);
            for (int j = 0; j < k1; j++) {
                qk[j] = temp[j] / sum;
            }
            for (int j = 0; j < k1; j++) {
                for (int l = 0; l < v2; l++) {
                    od[i * v2 + l] += qk[j] * vd[j * v2 + l];
                }
            }
        }
        delete[] qk;
        delete[] temp;
    }

    void CpuAttention::Run(const std::string &opType, const fastllm::DataDict &datas,
                           const fastllm::FloatDict &floatParams, const fastllm::IntDict &intParams) {
        Data &q = *(datas.find("q")->second);
        Data &k = *(datas.find("k")->second);
        Data &v = *(datas.find("v")->second);
        Data &mask = *(datas.find("mask")->second);
        Data &output = *(datas.find("output")->second);
        int group = intParams.find("group") != intParams.end() ? intParams.find("group")->second : 1;
        float scale = floatParams.find("scale") != floatParams.end() ? floatParams.find("scale")->second : 1.0;
        output.Allocate();
        int q0 = q.dims[0], q1 = q.dims[1], q2 = q.dims[2], k0 = k.dims[0], k1 = k.dims[1], v2 = v.dims[2];
        float *qd = (float*)q.cpuData;
        float *kd = (float*)k.cpuData;
        float *vd = (float*)v.cpuData;
        float *maskd = (datas.find("mask")->second && mask.dims.size() > 0) ? (float*)mask.cpuData : nullptr;
        float *od = (float*)output.cpuData;
        int batch = (mask.dims.size() == 3 ? mask.dims[0] : 1);
        int maskStride = (mask.dims.size() == 3 ? mask.strides[0] : mask.Count(0));
        std::fill(od, od + output.Count(0), 0.0f);
        auto pool = GetPool();
        std::vector<std::future<void> > futures;
        for (int o = 0; o < q0; o++) {
            futures.push_back(pool->Submit(SingleAttention,
                            qd + o * q.strides[0], kd + (o / group) * k.strides[0], vd + (o / group) * v.strides[0],
                            maskd + (o / (q0 / batch)) * maskStride, od + o * output.strides[0], scale,
                            q1, q2, k1, v2));
        }
        for (int o = 0; o < futures.size(); o++) {
            futures[o].get();
        }
    }

    void CpuCopyKVCacheOp::Reshape(const std::string &opType, const fastllm::DataDict &datas,
                                   const fastllm::FloatDict &floatParams, const fastllm::IntDict &intParams) {
        return;
    }

    void CpuCopyKVCacheOp::Run(const std::string &opType, const fastllm::DataDict &datas,
                               const fastllm::FloatDict &floatParams, const fastllm::IntDict &intParams) {
        Data &oldCache = *(datas.find("oldCache")->second);
        Data &newCache = *(datas.find("newCache")->second);

        int oldBsStart = intParams.find("oldBsStart") != intParams.end() ? intParams.find("oldBsStart")->second : -1;
        int newBsStart = intParams.find("newBsStart") != intParams.end() ? intParams.find("newBsStart")->second : -1;
        int bs = intParams.find("bs") != intParams.end() ? intParams.find("bs")->second : -1;
        int offset = intParams.find("offset") != intParams.end() ? intParams.find("offset")->second : -1;

        int unitSize = oldCache.unitSize;
        for (int o = 0; o < bs; o++) {
            uint8_t *cur = newCache.cpuData + (newBsStart + o) * newCache.strides[0] * unitSize;
            cur += offset * newCache.strides[1] * unitSize;
            uint8_t *old = oldCache.cpuData + (oldBsStart + o) * oldCache.strides[0] * unitSize;
            memcpy(cur, old, oldCache.dims[1] * oldCache.dims[2] * unitSize);
        }
    }

    void CpuEmbedding::Reshape(const std::string &opType, const fastllm::DataDict &datas,
                               const fastllm::FloatDict &floatParams, const fastllm::IntDict &intParams) {
        Data &input = *(datas.find("input")->second);
        Data &output = *(datas.find("output")->second);
        Data &weight = *(datas.find("weight")->second);

        AssertInFastLLM(weight.dims.size() == 2, "Embedding's weight's dim should be 2.\n");
        AssertInFastLLM(weight.dataType == DataType::FLOAT32 ||
                        weight.dataType == DataType::BFLOAT16, "Embedding's weight's type should be float32 or bfloat16.\n");
        AssertInFastLLM(input.dataType == DataType::FLOAT32, "Embedding's input's type should be float32.\n");

        weight.weightType = WeightType::EMBEDDING;
        int vocabSize = weight.dims[0], embSize = weight.dims[1];
        std::vector <int> dims = input.dims;
        dims.push_back(embSize);

        output.dataType = DataType::FLOAT32;
        output.Resize(dims);
    }

    void CpuEmbedding::Run(const std::string &opType, const fastllm::DataDict &datas,
                               const fastllm::FloatDict &floatParams, const fastllm::IntDict &intParams) {
        Data &input = *(datas.find("input")->second);
        Data &output = *(datas.find("output")->second);
        Data &weight = *(datas.find("weight")->second);;

        output.Allocate();

        int vocabSize = weight.dims[0], embSize = weight.dims[1];
        uint64_t inputLen = input.Count(0);
        float *inputData = (float*)input.cpuData;

        if (GetLowMemMode()) {
            FILE *fi = fopen(weight.fileName.c_str(), "rb");
            if (weight.dataType == DataType::FLOAT32) {
                float *outputData = (float *) output.cpuData;
                for (int i = 0; i < inputLen; i++) {
                    int token = (int) (inputData[i] + 1e-9);
#if defined(_WIN32) or defined(_WIN64)
                    _fseeki64(fi, (long long)token * embSize * sizeof(float) + weight.filePos, 0);
#else
                    fseek(fi, (long long)token * embSize * sizeof(float) + weight.filePos, 0);
#endif
                    int ret = fread(outputData + i * embSize, sizeof(float), embSize, fi);
                }
            } else {
                uint16_t *outputData = (uint16_t *) output.cpuData;
                uint16_t *weightData = new uint16_t[embSize];
                for (int i = 0; i < inputLen; i++) {
                    int token = (int) (inputData[i] + 1e-9);
#if defined(_WIN32) or defined(_WIN64)
                    _fseeki64(fi, (long long)token * embSize * sizeof(uint16_t) + weight.filePos, 0);
#else
                    fseek(fi, (long long)token * embSize * sizeof(uint16_t) + weight.filePos, 0);
#endif
                    int ret = fread(weightData, sizeof(uint16_t), embSize, fi);
                    for (int j = 0; j < embSize; j++) {
                        outputData[i * embSize * 2 + j * 2] = 0;
                        outputData[i * embSize * 2 + j * 2 + 1] = weightData[j];
                    }
                }
                delete[] weightData;
            }
            fclose(fi);
        } else {
            if (weight.dataType == DataType::FLOAT32) {
                float *outputData = (float *) output.cpuData;
                float *weightData = (float *) weight.cpuData;
                for (int i = 0; i < inputLen; i++) {
                    int token = (int) (inputData[i] + 1e-9);
                    memcpy(outputData + i * embSize, weightData + token * embSize, embSize * sizeof(float));
                }
            } else {
                uint16_t *outputData = (uint16_t *) output.cpuData;
                uint16_t *weightData = (uint16_t *) weight.cpuData;
                for (int i = 0; i < inputLen; i++) {
                    int token = (int) (inputData[i] + 1e-9);
                    for (int j = 0; j < embSize; j++) {
                        outputData[i * embSize * 2 + j * 2] = 0;
                        outputData[i * embSize * 2 + j * 2 + 1] = weightData[token * embSize + j];
                    }
                }
            }
        }
    }

    void CpuLayerNormOp::Run(const std::string &opType, const fastllm::DataDict &datas,
                             const fastllm::FloatDict &floatParams, const fastllm::IntDict &intParams) {
        Data &input = *(datas.find("input")->second);
        Data &output = *(datas.find("output")->second);
        Data &gamma = *(datas.find("gamma")->second);
        Data &beta = *(datas.find("beta")->second);

        output.Allocate();

        int axis = intParams.find("axis") != intParams.end() ? intParams.find("axis")->second : -1;
        int dimsLen = input.dims.size();
        axis = (axis % dimsLen + dimsLen) % dimsLen;

        int outer = input.Count(0) / input.Count(axis);
        int channels = input.dims[axis];
        int inner = input.strides[axis];

        float *mean = new float[inner], *var = new float[inner];
        float *inputData = (float *) input.cpuData;
        float *outputData = (float *) output.cpuData;
        float *gammaData = (float *) gamma.cpuData;
        float *betaData = (float *) beta.cpuData;

        if (inner == 1) {
            for (int i = 0; i < outer; i++) {
                float mean = 0.f, s2 = 0.f, var = 0.f;
                int j = 0;
#ifdef __aarch64__
                float32x4_t sums = vdupq_n_f32(0.0);
                    float32x4_t sums2 = vdupq_n_f32(0.0);
                    for (; j + 3 < channels; j += 4) {
                        float32x4_t vi = vld1q_f32(inputData + j);
                        sums = vaddq_f32(sums, vi);
                        sums2 = vaddq_f32(sums2, vmulq_f32(vi, vi));
                    }
                    mean = sums[0] + sums[1] + sums[2] + sums[3];
                    s2 = sums2[0] + sums2[1] + sums2[2] + sums2[3];
#endif
#ifdef __AVX2__
                __m256 sum_vec = _mm256_setzero_ps();
                __m256 squared_sum_vec = _mm256_setzero_ps();

                for (; j < channels - 7; j += 8) {
                    __m256 data_vec = _mm256_loadu_ps(inputData + j);
                    sum_vec = _mm256_add_ps(sum_vec, data_vec);

                    __m256 squared_data_vec = _mm256_mul_ps(data_vec, data_vec);
                    squared_sum_vec = _mm256_add_ps(squared_sum_vec, squared_data_vec);
                }

                float sum_array[8];
                _mm256_storeu_ps(sum_array, sum_vec);
                mean = sum_array[0] + sum_array[1] + sum_array[2] + sum_array[3] +
                            sum_array[4] + sum_array[5] + sum_array[6] + sum_array[7];

                float squared_sum_array[8];
                _mm256_storeu_ps(squared_sum_array, squared_sum_vec);
                s2 = squared_sum_array[0] + squared_sum_array[1] +
                                    squared_sum_array[2] + squared_sum_array[3] +
                                    squared_sum_array[4] + squared_sum_array[5] +
                                    squared_sum_array[6] + squared_sum_array[7];
#endif
                for (; j < channels; j++) {
                    mean += inputData[j];
                    s2 += inputData[j] * inputData[j];
                }
                mean /= channels;
                var = sqrt(s2 / channels - mean*mean + 1e-6);
                j = 0;
#ifdef __aarch64__
                float32x4_t means = vdupq_n_f32(mean);
                    float32x4_t vars = vdupq_n_f32(1.0 / var);
                    for (; j + 3 < channels; j += 4) {
                        float32x4_t va = vld1q_f32(gammaData + j), vb = vld1q_f32(betaData + j);
                        float32x4_t vi = vld1q_f32(inputData + j);
                        float32x4_t vo = vaddq_f32(vmulq_f32(vmulq_f32(vsubq_f32(vi, means), vars), va), vb);
                        vst1q_f32(outputData + j, vo);
                    }
#endif
                for (; j < channels; j++) {
                    float a = gammaData[j], b = betaData[j];
                    outputData[j] = (inputData[j] - mean) / var * a + b;
                }

                inputData += channels;
                outputData += channels;
            }
            return;
        } else {
            for (int i = 0; i < outer; i++) {
                std::fill(mean, mean + inner, 0.f);
                std::fill(var, var + inner, 0.f);
                float *inputWalk = inputData;
                for (int j = 0; j < channels; j++) {
                    for (int k = 0; k < inner; k++) {
                        mean[k] += *inputWalk++;
                    }
                }
                for (int k = 0; k < inner; k++) {
                    mean[k] /= channels;
                }
                inputWalk = inputData;
                for (int j = 0; j < channels; j++) {
                    for (int k = 0; k < inner; k++) {
                        float x = (*inputWalk++) - mean[k];
                        var[k] += x * x;
                    }
                }
                for (int k = 0; k < inner; k++) {
                    var[k] = sqrt(var[k] / channels + 1e-5);
                }

                inputWalk = inputData;
                float *outputWalk = outputData;
                for (int j = 0; j < channels; j++) {
                    float a = gammaData[j], b = betaData[j];
                    for (int k = 0; k < inner; k++) {
                        *outputWalk++ = ((*inputWalk++) - mean[k]) / var[k] * a + b;
                    }
                }

                inputData += channels * inner;
                outputData += channels * inner;
            }
            delete[] mean;
            delete[] var;
        }
    }

    void CpuGroupNormSingleGroup(float *dst, float *src, int channel, int spatial, float *gamma, float *beta) {
        float mean = 0, var = 0;
        int j = 0;
#ifdef __aarch64__
        float32x4_t sum = vdupq_n_f32(0);
        for (; j + 3 < channel * spatial; j += 4) {
            sum = vaddq_f32(sum, vld1q_f32(src + j));
        }
        mean = sum[0] + sum[1] + sum[2] + sum[3];
#endif
        for (; j < channel * spatial; j++) {
            mean += src[j];
        }
        mean /= (float) (channel * spatial);

        j = 0;
#ifdef __aarch64__
        float32x4_t meanx4 = vdupq_n_f32(mean);
        float32x4_t varx4 = vdupq_n_f32(0);
        for (; j + 3 < channel * spatial; j += 4) {
            float32x4_t xx4 = vsubq_f32(vld1q_f32(src + j), meanx4);
            varx4 = vaddq_f32(varx4, vmulq_f32(xx4, xx4));
        }
        var = varx4[0] + varx4[1] + varx4[2] + varx4[3];
#endif
        for (; j < channel * spatial; j++) {
            float x = src[j] - mean;
            var += x * x;
        }
        var = sqrtf(var / (channel * spatial) + 1e-5);

        for (int c = 0; c < channel; c++) {
            float g = gamma[c];
            float b = beta[c];
            int s = 0;
#ifdef __aarch64__
            float32x4_t gx4 = vdupq_n_f32(g);
            float32x4_t bx4 = vdupq_n_f32(b);
            float32x4_t meanx4 = vdupq_n_f32(mean);
            float32x4_t varx4 = vdupq_n_f32(var);
            for (; s + 3 < spatial; s += 4) {
                float32x4_t tmp = vdivq_f32(vsubq_f32(vld1q_f32(src + s), meanx4), varx4);
                vst1q_f32(dst + s, vaddq_f32(vmulq_f32(tmp, gx4), bx4));
            }
#endif
            for (; s < spatial; s++) {
                dst[s] = ((src[s] - mean) / var) * g + b;
            }

            src += spatial;
            dst += spatial;
        }
    }

    void CpuGroupNormOp::Run(const std::string &opType, const DataDict &datas, 
                             const FloatDict &floatParams, const IntDict &intParams) {
        Data &input = *(datas.find("input")->second);
        Data &output = *(datas.find("output")->second);
        Data &gamma = *(datas.find("gamma")->second);
        Data &beta = *(datas.find("beta")->second);
        int group = intParams.find("group")->second;

        output.Allocate();
        int batch = input.dims[0];
        int channel = input.dims[1];
        int perChannel = channel / group;
        int spatial = input.dims[2] * input.dims[3];

        float *inputData = (float *) input.cpuData;
        float *outputData = (float *) output.cpuData;
        float *gammaData = (float *) gamma.cpuData;
        float *betaData = (float *) beta.cpuData;

        auto pool = GetPool();
        int threadNum = GetThreads();

        std::vector<std::future<void>> futures;
        for (int b = 0; b < batch; b++) {
            for (int g = 0; g < group; g++) {
                float *src = inputData + (b * channel + g * perChannel) * spatial;
                float *dst = outputData + (b * channel + g * perChannel) * spatial;
                float *gamma = gammaData + g * perChannel;
                float *beta = betaData + g * perChannel;
                futures.push_back(pool->Submit(CpuGroupNormSingleGroup, dst, src, perChannel, spatial, gamma, beta));
            }
        }
        for (int i = 0; i < futures.size(); i++) {
            futures[i].get();
        }
    }

    void CpuRMSNormOp::Run(const std::string &opType, const fastllm::DataDict &datas,
                      const fastllm::FloatDict &floatParams, const fastllm::IntDict &intParams) {
        Data &input = *(datas.find("input")->second);
        Data &weight = *(datas.find("weight")->second);
        Data &output = *(datas.find("output")->second);
        output.Allocate();

        float eps = floatParams.find("eps") != floatParams.end() ? floatParams.find("eps")->second : 1e-5;
        int dimsLen = input.dims.size();
        int axis = dimsLen - 1;
        int outer = input.Count(0) / input.Count(axis);
        int channels = input.dims[axis];

        if (input.dataType == DataType::FLOAT32) {
            float *inputData = (float *) input.cpuData;
            float *outputData = (float *) output.cpuData;
            float *weightData = (float *) weight.cpuData;

            for (int i = 0; i < outer; i++) {
                float mean = 0.f;
                int j = 0;
#ifdef __aarch64__X
                float32x4_t sums = vdupq_n_f32(0.0);
                float32x4_t sums2 = vdupq_n_f32(0.0);
                for (; j + 3 < channels; j += 4) {
                    float32x4_t vi = vld1q_f32(inputData + j);
                    sums = vaddq_f32(sums, vi);
                    sums2 = vaddq_f32(sums2, vmulq_f32(vi, vi));
                }
                mean = sums[0] + sums[1] + sums[2] + sums[3];
                s2 = sums2[0] + sums2[1] + sums2[2] + sums2[3];
#endif
                for (; j < channels; j++) {
                    mean += inputData[j] * inputData[j];
                }
                float scale = 1.0 / sqrt(mean / channels + eps);
                j = 0;
#ifdef __aarch64__X
                float32x4_t means = vdupq_n_f32(mean);
                float32x4_t vars = vdupq_n_f32(1.0 / var);
                for (; j + 3 < channels; j += 4) {
                    float32x4_t va = vld1q_f32(gammaData + j), vb = vld1q_f32(betaData + j);
                    float32x4_t vi = vld1q_f32(inputData + j);
                    float32x4_t vo = vaddq_f32(vmulq_f32(vmulq_f32(vsubq_f32(vi, means), vars), va), vb);
                    vst1q_f32(outputData + j, vo);
                }
#endif
                for (; j < channels; j++) {
                    outputData[j] = inputData[j] * scale * weightData[j];
                }

                inputData += channels;
                outputData += channels;
            }
        } else if (input.dataType == DataType::FLOAT16) {
            uint16_t *inputData = (uint16_t *) input.cpuData;
            uint16_t *outputData = (uint16_t *) output.cpuData;
            float *weightData = (float *) weight.cpuData;

            for (int i = 0; i < outer; i++) {
                float mean = 0.f;
                int j = 0;
                for (; j < channels; j++) {
                    float x = fp16tofp32.dict[inputData[j]];
                    mean += x * x;
                }
                float scale = 1.0 / sqrt(mean / channels + eps);
                j = 0;
                for (; j < channels; j++) {
                    outputData[j] = float_to_half(fp16tofp32.dict[inputData[j]] * scale * weightData[j]);
                }

                inputData += channels;
                outputData += channels;
            }
        } else {
            ErrorInFastLLM("RMSNorm error: unsupport dataType.\n");
        }
    }

    void CpuLinearOp::Reshape(const std::string &opType, const fastllm::DataDict &datas,
                              const fastllm::FloatDict &floatParams, const fastllm::IntDict &intParams) {
        Data &input = *(datas.find("input")->second);
        Data &output = *(datas.find("output")->second);
        Data &weight = *(datas.find("weight")->second);

        AssertInFastLLM(weight.dims.size() == 2, "Linear's weight's shape's size should be 2.\n");
        AssertInFastLLM(input.dims.back() == weight.dims[1], "Linear's weight's shape error.\n");

        weight.weightType = WeightType::LINEAR;
        std::vector <int> dims = input.dims;
        dims.back() = weight.dims[0];

        output.dataType = input.dataType;
        output.Resize(dims);
    }

    void FloatLinearPart(float *inputData, float *weightData, float *biasData, float *outputData,
                         int n, int m, int k, int st, int end) {
        for (int i = 0; i < n; i++) {
            for (int j = st; j < end; j++) {
                float now = biasData ? biasData[j] : 0.0f;
                int l = 0;
#ifdef __aarch64__
                float32x4_t sum = {0, 0, 0, 0};
                for (; l + 3 < m; l += 4) {
                    sum = vaddq_f32(sum, vmulq_f32(vld1q_f32(inputData + i * m + l), vld1q_f32(weightData + j * m + l)));
                }
                now += sum[0] + sum[1] + sum[2] + sum[3];
#else
#ifdef __AVX2__
                __m256 vsum = _mm256_setzero_ps();
                for (; l + 7 < m; l += 8) {
                    __m256 vi = _mm256_loadu_ps(inputData + i * m + l);
                    __m256 vw = _mm256_loadu_ps(weightData + j * m + l);
                    vsum = _mm256_fmadd_ps(vi, vw, vsum);
                }
                now += Floatsum(vsum);
#endif
#endif
                for (; l < m; l++) {
                    now += inputData[i * m + l] * weightData[j * m + l];
                }
                outputData[i * k + j] = now;
            }
        }
    }

    // float的input, float16的weight, 直接计算得到float的output
    void Float16LinearPart(float *inputData, uint16_t *weightData, float *biasData, float *outputData,
                           int n, int m, int k, int st, int end) {
        for (int i = 0; i < n; i++) {
            for (int j = st; j < end; j++) {
                float now = biasData ? biasData[j] : 0.0f;
                int l = 0;
#ifdef __ARM_FEATURE_FP16_VECTOR_ARITHMETIC
                float16x8_t sum = {0, 0, 0, 0, 0, 0, 0, 0};
                for (; l + 7 < m; l += 8) {
                    sum = vfmaq_f16(sum, vld1q_f16((float16_t*)inputData + i * m + l),
                                        vld1q_f16((float16_t*)weightData + j * m + l));
                }
                now += sum[0] + sum[1] + sum[2] + sum[3] + sum[4] + sum[5] + sum[6] + sum[7];
#else
#ifdef __aarch64__
                float32x4_t sum = {0, 0, 0, 0};
                for (; l + 3 < m; l += 4) {
                    float32x4_t vcur = {fp16tofp32.dict[weightData[j * m + l]], fp16tofp32.dict[weightData[j * m + l + 1]],
                                        fp16tofp32.dict[weightData[j * m + l + 2]], fp16tofp32.dict[weightData[j * m + l + 3]]};
                    sum = vaddq_f32(sum, vmulq_f32(vld1q_f32(inputData + i * m + l), vcur));
                }
                now += sum[0] + sum[1] + sum[2] + sum[3];
#else
#ifdef __AVX2__
                __m256 vsum = _mm256_setzero_ps();
                for (; l + 7 < m; l += 8) {
                    __m256 vi = _mm256_loadu_ps(inputData + i * m + l);
                    __m256 vw = _mm256_cvtph_ps(_mm_loadu_si128((__m128i *) (weightData + j * m + l)));
                    vsum = _mm256_fmadd_ps(vi, vw, vsum);
                }
                now += Floatsum(vsum);
#endif
#endif
#endif
                for (; l < m; l++) {
                    now += inputData[i * m + l] * fp16tofp32.dict[weightData[j * m + l]];
                }
                outputData[i * k + j] = now;
            }
        }
    }

    // float16的input, float16的weight, 直接计算得到float16的output
    void Float16xFloat16LinearPart(uint16_t *inputData, uint16_t *weightData, float *biasData, uint16_t *outputData,
                           int n, int m, int k, int st, int end) {
        for (int i = 0; i < n; i++) {
            for (int j = st; j < end; j++) {
                float now = biasData ? biasData[j] : 0.0f;
                int l = 0;
#ifdef __ARM_FEATURE_FP16_VECTOR_ARITHMETIC
                float16x8_t sum = {0, 0, 0, 0, 0, 0, 0, 0};
                for (; l + 7 < m; l += 8) {
                    sum = vfmaq_f16(sum, vld1q_f16((float16_t*)inputData + i * m + l),
                                        vld1q_f16((float16_t*)weightData + j * m + l));
                }
                now += sum[0] + sum[1] + sum[2] + sum[3] + sum[4] + sum[5] + sum[6] + sum[7];
#endif
                for (; l < m; l++) {
                    now += inputData[i * m + l] * fp16tofp32.dict[weightData[j * m + l]];
                }
                outputData[i * k + j] = float_to_half(now);
            }
        }
    }

    // float的input, int8的weight, 直接计算得到float的output
    void Int8LinearPart(float *inputData, uint8_t *weightData, float *biasData, float *outputData,
                        LowBitConfig *configs, int n, int m, int k, int st, int end) {
        for (int i = 0; i < n; i++) {
            for (int j = st; j < end; j++) {
                float now = biasData ? biasData[j] : 0.0f;
                int l = 0;
#ifdef __aarch64__
                float32x4_t scales = vdupq_n_f32(configs[j].scale);
                uint8x8_t zeros = vdup_n_u8(configs[j].zeroPoint);
                float32x4_t sum0 = {0, 0, 0, 0};
                float32x4_t sum1 = {0, 0, 0, 0};
                for (; l + 7 < m; l += 8) {
                    uint8x8_t a = vld1_u8(weightData + j * m + l);
                    uint16x8_t result = vsubl_u8(a, zeros);
                    int16x8_t sresult = vreinterpretq_s16_u16(result);
                    int16x4_t result1 = vget_low_s16(sresult);
                    int16x4_t result2 = vget_high_s16(sresult);
                    int32x4_t result3 = vmovl_s16(result1);
                    int32x4_t result4 = vmovl_s16(result2);
                    float32x4_t f1 = vmulq_f32(scales, vcvtq_f32_s32(result3));
                    float32x4_t f2 = vmulq_f32(scales, vcvtq_f32_s32(result4));

                    sum0 = vaddq_f32(sum0, vmulq_f32(vld1q_f32(inputData + i * m + l + 0), f1));
                    sum1 = vaddq_f32(sum1, vmulq_f32(vld1q_f32(inputData + i * m + l + 4), f2));
                }
                now += sum0[0] + sum0[1] + sum0[2] + sum0[3];
                now += sum1[0] + sum1[1] + sum1[2] + sum1[3];
#endif

                for (; l < m; l++) {
                    now += inputData[i * m + l] * configs[j].invQuantization(weightData[j * m + l]);
                }

                outputData[i * k + j] = now;
            }
        }
    }

    // float的input, int4的weight, 直接计算得到float的output
    void Int4LinearPart(float *inputData, uint8_t *weightData, float *biasData, float *outputData,
                        LowBitConfig *configs, int n, int m, int k, int st, int end) {
        for (int i = 0; i < n; i++) {
            for (int j = st; j < end; j++) {
                float now = biasData ? biasData[j] : 0.0f;
                int l = 0;
#ifdef __aarch64__X
                float32x4_t scales = vdupq_n_f32(configs[j].scale);
                uint8x8_t zeros = vdup_n_u8(configs[j].zeroPoint);
                uint8x8_t maskHigh = vdup_n_u8(0xF0);
                uint8x8_t maskLow = vdup_n_u8(0xF);
                float32x4_t sum0 = {0, 0, 0, 0};
                float32x4_t sum1 = {0, 0, 0, 0};

                for (; l + 15 < m; l += 16) {
                    uint8x8_t ori = vld1_u8(weightData + (j * m + l) / 2);
                    float32x4x2_t in0 = vld2q_f32(inputData + i * m + l + 0);
                    float32x4x2_t in1 = vld2q_f32(inputData + i * m + l + 8);
                    uint8x8_t a = vand_u8(ori, maskLow);
                    uint16x8_t result = vsubl_u8(a, zeros);
                    int16x8_t sresult = vreinterpretq_s16_u16(result);
                    int16x4_t result1 = vget_low_s16(sresult);
                    int16x4_t result2 = vget_high_s16(sresult);
                    int32x4_t result3 = vmovl_s16(result1);
                    int32x4_t result4 = vmovl_s16(result2);
                    float32x4_t f1 = vmulq_f32(scales, vcvtq_f32_s32(result3));
                    float32x4_t f2 = vmulq_f32(scales, vcvtq_f32_s32(result4));
                    sum0 = vaddq_f32(sum0, vmulq_f32(in0.val[1], f1));
                    sum1 = vaddq_f32(sum1, vmulq_f32(in1.val[1], f2));

                    a = vshr_n_u8(vand_u8(ori, maskHigh), 4);
                    result = vsubl_u8(a, zeros);
                    sresult = vreinterpretq_s16_u16(result);
                    result1 = vget_low_s16(sresult);
                    result2 = vget_high_s16(sresult);
                    result3 = vmovl_s16(result1);
                    result4 = vmovl_s16(result2);
                    f1 = vmulq_f32(scales, vcvtq_f32_s32(result3));
                    f2 = vmulq_f32(scales, vcvtq_f32_s32(result4));

                    sum0 = vaddq_f32(sum0, vmulq_f32(in0.val[0], f1));
                    sum1 = vaddq_f32(sum1, vmulq_f32(in1.val[0], f2));
                }
                now += sum0[0] + sum0[1] + sum0[2] + sum0[3];
                now += sum1[0] + sum1[1] + sum1[2] + sum1[3];
#endif

                for (; l < m; l++) {
                    int id = (j * m + l) / 2;
                    float weight = 0.0f;
                    if ((j * m + l) % 2) {
                        weight = configs[j].invQuantization(weightData[id] & 0xF);
                    } else {
                        weight = configs[j].invQuantization(weightData[id] >> 4);
                    }
                    now += inputData[i * m + l] * weight;
                }

                outputData[i * k + j] = now;
            }
        }
    }

    //a = [n, m], b = [k, m], c = aT(b') = [n, k]
    void Multiply(uint8_t *a, uint8_t *b, int32_t *c, int n, int m, int k, int kstride) {
#ifdef __ARM_FEATURE_DOTPROD
        int block = 0;
        for (; block < n; block++) {
            uint8_t *weightWalk = b;
            uint8_t *inputStart = a + block * m;

            for (int i = 0; i < k; i++) {
                int value = 0;
                uint8_t *inputWalk = inputStart;
                int j = 0;
                uint32x4_t sum0 = {0, 0, 0, 0};
                for (; j + 31 < m; j += 32) {
                    uint8x16_t vi = vld1q_u8(inputWalk);
                    uint8x16_t vi0 = vld1q_u8(inputWalk + 16);
                    uint8x16_t vw = vld1q_u8(weightWalk);
                    uint8x16_t vw0 = vld1q_u8(weightWalk + 16);
                    sum0 = vdotq_u32(sum0, vi, vw);
                    sum0 = vdotq_u32(sum0, vi0, vw0);
                    inputWalk += 32;
                    weightWalk += 32;
                }

                value += sum0[0] + sum0[1] + sum0[2] + sum0[3];
                for (; j < m; j++) {
				    value += (int)(*(weightWalk++)) * (*(inputWalk++));
			    }
                c[block * kstride + i] = value;
            }
        }
#elif defined(__aarch64__)
        int block = 0;
        for (; block < n; block++) {
            uint8_t *weightWalk = b;
            uint8_t *inputStart = a + block * m;

            for (int i = 0; i < k; i++) {
                int value = 0;
                uint8_t *inputWalk = inputStart;

                int per = 64;
                int cnt = m / per;
                int sur = m % per;

                uint32x4_t sum = {0};
                uint16x8_t temp = {0};
                uint16x8_t temp1 = {0};
                uint16x8_t temp2 = {0};
                uint16x8_t temp3 = {0};
                uint16x8_t temp4 = {0};
                uint16x8_t temp5 = {0};
                uint16x8_t temp6 = {0};
                uint16x8_t temp7 = {0};

                while (cnt--) {
                    temp = vmull_u8(vld1_u8(inputWalk), vld1_u8(weightWalk));
                    temp1 = vmull_u8(vld1_u8(inputWalk + 8), vld1_u8(weightWalk + 8));
                    temp2 = vmull_u8(vld1_u8(inputWalk + 16), vld1_u8(weightWalk + 16));
                    temp3 = vmull_u8(vld1_u8(inputWalk + 24), vld1_u8(weightWalk + 24));
                    temp4 = vmull_u8(vld1_u8(inputWalk + 32), vld1_u8(weightWalk + 32));
                    temp5 = vmull_u8(vld1_u8(inputWalk + 40), vld1_u8(weightWalk + 40));
                    temp6 = vmull_u8(vld1_u8(inputWalk + 48), vld1_u8(weightWalk + 48));
                    temp7 = vmull_u8(vld1_u8(inputWalk + 56), vld1_u8(weightWalk + 56));

                    sum = vpadalq_u16(sum, temp);
                    sum = vpadalq_u16(sum, temp1);
                    sum = vpadalq_u16(sum, temp2);
                    sum = vpadalq_u16(sum, temp3);
                    sum = vpadalq_u16(sum, temp4);
                    sum = vpadalq_u16(sum, temp5);
                    sum = vpadalq_u16(sum, temp6);
                    sum = vpadalq_u16(sum, temp7);

                    inputWalk += per;
                    weightWalk += per;
                }

                value += (sum[0] + sum[1] + sum[2] + sum[3]);
                while (sur--) {
                    value += (int)(*(weightWalk++)) * (*(inputWalk++));
                }

                c[block * kstride + i] = value;
            }
        }
#elif defined(__AVX2__)
        int block = 0;
        for (; block < n; block++) {
            uint8_t *weightWalk = b;
            uint8_t *inputStart = a + block * m;

            for (int i = 0; i < k; i++) {
                uint8_t *inputWalk = inputStart;

                c[block * kstride + i] = DotU8U8(inputWalk, weightWalk, m);
                weightWalk += m;
            }
        }
#else
        int block = 0;
	    for (; block < n; block++) {
		    uint8_t *weightWalk = b;
		    uint8_t *inputStart = a + block * m;

		    for (int i = 0; i < k; i++) {
			    int value = 0;
			    uint8_t *inputWalk = inputStart;
			    for (int j = 0; j < m; j++) {
				    value += (int)(*(weightWalk++)) * (*(inputWalk++));
			    }

			    c[block * kstride + i] = value;
		    }
	    }
#endif
    }

    //a = [n, m], b = [k, m], c = aT(b') = [n, k]
    void MultiplyInt4(uint8_t *a, uint8_t *b, int32_t *c, int n, int m, int k, int kstride,
                      int *weightSums, int *weightZeros, float *scales, float *bias, LowBitConfig *config,
                      int *inputSums) {
        int block = 0;
        for (; block < n; block++) {
            uint32_t inputSum = inputSums[block];
            uint8_t *weightWalk = b;
            uint8_t *inputStart = a + block * m;

            for (int i = 0; i < k; i++) {
                int value = 0;
                uint8_t *inputWalk = inputStart;
                int j = 0;
#ifdef __ARM_FEATURE_DOTPROD
                uint8x8_t maskHigh = vdup_n_u8(0xF0);
                uint8x8_t maskLow = vdup_n_u8(0xF);
                uint32x2_t sum0 = {0, 0};

                for (; j + 15 < m; j += 16) {
                    uint8x8_t ori = vld1_u8(weightWalk + (i * m + j) / 2);
                    uint8x8x2_t in = vld2_u8(inputWalk + j);
                    uint8x8_t va = vand_u8(ori, maskLow);
                    uint8x8_t vb = vshr_n_u8(vand_u8(ori, maskHigh), 4);
                    sum0 = vdot_u32(sum0, va, in.val[1]);
                    sum0 = vdot_u32(sum0, vb, in.val[0]);
                }
                value += sum0[0] + sum0[1];
#elif defined(__aarch64__)
                uint8x8_t maskHigh = vdup_n_u8(0xF0);
                uint8x8_t maskLow = vdup_n_u8(0xF);
                uint32x4_t sum0 = {0, 0, 0, 0};

                for (; j + 15 < m; j += 16) {
                    uint8x8_t ori = vld1_u8(weightWalk + (i * m + j) / 2);
                    uint8x8x2_t in = vld2_u8(inputWalk + j);
                    uint8x8_t va = vand_u8(ori, maskLow);
                    uint8x8_t vb = vshr_n_u8(vand_u8(ori, maskHigh), 4);
                    sum0 = vpadalq_u16(sum0, vmull_u8(va, in.val[1]));
                    sum0 = vpadalq_u16(sum0, vmull_u8(vb, in.val[0]));
                }
                value += sum0[0] + sum0[1] + sum0[2] + sum0[3];
#elif defined(__AVX2__)
                value += DotU4U8(weightWalk + i * m / 2, inputWalk, m);
                j += m;
#endif
                for (; j + 1 < m; j += 2) {
                    int id = (i * m + j) / 2;
                    value += (weightWalk[id] >> 4) * inputWalk[j];
                    value += (weightWalk[id] & 0xF) * inputWalk[j + 1];
                }

                for (; j < m; j++) {
                    int id = (i * m + j) / 2;
                    if ((i * m + j) % 2) {
                        value += (weightWalk[id] & 0xF) * inputWalk[j];
                    } else {
                        value += (weightWalk[id] >> 4) * inputWalk[j];
                    }
                }

                value -= weightSums[i] * config[block].zeroPoint;
                value -= inputSum * weightZeros[i];
                value += (int)config[block].zeroPoint * weightZeros[i] * m;

                ((float*)c)[block * kstride + i] = scales[i] * config[block].scale * value +
                                                   (bias == nullptr ? 0.0 : bias[i]);
            }
        }
    }

    //a = [n, m], b = [k, m], c = aT(b') = [n, k]
    void MultiplyInt4NoZero(uint8_t *a, uint8_t *b, int32_t *c, int n, int m, int k, int kstride,
                      int *weightSums, float *weightMins, float *scales, float *bias, LowBitConfig *config,
                      int *inputSums) {
        int block = 0;
        for (; block < n; block++) {
            uint32_t inputSum = inputSums[block];
            uint8_t *weightWalk = b;
            uint8_t *inputStart = a + block * m;

            for (int i = 0; i < k; i++) {
                int value = 0;
                uint8_t *inputWalk = inputStart;
                int j = 0;
#ifdef __ARM_FEATURE_DOTPROD
                uint8x8_t maskHigh = vdup_n_u8(0xF0);
                uint8x8_t maskLow = vdup_n_u8(0xF);
                uint32x2_t sum0 = {0, 0};

                for (; j + 15 < m; j += 16) {
                    uint8x8_t ori = vld1_u8(weightWalk + (i * m + j) / 2);
                    uint8x8x2_t in = vld2_u8(inputWalk + j);
                    uint8x8_t va = vand_u8(ori, maskLow);
                    uint8x8_t vb = vshr_n_u8(vand_u8(ori, maskHigh), 4);
                    sum0 = vdot_u32(sum0, va, in.val[1]);
                    sum0 = vdot_u32(sum0, vb, in.val[0]);
                }
                value += sum0[0] + sum0[1];
#elif defined(__aarch64__)
                uint8x8_t maskHigh = vdup_n_u8(0xF0);
                uint8x8_t maskLow = vdup_n_u8(0xF);
                uint32x4_t sum0 = {0, 0, 0, 0};

                for (; j + 15 < m; j += 16) {
                    uint8x8_t ori = vld1_u8(weightWalk + (i * m + j) / 2);
                    uint8x8x2_t in = vld2_u8(inputWalk + j);
                    uint8x8_t va = vand_u8(ori, maskLow);
                    uint8x8_t vb = vshr_n_u8(vand_u8(ori, maskHigh), 4);
                    sum0 = vpadalq_u16(sum0, vmull_u8(va, in.val[1]));
                    sum0 = vpadalq_u16(sum0, vmull_u8(vb, in.val[0]));
                }
                value += sum0[0] + sum0[1] + sum0[2] + sum0[3];
#elif defined(__AVX2__)
                value += DotU4U8(weightWalk + i * m / 2, inputWalk, m);
                j += m;
#endif

                for (; j + 1 < m; j += 2) {
                    int id = (i * m + j) / 2;
                    value += (weightWalk[id] >> 4) * inputWalk[j];
                    value += (weightWalk[id] & 0xF) * inputWalk[j + 1];
                }

                value -= weightSums[i] * config[block].zeroPoint;
                ((float*)c)[block * kstride + i] = scales[i] * config[block].scale * value +
                        weightMins[i] * ((float)inputSum - (int)config[block].zeroPoint * m) * config[block].scale +
                        (bias == nullptr ? 0.0 : bias[i]);
            }
        }
    }

    //a = [n, m], b = [k, m], c = aT(b') = [n, k]
    void MultiplyMultiThread(uint8_t *a, uint8_t *b, int32_t *c, int n, int m, int k, int threadNum) {
        int per = k / threadNum;
        int cur = 0;
        if (threadNum == 1) {
            Multiply(a, b + cur * m, c + cur, n, m, k - cur, k);
        } else {
            auto pool = GetPool();
            std::vector<std::future<void> > futures;
            for (int i = 0; i < threadNum; i++) {
                int end = cur + per + (cur + per * (threadNum - i) < k);
                if (i == threadNum - 1) {
                    end = k;
                }
                futures.push_back(pool->Submit(Multiply, a, b + cur * m, c + cur, n, m, end - cur, k));
                cur = end;
            }
            for (int i = 0; i < futures.size(); i++) {
                futures[i].get();
            }
        }
    }

    //a = [n, m], b = [k, m], c = aT(b') = [n, k]
    void MultiplyInt4MultiThread(uint8_t *a, uint8_t *b, int32_t *c, int n, int m, int k,
                                 int *weightSums, int *weightZeros, float *scales, float *bias, std::vector <LowBitConfig> &configs, int threadNum) {
        std::vector <int> inputSums;
        for (int i = 0; i < n; i++) {
            int sum = 0;
            for (int j = 0; j < m; j++) {
                sum += a[i * m + j];
            }
            inputSums.push_back(sum);
        }
        int per = k / threadNum;
        int cur = 0;
        if (threadNum == 1) {
            MultiplyInt4(a, b + cur * m / 2, c + cur, n, m, k - cur, k,
                         weightSums + cur, weightZeros + cur, scales + cur,
                         (bias == nullptr ? (float*)nullptr : bias + cur), configs.data(), inputSums.data());
        } else {
            auto pool = GetPool();
            std::vector<std::future<void> > futures;
            for (int i = 0; i < threadNum; i++) {
                int end = (i == threadNum - 1 ? k : cur + per + (cur + per * (threadNum - i) < k));
                futures.push_back(pool->Submit(MultiplyInt4, a, b + cur * m / 2, c + cur, n, m, end - cur, k,
                                               weightSums + cur, weightZeros + cur, scales + cur,
                                               (bias == nullptr ? (float *) nullptr : bias + cur), configs.data(),
                                               inputSums.data()));
                cur = end;
            }
            for (int i = 0; i < futures.size(); i++) {
                futures[i].get();
            }
        }
    }

    //a = [n, m], b = [k, m], c = aT(b') = [n, k]
    void MultiplyInt4NoZeroMultiThread(uint8_t *a, uint8_t *b, int32_t *c, int n, int m, int k,
                                 int *weightSums, float *weightMins, float *scales, float *bias,
                                 std::vector <LowBitConfig> &configs, int threadNum) {
        std::vector <int> inputSums;
        for (int i = 0; i < n; i++) {
            int sum = 0;
            for (int j = 0; j < m; j++) {
                sum += a[i * m + j];
            }
            inputSums.push_back(sum);
        }
        int per = k / threadNum;
        int cur = 0;
        if (threadNum == 1) {
            MultiplyInt4NoZero(a, b + cur * m / 2, c + cur, n, m, k - cur, k,
                         weightSums + cur, weightMins + cur, scales + cur,
                         (bias == nullptr ? (float*)nullptr : bias + cur), configs.data(), inputSums.data());
        } else {
            auto pool = GetPool();
            std::vector<std::future<void> > futures;
            for (int i = 0; i < threadNum; i++) {
                int end = (i == threadNum - 1 ? k : cur + per + (cur + per * (threadNum - i) < k));
                futures.push_back(pool->Submit(MultiplyInt4NoZero, a, b + cur * m / 2, c + cur, n, m, end - cur, k,
                                               weightSums + cur, weightMins + cur, scales + cur,
                                               (bias == nullptr ? (float *) nullptr : bias + cur), configs.data(),
                                               inputSums.data()));
                cur = end;
            }
            for (int i = 0; i < futures.size(); i++) {
                futures[i].get();
            }
        }
    }

    void CpuLinearOp::Run(const std::string &opType, const fastllm::DataDict &datas,
                          const fastllm::FloatDict &floatParams, const fastllm::IntDict &intParams) {
//auto st = std::chrono::system_clock::now();
        Data &input = *(datas.find("input")->second);
        Data &output = *(datas.find("output")->second);
        Data &weight = *(datas.find("weight")->second);
        Data &bias = *(datas.find("bias")->second);

        output.Allocate(0.0f);
        int n = input.Count(0) / input.dims.back();
        int m = input.dims.back();
        int k = output.dims.back();

        if (input.dataType == DataType::FLOAT32 && output.dataType == DataType::FLOAT32) {
            if (weight.dataType == DataType::FLOAT32) {
                float *inputData = (float *) input.cpuData;
                float *weightData = (float *) weight.cpuData;
                float *outputData = (float *) output.cpuData;
                float *biasData = bias.dims.size() > 0 ? (float *) bias.cpuData : nullptr;

                int threadNum = GetThreads();
                int per = k / threadNum;
                int cur = 0;
                auto pool = GetPool();
                std::vector<std::future<void> > futures;
                for (int i = 0; i < threadNum - 1; i++) {
                    int end = cur + per + (cur + per * (threadNum - i) < k);
                    futures.push_back(pool->Submit(FloatLinearPart, inputData, weightData, biasData, outputData,
                                                   n, m, k, cur, end));
                    cur = end;
                }

                FloatLinearPart(inputData, weightData, biasData, outputData, n, m, k, cur, k);
                for (int i = 0; i < futures.size(); i++) {
                    futures[i].get();
                }
            } else if (weight.dataType == DataType::FLOAT16) {
                float *inputData = (float *) input.cpuData;
                uint16_t *weightData = (uint16_t *) weight.cpuData;
                float *outputData = (float *) output.cpuData;
                float *biasData = bias.dims.size() > 0 ? (float *) bias.cpuData : nullptr;
#ifdef __ARM_FEATURE_FP16_VECTOR_ARITHMETIC
                uint16_t *temp = new uint16_t[n * m];
                for (int i = 0; i < n * m; i++) {
                    temp[i] = float_to_half(inputData[i]);
                }
                inputData = (float*)temp;
#endif
                int threadNum = GetThreads();
                int per = k / threadNum;
                int cur = 0;
                auto pool = GetPool();
                std::vector<std::future<void> > futures;
                for (int i = 0; i < threadNum - 1; i++) {
                    int end = cur + per + (cur + per * (threadNum - i) < k);
                    futures.push_back(pool->Submit(Float16LinearPart, inputData, weightData, biasData, outputData,
                                                   n, m, k, cur, end));
                    cur = end;
                }

                Float16LinearPart(inputData, weightData, biasData, outputData, n, m, k, cur, k);
                for (int i = 0; i < futures.size(); i++) {
                    futures[i].get();
                }
#ifdef __ARM_FEATURE_FP16_VECTOR_ARITHMETIC
                delete[] temp;
#endif
            } else if (weight.dataType == DataType::INT8) {
                float *inputData = (float *) input.cpuData;
                uint8_t *weightData = (uint8_t *) weight.cpuData;
                float *outputData = (float *) output.cpuData;
                float *biasData = bias.dims.size() > 0 ? (float *) bias.cpuData : nullptr;
                weight.CalcWeightSum();

                std::vector<LowBitConfig> inputConfigs;
                for (int i = 0; i < n; i++) {
                    float minValue = 1e9, maxValue = -1e9;
                    for (int j = 0; j < m; j++) {
                        minValue = std::min(minValue, inputData[i * m + j]);
                        maxValue = std::max(maxValue, inputData[i * m + j]);
                    }
                    inputConfigs.push_back(LowBitConfig(minValue, maxValue, 8, 0));
                }
                std::vector<uint8_t> uinput;
                uinput.resize(n * m);
                for (int i = 0; i < n * m; i++) {
#ifdef __AVX2__
                    uinput[i] = inputConfigs[i / m].quantization(inputData[i]);
                    uinput[i] = (uinput[i] + !uinput[i]) ^ 128;
#else
                    uinput[i] = inputConfigs[i / m].quantization(inputData[i]);
#endif
                }

                MultiplyMultiThread(uinput.data(), weightData, (int32_t *) outputData, n, m, k, GetThreads());
                for (int i = 0; i < n; i++) {
                    uint32_t inputSum = 0;
                    for (int j = 0; j < m; j++) {
#ifdef __AVX2__
                        inputSum += uinput[i * m + j] ^ 128;
#else
                        inputSum += uinput[i * m + j];
#endif
                    }

                    for (int j = 0; j < k; j++) {
                        int value = ((int32_t *) outputData)[i * k + j];
#ifdef __AVX2__
                        value += (128 * weight.weightSum[j]);
                        value += (128 * inputSum);
                        value -= m * 128 * 128;
#endif
                        value -= weight.weightSum[j] * inputConfigs[i].zeroPoint;
                        value -= inputSum * weight.perChannelsConfigs[j].zeroPoint;
                        value += (int) inputConfigs[i].zeroPoint * weight.perChannelsConfigs[j].zeroPoint * m;
                        outputData[i * k + j] = weight.perChannelsConfigs[j].scale * inputConfigs[i].scale * value +
                                                (biasData == nullptr ? 0.0 : biasData[j]);
                    }
                }

                /*
                这部分是float输入，float输出
                int threadNum = threads;
                int per = k / threadNum;
                int cur = 0;
                std::vector<std::thread *> threads;
                for (int i = 0; i < threadNum - 1; i++) {
                    int end = cur + per + (cur + per * (threadNum - i) < k);
                    threads.push_back(new std::thread(&Int8LinearPart, inputData, weightData, biasData, outputData,
                                                      weight.perChannelsConfigs.data(), n, m, k, cur, end));
                    cur = end;
                }
                Int8LinearPart(inputData, weightData, biasData, outputData, weight.perChannelsConfigs.data(), n, m, k, cur, k);
                for (int i = 0; i < threadNum - 1; i++) {
                    threads[i]->join();
                    delete threads[i];
                }
                */
            } else if (weight.dataType == DataType::INT4 || weight.dataType == DataType::INT4_NOZERO) {
                float *inputData = (float *) input.cpuData;
                uint8_t *weightData = (uint8_t *) weight.cpuData;
                float *outputData = (float *) output.cpuData;
                float *biasData = bias.dims.size() > 0 ? (float *) bias.cpuData : nullptr;
                weight.CalcWeightSum();

                std::vector<LowBitConfig> inputConfigs;
                for (int i = 0; i < n; i++) {
                    float minValue = 1e9, maxValue = -1e9;
                    for (int j = 0; j < m; j++) {
                        minValue = std::min(minValue, inputData[i * m + j]);
                        maxValue = std::max(maxValue, inputData[i * m + j]);
                    }
                    inputConfigs.push_back(LowBitConfig(minValue, maxValue, 8, 0));
                }
                std::vector<uint8_t> uinput;
                uinput.resize(n * m);
                for (int i = 0; i < n * m; i++) {
                    uinput[i] = inputConfigs[i / m].quantization(inputData[i]);
                }
#ifdef __AVX__
                uint8_t *temp = new uint8_t[32];
                for (int i = 0; i < n; i++) {
                    for (int j = 0; j + 31 < m; j += 32) {
                        memcpy(temp, uinput.data() + i * m + j, 32);
                        for (int k = 0; k < 16; k++) {
                            uinput[i * m + j + k] = temp[k * 2 + 1];
                            uinput[i * m + j + k + 16] = temp[k * 2];
                        }
                    }
                }
                delete[] temp;
#endif
                if (weight.dataType == DataType::INT4) {
                    MultiplyInt4MultiThread(uinput.data(), weightData, (int32_t *) outputData, n, m, k,
                                            weight.weightSum.data(), weight.zeros.data(), weight.scales.data(),
                                            biasData,
                                            inputConfigs, GetThreads());
                } else {
                    MultiplyInt4NoZeroMultiThread(uinput.data(), weightData, (int32_t *) outputData, n, m, k,
                                                  weight.weightSum.data(), weight.mins.data(), weight.scales.data(),
                                                  biasData,
                                                  inputConfigs, GetThreads());
                }

/*
            //这部分是float输入，float输出
            int threadNum = GetThreads();
            int per = k / threadNum;
            int cur = 0;
            std::vector<std::thread *> threads;
            for (int i = 0; i < threadNum - 1; i++) {
                int end = cur + per + (cur + per * (threadNum - i) < k);
                threads.push_back(new std::thread(&Int4LinearPart, inputData, weightData, biasData, outputData,
                                                  weight.perChannelsConfigs.data(), n, m, k, cur, end));
                cur = end;
            }
            Int4LinearPart(inputData, weightData, biasData, outputData, weight.perChannelsConfigs.data(), n, m, k, cur, k);
            for (int i = 0; i < threadNum - 1; i++) {
                threads[i]->join();
                delete threads[i];
            }
*/
            } else {
                ErrorInFastLLM("Linear error: unsupport weight's dataType.\n");
            }
        } else if (input.dataType == DataType::FLOAT16 && output.dataType == DataType::FLOAT16) {
            if (weight.dataType == DataType::FLOAT16) {
                uint16_t *inputData = (uint16_t *) input.cpuData;
                uint16_t *weightData = (uint16_t *) weight.cpuData;
                uint16_t *outputData = (uint16_t *) output.cpuData;
                float *biasData = bias.dims.size() > 0 ? (float *) bias.cpuData : nullptr;
                int threadNum = GetThreads();
                int per = k / threadNum;
                int cur = 0;
                auto pool = GetPool();
                std::vector<std::future<void> > futures;
                for (int i = 0; i < threadNum - 1; i++) {
                    int end = cur + per + (cur + per * (threadNum - i) < k);
                    futures.push_back(pool->Submit(Float16xFloat16LinearPart, inputData, weightData, biasData, outputData,
                                                   n, m, k, cur, end));
                    cur = end;
                }

                Float16xFloat16LinearPart(inputData, weightData, biasData, outputData, n, m, k, cur, k);
                for (int i = 0; i < futures.size(); i++) {
                    futures[i].get();
                }
            } else {
                ErrorInFastLLM("Linear error: unsupport weight's dataType.\n");
            }
        } else {
            ErrorInFastLLM("Linear error: unsupport weight's dataType.\n");
        }
//float spend = GetSpan(st, std::chrono::system_clock::now());
//float gops = (float)n * m * k / spend / 1e9;
// printf("n = %d, m = %d, k = %d, spend %f s, gops = %f\n", n, m, k, spend, gops);
    }

    long long int CpuLinearOp::Ops(const std::string &opType, const fastllm::DataDict &datas,
                                   const fastllm::FloatDict &floatParams, const fastllm::IntDict &intParams) {
        Data &input = *(datas.find("input")->second);
        Data &output = *(datas.find("output")->second);
        Data &weight = *(datas.find("weight")->second);

        int m = input.dims.back();
        int k = output.dims.back();
        int n = input.Count(0) / m;

        return (long long int) n * m * k;
    }

    inline bool is_a_ge_zero_and_a_lt_b(int a, int b) {
        return static_cast<unsigned>(a) < static_cast<unsigned>(b);
    }

    template <typename T>
    void Im2colPart(T *image, T *col, int inputChannel, int inputHeight, int inputWidth, int outputHeight, int outputWidth,
                    int kernel, int stride, int padding, int dialtion, bool rev = false) {
        int input_row = 0, input_col = 0, current_row = 0, current_col = 0;
        T *current_im;

        if (rev) {
            for (int output_rows = outputHeight; output_rows; output_rows--) {
                input_col = 0;
                for (int output_cols = outputWidth; output_cols; output_cols--) {
                    current_im = image;
                    for (int channel = inputChannel; channel--;) {
                        current_row = input_row - padding;
                        for (int kernel_row = 0; kernel_row < kernel; kernel_row++) {
                            current_col = input_col - padding;
                            for (int kernel_col = 0; kernel_col < kernel; kernel_col++) {
                                if (!is_a_ge_zero_and_a_lt_b(current_row, inputHeight) ||
                                    !is_a_ge_zero_and_a_lt_b(current_col, inputWidth)) {
                                    *(col++) = (T) 0;
                                } else {
                                    *(col++) = current_im[current_row * inputWidth + current_col];
                                }
                                current_col += dialtion;
                            }
                            current_row += dialtion;
                        }
                        current_im += inputHeight * inputWidth;
                    }
                    input_col += stride;
                }
                input_row += stride;
            }
        } else {
            // optimize for 1x1, s1
            if (kernel == 1 && padding == 0 && stride == 1 && dialtion == 1) {
                memcpy(col, image, sizeof(T) * inputChannel * inputHeight * inputWidth);
            }
            // optimize for 3x3, s1
            else if (kernel == 3 && padding == 1 && stride == 1 && dialtion == 1 && inputHeight * inputWidth > 1) {
                int outputChannelSize = outputHeight * outputWidth;
                std::fill(col, col + inputChannel * kernel * kernel * outputHeight * outputWidth, (T) 0);
                for (int c = 0; c < inputChannel; c++) {
                    T *cols[9];
                    cols[0] = col;
                    cols[1] = col + outputChannelSize;
                    cols[2] = col + 2 * outputChannelSize;
                    cols[3] = col + 3 * outputChannelSize;
                    cols[4] = col + 4 * outputChannelSize;
                    cols[5] = col + 5 * outputChannelSize;
                    cols[6] = col + 6 * outputChannelSize;
                    cols[7] = col + 7 * outputChannelSize;
                    cols[8] = col + 8 * outputChannelSize;
                    //height:1
                    //kh 0
                    cols[0] += inputWidth;
                    cols[1] += inputWidth;
                    cols[2] += inputWidth;
                    //kw0
                    memcpy(cols[0] + 1, image, sizeof(T) * (inputWidth - 1));
                    cols[0] += inputWidth;
                    //kw1
                    memcpy(cols[1], image, sizeof(T) * inputWidth);
                    cols[1] += inputWidth;
                    //kw2
                    memcpy(cols[2], image + 1, sizeof(T) * (inputWidth - 1));
                    cols[2] += inputWidth;
                    //kh1
                    //kw0
                    memcpy(cols[3] + 1, image, sizeof(T) * (inputWidth - 1));
                    cols[3] += inputWidth;
                    //kw1
                    memcpy(cols[4], image, sizeof(T) * inputWidth);
                    cols[4] += inputWidth;
                    //kw2
                    memcpy(cols[5], image + 1, sizeof(T) * (inputWidth - 1));
                    cols[5] += inputWidth;
                    image += inputWidth;
                    // 1~(hight-1)
                    for (int h = 1; h < inputHeight - 1; h++) {
                        T **cols_ptr = cols;
                        for (int kernel_row = 0; kernel_row < kernel; kernel_row++) {
                            memcpy(cols_ptr[0] + 1, image, sizeof(T) * (inputWidth - 1));
                            cols_ptr[0] += inputWidth;
                            //kw1
                            memcpy(cols_ptr[1], image, sizeof(T) * inputWidth);
                            cols_ptr[1] += inputWidth;
                            //kw2
                            memcpy(cols_ptr[2], image + 1, sizeof(T) * (inputWidth - 1));
                            cols_ptr[2] += inputWidth;
                            cols_ptr += 3;
                        }
                        image += inputWidth;
                    }
                    //hight
                    //kh1
                    //kw0
                    memcpy(cols[3] + 1, image, sizeof(T) * (inputWidth - 1));
                    cols[3] += inputWidth;
                    //kw1
                    memcpy(cols[4], image, sizeof(T) * inputWidth);
                    cols[4] += inputWidth;
                    //kw2
                    memcpy(cols[5], image+1, sizeof(T) * (inputWidth - 1));
                    cols[5] += inputWidth;
                    //kh2
                    //kw0
                    memcpy(cols[6] + 1, image, sizeof(T) * (inputWidth - 1));
                    cols[6] += inputWidth;
                    //kw1
                    memcpy(cols[7], image, sizeof(T) * inputWidth);
                    cols[7] += inputWidth;
                    //kw2
                    memcpy(cols[8], image + 1, sizeof(T) * (inputWidth - 1));
                    cols[8] += inputWidth;

                    col += outputChannelSize * kernel * kernel;
                    image += inputWidth;
                }
            } else {
                for (int channel = inputChannel; channel--; image += inputHeight * inputWidth) {
                    for (int kernel_row = 0; kernel_row < kernel; kernel_row++) {
                        for (int kernel_col = 0; kernel_col < kernel; kernel_col++) {
                            int input_row = -padding + kernel_row * dialtion;
                            for (int output_rows = outputHeight; output_rows; output_rows--) {
                                if (!is_a_ge_zero_and_a_lt_b(input_row, inputHeight)) {
                                    std::fill(col, col + outputWidth, (T) 0);
                                    col += outputWidth;
                                } else {
                                    if (stride == 1) {
                                        int input_col = -padding + kernel_col * dialtion, cur = outputWidth;
                                        if (input_col < 0) {
                                            std::fill(col, col - input_col, (T) 0);
                                            cur += input_col;
                                            col -= input_col;
                                            input_col = 0;
                                        }

                                        int cpyNumbers = std::min(cur, inputWidth - input_col);
                                        memcpy(col, image + input_row * inputWidth + input_col, cpyNumbers * sizeof(T));
                                        col += cpyNumbers;
                                        cur -= cpyNumbers;
                                        if (cur > 0) {
                                            std::fill(col, col + cur, (T) 0);
                                            col += cur;
                                            cur = 0;
                                        }
                                    } else {
                                        int input_col = -padding + kernel_col * dialtion;
                                        for (int output_col = outputWidth; output_col; output_col--) {
                                            if (is_a_ge_zero_and_a_lt_b(input_col, inputWidth)) {
                                                *(col++) = image[input_row * inputWidth + input_col];
                                            } else {
                                                *(col++) = (T) 0;
                                            }
                                            input_col += stride;
                                        }
                                    }
                                }
                                input_row += stride;
                            }
                        }
                    }
                }
            }
        }
    }

    void CpuConv2dOp::Reshape(const std::string &opType, const fastllm::DataDict &datas,
                              const fastllm::FloatDict &floatParams, const fastllm::IntDict &intParams) {
        Data &input = *(datas.find("input")->second);
        Data &output = *(datas.find("output")->second);
        Data &weight = *(datas.find("weight")->second);

        AssertInFastLLM(weight.dims[2] == weight.dims[3], "Fastllm dosen`t support kernel_h != kernel_w");

        int stride = intParams.find("stride")->second;
        int padding = intParams.find("padding")->second;
        int dilation = intParams.find("dilation")->second;
        int kernel = weight.dims[2];
        int inputChannel = input.dims[1];
        int inputHeight = input.dims[2];
        int inputWidth = input.dims[3];

        int batch = input.dims[0];
        int outputChannel = weight.dims[0];
        int outputHeight = ((inputHeight + padding * 2) - (dilation * (kernel - 1) + 1)) / stride + 1;
        int outputWidth = ((inputWidth + padding * 2) - (dilation * (kernel - 1) + 1)) / stride + 1;

        output.dataType = input.dataType;
        output.Resize({batch, outputChannel, outputHeight, outputWidth});
    }

    void CpuConv2dOp::Run(const std::string &opType, const fastllm::DataDict &datas,
                          const fastllm::FloatDict &floatParams, const fastllm::IntDict &intParams) {
        Data &input = *(datas.find("input")->second);
        Data &output = *(datas.find("output")->second);
        Data &weight = *(datas.find("weight")->second);
        Data &bias = *(datas.find("bias")->second);

        int kernel = weight.dims[2];
        int stride = intParams.find("stride")->second;
        int padding = intParams.find("padding")->second;
        int dilation = intParams.find("dilation")->second;
        int group = intParams.find("group")->second;

        int batch = input.dims[0];
        int inputChannel = input.dims[1];
        int inputHeight = input.dims[2];
        int inputWidth = input.dims[3];
        int outputChannel = output.dims[1];
        int outputHeight = output.dims[2];
        int outputWidth = output.dims[3];

        output.Allocate();
        auto pool = GetPool();
        std::vector<std::future<void> > futures;
        int threadNum = GetThreads();

        int n = outputChannel / group;
        int m = inputChannel * kernel * kernel / group;
        int k = outputHeight * outputWidth;
        int colOffset =  m * k;
        int weightOffset = n * m;
        int outputOffset = n * k;

        if (input.dataType == DataType::FLOAT32 && output.dataType == DataType::FLOAT32) {
            float *inputData = (float *) input.cpuData;
            float *outputData = (float *) output.cpuData;
            float *biasData = bias.dims.size() > 0 ? (float *) bias.cpuData : nullptr;

            float *col = new float[batch * inputChannel * kernel * kernel * outputHeight * outputWidth];
            // 1. im2col 
            // 此处 batch * inputChannel 合并为一个channel维度做im2col
            int channels = batch * inputChannel;
            int per = channels / threadNum;
            if (per > 0) {
                for (int i = 0; i < threadNum - 1; i++) {
                    float *curImage = inputData + i * per * inputHeight * inputWidth;
                    float *curCol = col + i * per * kernel * kernel * outputHeight * outputWidth;
                    futures.push_back(pool->Submit(Im2colPart<float>, curImage, curCol, per, 
                        inputHeight, inputWidth, outputHeight, outputWidth, kernel, stride, padding, dilation, false));
                }
            }
            int last = channels - (threadNum - 1) * per;
            float *curImage = inputData + (threadNum - 1) * per * inputHeight * inputWidth;
            float *curCol = col + (threadNum - 1) * per * kernel * kernel * outputHeight * outputWidth;
            Im2colPart(curImage, curCol, last, inputHeight, inputWidth, outputHeight, outputWidth, 
                       kernel, stride, padding, dilation, false);
            for (int i = 0; i < futures.size(); i++) {
                futures[i].get();
            }
            futures.clear();

            // 2. gemm
            for (int b = 0; b < batch; b++) {
                if (weight.dataType == DataType::FLOAT32) {
                    float *weightData = (float *) weight.cpuData;
                    for (int g = 0; g < group; g++) {
                        float *colWalk = col + g * colOffset;
                        float *weightWalk = weightData + g * weightOffset;
                        float *outputWalk = outputData + g * outputOffset;

                        // transpose col from [m, k] to [k, m]
                        float *colTrans = new float[k * m];
                        Transpose(colTrans, colWalk, m, k, m, k);

                        int per = k / threadNum;
                        int cur = 0;
                        for (int i = 0; i < threadNum - 1; i++) {
                            int end = cur + per + (cur + per * (threadNum - i) < k);
                            futures.push_back(pool->Submit(FloatLinearPart, weightWalk, colTrans, nullptr, outputWalk,
                                                           n, m, k, cur, end));
                            cur = end;
                        }
                        FloatLinearPart(weightWalk, colTrans, nullptr, outputWalk, n, m, k, cur, k);
                        for (int i = 0; i < futures.size(); i++) {
                            futures[i].get();
                        }
                        futures.clear();

                        delete[] colTrans;
                    }
                } else if (weight.dataType == DataType::FLOAT16) {
                    uint16_t *weightData = (uint16_t *) weight.cpuData;
                    for (int g = 0; g < group; g++) {
                        float *colWalk = col + g * colOffset;
                        uint16_t *weightWalk = weightData + g * weightOffset;
                        float *outputWalk = outputData + g * outputOffset;

                        // transpose col from [m, k] to [k, m]
                        float *colTrans = new float[k * m];
                        Transpose(colTrans, colWalk, m, k, m, k);

#ifdef __ARM_FEATURE_FP16_VECTOR_ARITHMETIC
                        uint16_t *temp = new uint16_t[m * k];
                        for (int i = 0; i < m * k; i++) {
                            temp[i] = float_to_half(colTrans[i]);
                        }

                        int per = k / threadNum;
                        int cur = 0;
                        std::vector<std::future<void> > futures;
                        for (int i = 0; i < threadNum - 1; i++) {
                            int end = cur + per + (cur + per * (threadNum - i) < k);
                            futures.push_back(pool->Submit(Float16LinearPart, (float *) weightWalk, temp, nullptr, outputWalk,
                                                        n, m, k, cur, end));
                            cur = end;
                        }
                        Float16LinearPart((float *) weightWalk, temp, nullptr, outputWalk, n, m, k, cur, k);
                        for (int i = 0; i < futures.size(); i++) {
                            futures[i].get();
                        }
                        futures.clear();
                        delete[] temp;
#else
                        // todo
#endif
                        delete[] colTrans;
                    }
                } else if (weight.dataType == DataType::INT8) {
                    uint8_t *weightData = (uint8_t *) weight.cpuData;
                    weight.CalcWeightSum();
                    for (int g = 0; g < group; g++) {
                        float *colWalk = col + g * colOffset;
                        uint8_t *weightWalk = weightData + g * weightOffset;
                        float *outputWalk = outputData + g * outputOffset;

                        auto weightSum = std::vector<int>(weight.weightSum.begin() + g * n,
                            weight.weightSum.begin() + g * n + n);
                        auto weightConfig = std::vector<LowBitConfig>(weight.perChannelsConfigs.begin() + g * n,
                            weight.perChannelsConfigs.begin() + g * n + n);
                        
                        // transpose col from [m, k] to [k, m]
                        float *colTrans = new float[k * m];
                        Transpose(colTrans, colWalk, m, k, m, k);

                        float minValue = 1e9, maxValue = -1e9;
                        for (int i = 0; i < k * m; i++) {
                            minValue = std::min(minValue, colTrans[i]);
                            maxValue = std::max(maxValue, colTrans[i]);
                        }
                        auto inputConfig = LowBitConfig(minValue, maxValue, 8, 0);
                        std::vector<uint8_t> uinput;
                        uinput.resize(k * m);

                        for (int i = 0; i < k * m; i++) {
#ifdef __AVX2__
                            uinput[i] = inputConfig.quantization(colTrans[i]);
                            uinput[i] = (uinput[i] + !uinput[i]) ^ 128;
#else
                            uinput[i] = inputConfig.quantization(colTrans[i]);
#endif
                        }
                        delete[] colTrans;

                        std::vector<uint32_t> inputSum(k, 0);
                        for (int i = 0; i < k; i++) {
                            for (int j = 0; j < m; j++) {
#ifdef __AVX2__
                                inputSum[i] += uinput[i * m + j] ^ 128;
#else
                                inputSum[i] += uinput[i * m + j];
#endif
                            }
                        }

                        MultiplyMultiThread(weightWalk, uinput.data(), (int32_t *) outputWalk, n, m, k, GetThreads());
                        for (int i = 0; i < n; i++) {
                            for (int j = 0; j < k; j++) {
                                int value = ((int32_t *) outputWalk)[i * k + j];
#ifdef __AVX2__
                                value += (128 * inputSum[j]);
                                value += (128 * weightSum[i]);
                                value -= m * 128 * 128;
#endif
                                value -= inputSum[j] * weightConfig[i].zeroPoint;
                                value -= weightSum[i] * inputConfig.zeroPoint;
                                value += (int) weightConfig[i].zeroPoint * inputConfig.zeroPoint * m;
                                outputWalk[i * k + j] = inputConfig.scale * weightConfig[i].scale * value;
                            }
                        }
                    }
                } else {
                    ErrorInFastLLM("Unsupported conv weight data type.\n");
                }

                col += inputChannel * kernel * kernel * outputHeight * outputWidth;
                outputData += outputChannel * outputHeight * outputWidth;
            }
            
            // clear data
            col -= batch * inputChannel * kernel * kernel * outputHeight * outputWidth;
            delete[] col;

            // 3. add bias
            if (biasData) {
                outputData = (float *) output.cpuData;
                for (int b = 0; b < batch; b++) {
                    for (int c = 0; c < outputChannel; c++) {
                        for (int i = 0; i < outputHeight * outputWidth; i++) {
                            outputData[i] += biasData[c];
                        }
                        outputData += outputHeight * outputWidth;
                    }
                }
            }
        } else if (input.dataType == DataType::FLOAT16 && output.dataType == DataType::FLOAT16) {
            // todo
            uint16_t *inputData = (uint16_t *) input.cpuData;
            uint16_t *weightData = (uint16_t *) weight.cpuData;
            uint16_t *outputData = (uint16_t *) output.cpuData;
            float *biasData = bias.dims.size() > 0 ? (float *) bias.cpuData : nullptr;
        } else {
            
        }
    }

    long long int CpuConv2dOp::Ops(const std::string &opType, const fastllm::DataDict &datas,
                                   const fastllm::FloatDict &floatParams, const fastllm::IntDict &intParams) {
        Data &input = *(datas.find("input")->second);
        Data &output = *(datas.find("output")->second);
        Data &weight = *(datas.find("weight")->second);

        int kernel = weight.dims[2];
        int group = intParams.find("group")->second;

        int batch = input.dims[0];
        int inputChannel = input.dims[1];
        int outputChannel = output.dims[1];
        int outputHeight = output.dims[2];
        int outputWidth = output.dims[3];
        return (long long int) batch * outputChannel * inputChannel * outputHeight * outputWidth * kernel * kernel / group * 2;
    }

    void CpuInterpolateOp::Reshape(const std::string &opType, const DataDict &datas, 
                                   const FloatDict &floatParams, const IntDict &intParams) {
        Data &input = *(datas.find("input")->second);
        Data &output = *(datas.find("output")->second);
        float scale = floatParams.find("scale") != floatParams.end() ? floatParams.find("scale")->second : 2.0;

        std::vector<int> dims = input.dims;
        dims[2] = (int) roundf(dims[2] * scale);
        dims[3] = (int) roundf(dims[3] * scale);

        output.dataType = input.dataType;
        output.Resize(dims);
    }

    void CpuInterpolateOp::Run(const std::string &opType, const DataDict &datas, 
                               const FloatDict &floatParams, const IntDict &intParams) {
        Data &input = *(datas.find("input")->second);
        Data &output = *(datas.find("output")->second);
        float scale = floatParams.find("scale") != floatParams.end() ? floatParams.find("scale")->second : 2.0;
        int mode = intParams.find("mode") != intParams.end() ? intParams.find("mode")->second : 0;
        int alignCorner = intParams.find("alignCorner") != intParams.end() ? intParams.find("alignCorner")->second : 0;

        output.Allocate();

        float *inputData = (float *) input.cpuData;
        float *outputData = (float *) output.cpuData;
        int channel = input.dims[0] * input.dims[1];
        int inputHeight = input.dims[2], inputWidth = input.dims[3];
        int outputHeight = output.dims[2], outputWidth = output.dims[3];

        if (mode == 0) {
            // bilinear
            for (int i = 0; i < channel; i++) {
                for (int h = 0; h < outputHeight; h++) {
                    float input_h = (float) h / scale;
                    int h0 = int(input_h);
                    int h1 = std::min(h0 + 1, inputHeight - 1);
                    for (int w = 0; w < outputWidth; w++) {
                        float input_w = (float) w / scale;
                        int w0 = int(input_w);
                        int w1 = std::min(w0 + 1, inputWidth - 1);

                        outputData[h * outputWidth + w] = 
                            inputData[h0 * inputWidth + w0] * (1 - (input_h - h0)) * (1 - (input_w - w0)) +
                            inputData[h1 * inputWidth + w0] * (input_h - h0) * (1 - (input_w - w0)) +
                            inputData[h0 * inputWidth + w1] * (1 - (input_h - h0)) * (input_w - w0) +
                            inputData[h1 * inputWidth + w1] * (input_h - h0) * (input_w - w0);
                    }
                }
                inputData += inputHeight * inputWidth;
                outputData += outputHeight * outputWidth;
            }
        } else if (mode == 1) {
            // nearest
            for (int i = 0; i < channel; i++) {
                for (int h = 0; h < outputHeight; h++) {
                    int in_h = std::min(alignCorner ? int(round(h / scale)) : int(h / scale), inputHeight - 1);
                    for (int w = 0; w < outputWidth; w++) {
                        int in_w = std::min(alignCorner ? int(round(w / scale)) : int(w / scale), inputWidth - 1);
                        (*outputData++) = inputData[in_h * inputWidth + in_w];
                    }
                }
                inputData += inputHeight * inputWidth;
            }
        } else {
            ErrorInFastLLM("Unssuported Interpolate type.\n");
        }

        output.Allocate();
    }

    void CpuSplitOp::Reshape(const std::string &opType, const fastllm::DataDict &datas,
                             const fastllm::FloatDict &floatParams, const fastllm::IntDict &intParams) {
        Data &input = *(datas.find("input")->second);
        Data &output = *(datas.find("output")->second);
        int axis = intParams.find("axis") != intParams.end() ? intParams.find("axis")->second : -1;
        int start = intParams.find("start") != intParams.end() ? intParams.find("start")->second : 0;
        int end = intParams.find("end") != intParams.end() ? intParams.find("end")->second : 0;

        int dimsLen = input.dims.size();
        axis = (axis % dimsLen + dimsLen) % dimsLen;

        start = std::max(0, std::min(input.dims[axis] - 1, start));
        end = std::max(0, std::min(input.dims[axis], end));
        std::vector <int> dims = input.dims;
        dims[axis] = end - start;

        output.dataType = input.dataType;
        output.Resize(dims);
    }

    void CpuSplitOp::Run(const std::string &opType, const fastllm::DataDict &datas,
                         const fastllm::FloatDict &floatParams, const fastllm::IntDict &intParams) {
        Data &input = *(datas.find("input")->second);
        Data &output = *(datas.find("output")->second);

        output.Allocate();

        int axis = intParams.find("axis") != intParams.end() ? intParams.find("axis")->second : -1;
        int start = intParams.find("start") != intParams.end() ? intParams.find("start")->second : 0;
        int end = intParams.find("end") != intParams.end() ? intParams.find("end")->second : 0;

        int dimsLen = input.dims.size();
        axis = (axis % dimsLen + dimsLen) % dimsLen;
        start = std::max(0, std::min(input.dims[axis] - 1, start));
        end = std::max(0, std::min(input.dims[axis], end));

        int outer = input.Count(0) / input.Count(axis);
        int inputStride = input.Count(axis);
        int outputStride = output.Count(axis);
        int channels = input.dims[axis];
        int inner = input.strides[axis];
        int unitSize = input.unitSize;

        for (int o = 0; o < outer; o++) {
            memcpy(output.cpuData + o * outputStride * unitSize,
                   input.cpuData + (o * inputStride + start * inner) * unitSize,
                   (end - start) * inner * unitSize);
        }
    }

    void CpuCatOp::Reshape(const std::string &opType, const fastllm::DataDict &datas,
                           const fastllm::FloatDict &floatParams, const fastllm::IntDict &intParams) {
        Data &input0 = *(datas.find("input0")->second);
        Data &input1 = *(datas.find("input1")->second);
        Data &output = *(datas.find("output")->second);

        int axis = intParams.find("axis") != intParams.end() ? intParams.find("axis")->second : -1;

        if (input0.dims.size() == 0 && input1.dims.size() > 0) {
            output.Resize(input1.dims);
            return;
        }
        if (input1.dims.size() == 0 && input0.dims.size() > 0) {
            output.Resize(input0.dims);
            return;
        }

        AssertInFastLLM((input0.dataType == DataType::FLOAT32 && input1.dataType == DataType::FLOAT32) ||
                        (input0.dataType == DataType::FLOAT16 && input1.dataType == DataType::FLOAT16),
                        "Cat's input's type should be float32.\n");
        AssertInFastLLM(input0.dims.size() == input1.dims.size(), "Cat Error: input's shape's size should be same.");

        int dimsLen = input0.dims.size();
        axis = (axis % dimsLen + dimsLen) % dimsLen;

        for (int i = 0; i < dimsLen; i++) {
            if (i != axis) {
                AssertInFastLLM(input0.dims[i] == input1.dims[i], "Cat Error: input's shape doesn't match.");
            }
        }

        std::vector <int> dims = input0.dims;
        dims[axis] += input1.dims[axis];

        output.dataType = input0.dataType;
        output.Resize(dims);
    }

    void CpuCatOp::Run(const std::string &opType, const fastllm::DataDict &datas,
                       const fastllm::FloatDict &floatParams, const fastllm::IntDict &intParams) {
        Data &input0 = *(datas.find("input0")->second);
        Data &input1 = *(datas.find("input1")->second);
        Data &output = *(datas.find("output")->second);

        output.Allocate();

        int axis = intParams.find("axis") != intParams.end() ? intParams.find("axis")->second : -1;
        if (input0.dims.size() == 0 && input1.dims.size() > 0) {
            output.CopyFrom(input1);
            return;
        }
        if (input1.dims.size() == 0 && input0.dims.size() > 0) {
            output.CopyFrom(input0);
            return;
        }

        int dimsLen = input0.dims.size();
        axis = (axis % dimsLen + dimsLen) % dimsLen;

        int outer = output.Count(0) / output.Count(axis);
        int input0Stride = input0.Count(axis);
        int input1Stride = input1.Count(axis);
        int outputStride = output.Count(axis);
        int inner = input0.strides[axis];
        int unitSize = input0.unitSize;

        for (int o = 0; o < outer; o++) {
            memcpy(output.cpuData + o * outputStride * unitSize,
                   input0.cpuData + (o * input0Stride) * unitSize,
                   input0.dims[axis] * inner * unitSize);
            memcpy(output.cpuData + o * outputStride * unitSize + input0.dims[axis] * inner * unitSize,
                   input1.cpuData + (o * input1Stride) * unitSize,
                   input1.dims[axis] * inner * unitSize);
        }
    }

    void CpuCatDirectOp::Run(const std::string &opType, const fastllm::DataDict &datas,
                             const fastllm::FloatDict &floatParams, const fastllm::IntDict &intParams) {
        Data &input0 = *(datas.find("input0")->second);
        Data &input1 = *(datas.find("input1")->second);

        int axis = intParams.find("axis") != intParams.end() ? intParams.find("axis")->second : -1;

        AssertInFastLLM((input0.dataType == DataType::FLOAT32 && input1.dataType == DataType::FLOAT32) ||
                        (input0.dataType == DataType::FLOAT16 && input1.dataType == DataType::FLOAT16),
                        "CatDirect's input's type should be float32.\n");
        AssertInFastLLM(input0.dataDevice == input1.dataDevice, "CatDirect error: inputs should use same device.\n");

        if (input0.dims.size() == 0) {
            input0.Resize(input1.dims);
            AssertInFastLLM(input0.expansionDims.size() == input1.dims.size() &&
                            input1.dims[axis] <= input0.expansionDims[axis],
                            "CatDirect Error: input0's expansion size is not enough.\n");
            int outer = input1.Count(0) / input1.Count(axis);
            int input0Stride = input0.Count(axis);
            int input1Stride = input1.Count(axis);
            int inner = input0.strides[axis];
            int unitSize = input0.unitSize;
            for (int o = 0; o < outer; o++) {
                memcpy(input0.cpuData + o * input0Stride * unitSize,
                       input1.cpuData + o * input1Stride * unitSize,
                       input1.dims[axis] * inner * unitSize);
            }

            return;
        }

        AssertInFastLLM(input0.dims.size() == input1.dims.size(), "Cat Error: input's shape's size should be same.\n");
        int dimsLen = input0.dims.size();
        axis = (axis % dimsLen + dimsLen) % dimsLen;

        for (int i = 0; i < dimsLen; i++) {
            if (i != axis) {
                AssertInFastLLM(input0.dims[i] == input1.dims[i], "Cat Error: input's shape doesn't match.");
            }
        }

        std::vector <int> dims = input0.dims;
        std::vector <int> oldDims = dims;
        dims[axis] += input1.dims[axis];
        input0.Resize(dims);
        int outer = input0.Count(0) / input0.Count(axis);
        int input0Stride = input0.Count(axis);
        int input1Stride = input1.Count(axis);

        int inner = input0.strides[axis];
        int unitSize = input0.unitSize;

        for (int o = 0; o < outer; o++) {
            memcpy(input0.cpuData + o * input0Stride * unitSize + oldDims[axis] * inner * unitSize,
                   input1.cpuData + (o * input1Stride) * unitSize,
                   input1.dims[axis] * inner * unitSize);
        }
    }

    void MatMulSingle(float *input0Base, float *input1Base, float *outputBase,
                      int input0Spatial, int input1Spatial, int outputSpatial,
                      int input0Stride, int input1Stride,
                      int n, int m, int k, float alpha, int st, int end) {
        for (int b = st; b < end; b++) {
            float *input0Data = input0Base + b * input0Spatial;
            float *input1Data = input1Base + b * input1Spatial;
            float *outputData = outputBase + b * outputSpatial;
            std::fill(outputData, outputData + n * k, 0.0f);
            for (int i = 0; i < n; i++) {
                for (int j = 0; j < m; j++) {
                    float now = input0Data[i * input0Stride + j] * alpha;
                    int l = 0;
#ifdef __aarch64__
                    float32x4_t nowx4 = vdupq_n_f32(now);
                    for (; l + 3 < k; l += 4) {
                        float32x4_t sumx4 = vaddq_f32(vld1q_f32(outputData + i * k + l), vmulq_f32(nowx4, vld1q_f32(input1Data + j * k + l)));
                        vst1q_f32(outputData + i * k + l, sumx4);
                    }
#endif
                    for (; l < k; l++) {
                        outputData[i * k + l] += (now * input1Data[j * k + l]);
                    }
                }
            }
        }
    }

    void MatMulFloat16Single(uint16_t *input0Base, uint16_t *input1Base, uint16_t *outputBase,
                             int input0Spatial, int input1Spatial, int outputSpatial,
                             int input0Stride, int input1Stride,
                             int n, int m, int k, float alpha, int st, int end) {
        float *input0 = new float[n * m];
        float *input1 = new float[m * k];
        float *output = new float[n * k];

        for (int b = st; b < end; b++) {
            uint16_t *input0Data = input0Base + b * input0Spatial;
            uint16_t *input1Data = input1Base + b * input1Spatial;
            uint16_t *outputData = outputBase + b * outputSpatial;
            for (int i = 0; i < n; i++) {
                for (int j = 0; j < m; j++) {
                    input0[i * m + j] = fp16tofp32.dict[input0Data[i * input0Stride + j]];
                }
            }
            for (int j = 0; j < m; j++) {
                for (int l = 0; l < k; l++) {
                    input1[j * k + l] = fp16tofp32.dict[input1Data[j * k + l]];
                }
            }
            std::fill(output, output + n * k, 0.0f);
            for (int i = 0; i < n; i++) {
                for (int j = 0; j < m; j++) {
                    float now = input0[i * m + j] * alpha;
                    for (int l = 0; l < k; l++) {
                        output[i * k + l] += (now * input1[j * k + l]);
                    }
                }
            }
            for (int i = 0; i < n * k; i++) {
                outputData[i] = float_to_half(output[i]);
            }
        }

        delete[] input0;
        delete[] input1;
        delete[] output;
    }

    void MatMulTransBSingle(float *input0Base, float *input1Base, float *outputBase,
                            int input0Spatial, int input1Spatial, int outputSpatial,
                            int input0Stride, int input1Stride,
                            int n, int m, int k, float alpha, int st, int end) {
        for (int b = st; b < end; b++) {
            float *input0Data = input0Base + b * input0Spatial;
            float *input1Data = input1Base + b * input1Spatial;
            float *outputData = outputBase + b * outputSpatial;
            for (int i = 0; i < n; i++) {
                for (int j = 0; j < k; j++) {
                    float now = 0.0f;
                    int l = 0;
#ifdef __aarch64__
                    float32x4_t sum = {0, 0, 0, 0};
                    for (; l + 3 < m; l += 4) {
                        sum = vaddq_f32(sum, vmulq_f32(vld1q_f32(input0Data + i * input0Stride + l),
                                                       vld1q_f32(input1Data + j * input1Stride + l)));
                    }
                    now += sum[0] + sum[1] + sum[2] + sum[3];
#elif defined(__AVX__)
                    __m256 vsum = _mm256_set1_ps(0.0f);
                    for (; l + 7 < m; l += 8) {
                        __m256 vx = _mm256_loadu_ps((const float *) (input0Data + i * input0Stride + l));
                        __m256 vy = _mm256_loadu_ps((const float *) (input1Data + j * input1Stride + l));
                        vsum = _mm256_add_ps(vsum, _mm256_mul_ps(vx, vy));
                    }
                    now += Floatsum(vsum);
#endif
                    for (; l < m; l++) {
                        now += input0Data[i * input0Stride + l] * input1Data[j * input1Stride + l];
                    }
                    outputData[i * k + j] = now * alpha;
                }
            }
        }
    }

    void MatMulTransBFloat16Single(uint16_t *input0Base, uint16_t *input1Base, uint16_t *outputBase,
                            int input0Spatial, int input1Spatial, int outputSpatial,
                            int input0Stride, int input1Stride,
                            int n, int m, int k, float alpha, int st, int end) {
        for (int b = st; b < end; b++) {
            uint16_t *input0Data = input0Base + b * input0Spatial;
            uint16_t *input1Data = input1Base + b * input1Spatial;
            uint16_t *outputData = outputBase + b * outputSpatial;
            for (int i = 0; i < n; i++) {
                for (int j = 0; j < k; j++) {
                    float now = 0.0f;
                    int l = 0;
                    for (; l < m; l++) {
                        now += fp16tofp32.dict[input0Data[i * input0Stride + l]] *
                                fp16tofp32.dict[input1Data[j * input1Stride + l]];
                    }
                    outputData[i * k + j] = float_to_half(now * alpha);
                }
            }
        }
    }

    void CpuMatMulOp::Reshape(const std::string &opType, const fastllm::DataDict &datas,
                              const fastllm::FloatDict &floatParams, const fastllm::IntDict &intParams) {
        Data &input0 = *(datas.find("input0")->second);
        Data &input1 = *(datas.find("input1")->second);
        Data &output = *(datas.find("output")->second);

        AssertInFastLLM(input0.dataDevice == input1.dataDevice, "MatMul error: inputs should use same device.\n");
        AssertInFastLLM((input0.dataType == DataType::FLOAT32 && input1.dataType == DataType::FLOAT32) ||
                        (input0.dataType == DataType::FLOAT16 && input1.dataType == DataType::FLOAT16),
                        "MatMul's input's type should be float32 or float16.\n");
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

    void CpuMatMulOp::Run(const std::string &opType, const fastllm::DataDict &datas,
                          const fastllm::FloatDict &floatParams, const fastllm::IntDict &intParams) {
        Data &input0 = *(datas.find("input0")->second);
        Data &input1 = *(datas.find("input1")->second);
        Data &output = *(datas.find("output")->second);

        output.Allocate();

        float alpha = floatParams.find("alpha") != floatParams.end() ? floatParams.find("alpha")->second : 1.0;
        int input0Spatial = input0.Count(input0.dims.size() - 2);
        int input1Spatial = input1.Count(input1.dims.size() - 2);
        int input0Stride = input0.strides[input0.dims.size() - 2];
        int input1Stride = input1.strides[input1.dims.size() - 2];
        int n = input0.dims[input0.dims.size() - 2];
        int m = input0.dims.back();
        int k = input1.dims[input1.dims.size() - 1];
        int batch0 = input0.Count(0) / input0Spatial;
        int batch1 = input1.Count(0) / input1Spatial;

        int outputSpatial = output.Count(output.dims.size() - 2);
        int threadNum = GetThreads();
#ifdef _WIN64
        threadNum = 1;
#endif
        if ((long) batch0 * n * m * k < 64ll * 4096) {
            threadNum = 1;
        }
        // threadNum = std::min(threadNum, 4);
        // TODO: 汇编优化
        int per = batch0 / threadNum;
        int cur = 0;
        auto pool = GetPool();
        std::vector <std::future <void> > futures;
        if (input0.dataType == DataType::FLOAT32) {
            for (int i = 0; i < threadNum - 1; i++) {
                int end = cur + per + (cur + per * (threadNum - i) < batch0);
                futures.push_back(pool->Submit(MatMulSingle,
                                               (float *) input0.cpuData, (float *) input1.cpuData,
                                               (float *) output.cpuData,
                                               input0Spatial, input1Spatial, outputSpatial, input0Stride, input1Stride,
                                               n, m, k, alpha, cur, end));
                cur = end;
            }
            MatMulSingle((float *) input0.cpuData, (float *) input1.cpuData, (float *) output.cpuData,
                         input0Spatial, input1Spatial, outputSpatial, input0Stride, input1Stride,
                         n, m, k, alpha, cur, batch0);
        } else if (input0.dataType == DataType::FLOAT16) {
            for (int i = 0; i < threadNum - 1; i++) {
                int end = cur + per + (cur + per * (threadNum - i) < batch0);
                futures.push_back(pool->Submit(MatMulFloat16Single,
                                               (uint16_t *) input0.cpuData, (uint16_t *) input1.cpuData,
                                               (uint16_t *) output.cpuData,
                                               input0Spatial, input1Spatial, outputSpatial, input0Stride, input1Stride,
                                               n, m, k, alpha, cur, end));
                cur = end;
            }
            MatMulFloat16Single((uint16_t *) input0.cpuData, (uint16_t *) input1.cpuData, (uint16_t *) output.cpuData,
                         input0Spatial, input1Spatial, outputSpatial, input0Stride, input1Stride,
                         n, m, k, alpha, cur, batch0);
        }

        for (int i = 0; i < futures.size(); i++) {
            futures[i].get();
        }
    }

    long long int CpuMatMulOp::Ops(const std::string &opType, const DataDict &datas, 
                                   const FloatDict &floatParams, const IntDict &intParams) {
        Data &input0 = *(datas.find("input0")->second);
        Data &input1 = *(datas.find("input1")->second);
        Data &output = *(datas.find("output")->second);
        int batch = input0.dims[0];
        int n = input0.dims[input0.dims.size() - 2];
        int m = input0.dims.back();
        int k = input1.dims[input1.dims.size() - 1];

        return (long long int) batch * n * m * k;
    }

    void CpuMatMulTransBOp::Reshape(const std::string &opType, const fastllm::DataDict &datas,
                                    const fastllm::FloatDict &floatParams, const fastllm::IntDict &intParams) {
        Data &input0 = *(datas.find("input0")->second);
        Data &input1 = *(datas.find("input1")->second);
        Data &output = *(datas.find("output")->second);

        AssertInFastLLM(input0.dataDevice == input1.dataDevice, "MatMulTransB error: inputs should use same device.\n");
        AssertInFastLLM((input0.dataType == DataType::FLOAT32 && input1.dataType == DataType::FLOAT32) ||
                        (input0.dataType == DataType::FLOAT16 && input1.dataType == DataType::FLOAT16),
                        "MatMulTransB's input's type should be float32 or float16.\n");
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

    void CpuMatMulTransBOp::Run(const std::string &opType, const fastllm::DataDict &datas,
                                const fastllm::FloatDict &floatParams, const fastllm::IntDict &intParams) {
        Data &input0 = *(datas.find("input0")->second);
        Data &input1 = *(datas.find("input1")->second);
        Data &output = *(datas.find("output")->second);

        output.Allocate();

        float alpha = floatParams.find("alpha") != floatParams.end() ? floatParams.find("alpha")->second : 1.0;
        int input0Spatial = input0.Count(input0.dims.size() - 2);
        int input1Spatial = input1.Count(input1.dims.size() - 2);
        int input0Stride = input0.strides[input0.dims.size() - 2];
        int input1Stride = input1.strides[input1.dims.size() - 2];
        int n = input0.dims[input0.dims.size() - 2];
        int m = input0.dims.back();
        int k = input1.dims[input1.dims.size() - 2];
        int batch0 = input0.Count(0) / input0Spatial;
        int batch1 = input1.Count(0) / input1Spatial;

        int outputSpatial = output.Count(output.dims.size() - 2);
        int threadNum = GetThreads();
#ifdef _WIN64
        threadNum = 1;
#endif
        if ((long) batch0 * n * m * k < 64ll * 4096) {
            threadNum = 1;
        }
        // threadNum = std::min(threadNum, 4);
        int per = batch0 / threadNum;
        int cur = 0;
        auto pool = GetPool();
        std::vector <std::future <void> > futures;
        if (input0.dataType == DataType::FLOAT32) {
            for (int i = 0; i < threadNum - 1; i++) {
                int end = cur + per + (cur + per * (threadNum - i) < batch0);
                futures.push_back(pool->Submit(MatMulTransBSingle,
                                               (float *) input0.cpuData, (float *) input1.cpuData,
                                               (float *) output.cpuData,
                                               input0Spatial, input1Spatial, outputSpatial, input0Stride, input1Stride,
                                               n, m, k, alpha, cur, end));
                cur = end;
            }
            MatMulTransBSingle((float *) input0.cpuData, (float *) input1.cpuData, (float *) output.cpuData,
                               input0Spatial, input1Spatial, outputSpatial, input0Stride, input1Stride,
                               n, m, k, alpha, cur, batch0);
        } else {
            for (int i = 0; i < threadNum - 1; i++) {
                int end = cur + per + (cur + per * (threadNum - i) < batch0);
                futures.push_back(pool->Submit(MatMulTransBFloat16Single,
                                               (uint16_t *) input0.cpuData, (uint16_t *) input1.cpuData,
                                               (uint16_t *) output.cpuData,
                                               input0Spatial, input1Spatial, outputSpatial, input0Stride, input1Stride,
                                               n, m, k, alpha, cur, end));
                cur = end;
            }
            MatMulTransBFloat16Single((uint16_t *) input0.cpuData, (uint16_t *) input1.cpuData, (uint16_t *) output.cpuData,
                               input0Spatial, input1Spatial, outputSpatial, input0Stride, input1Stride,
                               n, m, k, alpha, cur, batch0);
        }

        for (int i = 0; i < futures.size(); i++) {
            futures[i].get();
        }
    }

    long long int CpuMatMulTransBOp::Ops(const std::string &opType, const fastllm::DataDict &datas,
                                         const fastllm::FloatDict &floatParams, const fastllm::IntDict &intParams) {
        Data &input0 = *(datas.find("input0")->second);
        Data &input1 = *(datas.find("input1")->second);
        Data &output = *(datas.find("output")->second);
        int batch = input0.dims[0];
        int n = input0.dims[input0.dims.size() - 2];
        int m = input0.dims.back();
        int k = input1.dims[input1.dims.size() - 2];

        return (long long int) batch * n * m * k;
    }

    void CpuSoftMaxInner1Single(float *inputData, float *outputData, int outer, int channels) {
        for (int i = 0; i < outer; i++) {
            float maxValue = -FLT_MAX;
            int j = 0;
#ifdef __aarch64__
            float32x4_t vmax = vdupq_n_f32(maxValue);
            for (; j + 3 < channels; j += 4) {
                vmax = vmaxq_f32(vmax, vld1q_f32(inputData + j));
            }
            for (int k = 0; k < 4; k++) {
                maxValue = std::max(maxValue, vmax[k]);
            }
#endif
            for (; j < channels; j++) {
                maxValue = std::max(maxValue, inputData[j]);
            }

            j = 0;
#ifdef __aarch64__
            vmax = vdupq_n_f32(maxValue);
            for (; j + 3 < channels; j += 4) {
                vst1q_f32(outputData + j, exp_ps(vsubq_f32(vld1q_f32(inputData + j), vmax)));
            }
#endif
            for (; j < channels; j++) {
                outputData[j] = exp(inputData[j] - maxValue);
            }
            float sum = 0.0;
            j = 0;
            for (; j < channels; j++) {
                sum += outputData[j];
            }
            if (fabs(sum) == 0) {
                sum = std::numeric_limits<float>::quiet_NaN();
            } else {
                sum = 1 / sum;
            }
            j = 0;
#ifdef __aarch64__
            float32x4_t fsum = vdupq_n_f32(sum);
            for (j = 0; j + 3 < channels; j += 4) {
                vst1q_f32(outputData + j, vmulq_f32(vld1q_f32(outputData + j), fsum));
            }
#endif
            for (; j < channels; j++) {
                outputData[j] = outputData[j] * sum;
            }
            inputData += channels;
            outputData += channels;
        }
    }

    void CpuSoftMaxOp::Run(const std::string &opType, const fastllm::DataDict &datas,
                           const fastllm::FloatDict &floatParams, const fastllm::IntDict &intParams) {
        Data &input = *(datas.find("input")->second);
        Data &output = *(datas.find("output")->second);
        output.Allocate();
        int axis = intParams.find("axis") != intParams.end() ? intParams.find("axis")->second : -1;

        AssertInFastLLM(input.dataType == DataType::FLOAT32 || input.dataType == DataType::FLOAT16,
                        "Softmax error: Data's type should be float32.\n");

        int dimsLen = input.dims.size();
        axis = (axis % dimsLen + dimsLen) % dimsLen;
        int outer = input.Count(0) / input.Count(axis);
        int channels = input.dims[axis];
        int inner = input.Count(axis + 1);

        float *inputData = (float*)input.cpuData;
        float *outputData = (float*)output.cpuData;

        int threadNum = GetThreads();
        auto pool = GetPool();

        if (input.dataType == DataType::FLOAT16) {
            int len = input.Count(0);
            inputData = new float[len];
            outputData = new float[len];
            for (int i = 0; i < len; i++) {
                inputData[i] = fp16tofp32.dict[((uint16_t *) input.cpuData)[i]];
            }
        }

        if (inner == 1) {
            int per = outer / threadNum;
            if (per > 0) {
                std::vector<std::future<void>> futures;
                for (int i = 0; i < threadNum - 1; i++) {
                    float *inputWalk = inputData + i * per * channels;
                    float *outputWalk = outputData + i * per * channels;
                    futures.push_back(pool->Submit(CpuSoftMaxInner1Single, inputWalk, outputWalk, per, channels));
                }
                int last = outer - (threadNum - 1) * per;
                float *inputWalk = inputData + (threadNum - 1) * per * channels;
                float *outputWalk = outputData + (threadNum - 1) * per * channels;
                CpuSoftMaxInner1Single(inputWalk, outputWalk, last, channels);
                for (int i = 0; i < futures.size(); i++) {
                    futures[i].get();
                }
            } else {
                CpuSoftMaxInner1Single(inputData, outputData, outer, channels);
            }
        } else {
            for (int i = 0; i < outer; i++) {
                std::vector<float> maxValue(inner, -FLT_MAX);
                for (int j = 0; j < channels; j++) {
                    for (int k = 0; k < inner; k++) {
                        maxValue[k] = std::max(maxValue[k], inputData[j * inner + k]);
                    }
                }
                std::vector<float> sum(inner, 0.0);
                for (int j = 0; j < channels; j++) {
                    for (int k = 0; k < inner; k++) {
                        outputData[j * inner + k] = std::exp(inputData[j * inner + k] - maxValue[k]);
                        sum[k] += outputData[j * inner + k];
                    }
                }

                for (int j = 0; j < channels; j++) {
                    for (int k = 0; k < inner; k++) {
                        outputData[j * inner + k] /= sum[k];
                    }
                }

                inputData += channels * inner;
                outputData += channels * inner;
            }
        }

        if (input.dataType == DataType::FLOAT16) {
            int len = input.Count(0);
            inputData -= len;
            outputData -= len;
            for (int i = 0; i < len; i++) {
                ((uint16_t *) output.cpuData)[i] = float_to_half(outputData[i]);
            }

            delete[] inputData;
            delete[] outputData;
        }
    }

    void FloatSiluPart(float *inputData, float *outputData, int len) {
        int i = 0;
#ifdef __aarch64__
        float32x4_t one = vdupq_n_f32(1.f);
        for (; i + 3 < len; i += 4) {
            float32x4_t x = vld1q_f32(inputData + i);
            vst1q_f32(outputData + i, vdivq_f32(x, vaddq_f32(one, exp_ps(vnegq_f32(x)))));
        }
#endif
        for (; i < len; i++) {
            float x = inputData[i];
            outputData[i] = x / (1.0 + expf(-x));
        }
    }

    void CpuSiluOp::Run(const std::string &opType, const fastllm::DataDict &datas,
                        const fastllm::FloatDict &floatParams, const fastllm::IntDict &intParams) {
        Data &input = *(datas.find("input")->second);
        Data &output = *(datas.find("output")->second);
        output.Allocate();
        AssertInFastLLM(input.dataType == DataType::FLOAT32 || input.dataType == DataType::FLOAT16,
                        "Silu error: Data's type should be float32.\n");
        float *inputData = (float*)input.cpuData;
        float *outputData = (float*)output.cpuData;

        if (input.dataType == DataType::FLOAT16) {
            int len = input.Count(0);
            inputData = new float[len];
            outputData = new float[len];
            for (int i = 0; i < len; i++) {
                inputData[i] = fp16tofp32.dict[((uint16_t *) input.cpuData)[i]];
            }
        }

        int len = input.Count(0);
        int threadNum = GetThreads();
        int per = len / threadNum;
        int last = len - (threadNum - 1) * per;
        
        auto pool = GetPool();
        std::vector <std::future <void> > futures;
        int start = 0;
        for (int i = 0; i < threadNum - 1; i++) {
            futures.push_back(pool->Submit(FloatSiluPart, inputData + start, outputData + start, per));
            start += per;
        }
        FloatSiluPart(inputData + start, outputData + start, last);
        for (int i = 0; i < futures.size(); i++) {
            futures[i].get();
        }

        if (input.dataType == DataType::FLOAT16) {
            for (int i = 0; i < len; i++) {
                ((uint16_t *) output.cpuData)[i] = float_to_half(outputData[i]);
            }

            delete[] inputData;
            delete[] outputData;
        }
    }

    void FloatGeluNewPart(float *inputData, float *outputData, int len) {
        int i = 0;
#ifdef __aarch64__
        float32x4_t c0 = vdupq_n_f32(0.044715f);
        float32x4_t c1 = vdupq_n_f32(1.0f);
        float32x4_t c2 = vdupq_n_f32(0.7978845608028654f);
        float32x4_t c3 = vdupq_n_f32(0.5f);

        for (; i + 3 < len; i += 4) {
            float32x4_t vx = vld1q_f32(inputData + i);
            float32x4_t v1 = vaddq_f32(c1, vmulq_f32(vmulq_f32(c0, vx), vx));
            float32x4_t v2 = vmulq_f32(vmulq_f32(c2, vx), v1);
            float32x4_t vex = exp_ps(v2);
            float32x4_t venegx = exp_ps(vnegq_f32(v2));
            float32x4_t vtan = vdivq_f32(vsubq_f32(vex, venegx), vaddq_f32(vex, venegx));
            float32x4_t vout = vmulq_f32(vmulq_f32(c3, vx), vaddq_f32(c1, vtan));
            vst1q_f32(outputData + i, vout);
        }
#endif
#ifdef __AVX2__
        auto var1 = _mm256_set1_ps(0.044715f);
        auto var2 = _mm256_set1_ps(0.7978845608028654f);
        auto var3 = _mm256_set1_ps(378.f);
        auto var4 = _mm256_set1_ps(17325.f);
        auto var5 = _mm256_set1_ps(135135.f);
        auto var6 = _mm256_set1_ps(28.f);
        auto var7 = _mm256_set1_ps(3150.f);
        auto var8 = _mm256_set1_ps(62370.f);
        auto var9 = _mm256_set1_ps(135135.f);
        auto var10 = _mm256_set1_ps(0.5);
        auto varOne = _mm256_set1_ps(1.f);
        auto varNegOne = _mm256_set1_ps(-1.f);

        for (; i < len - 7; i+=8) {
            auto x = _mm256_loadu_ps(inputData + i);  
            // sqrt(2 / PI) * (0.044715 * x^3 + x)
            auto y = _mm256_mul_ps(x, x);
            y = _mm256_mul_ps(y, x);
            y = _mm256_mul_ps(y, var1);
            y = _mm256_add_ps(y, x);
            y = _mm256_mul_ps(y, var2);

            // y = tanh(y)
            {
            auto y2 = _mm256_mul_ps(y, y);
            auto w = _mm256_add_ps(y2, var3);
            w = _mm256_mul_ps(w, y2);
            w = _mm256_add_ps(w, var4);
            w = _mm256_mul_ps(w, y2);
            w = _mm256_add_ps(w, var5);
            w = _mm256_mul_ps(w, y);
            auto z = _mm256_mul_ps(y2, var6);
            z = _mm256_add_ps(z, var7);
            z = _mm256_mul_ps(z, y2);
            z = _mm256_add_ps(z, var8);
            z = _mm256_mul_ps(z, y2);
            z = _mm256_add_ps(z, var9);
            z = _mm256_div_ps(w, z);
            z = _mm256_max_ps(z, varNegOne);
            y = _mm256_min_ps(z, varOne);
            }

            y = _mm256_add_ps(y, varOne);
            y = _mm256_mul_ps(y, x);
            y = _mm256_mul_ps(y, var10);
            _mm256_storeu_ps(outputData + i, y);
        }
#endif
        for (; i < len; i++) {
            float x = inputData[i];
            outputData[i] = 0.5f * x * (1.0f + tanhf(0.7978845608028654f * x * (1.0f + 0.044715f * x * x)));
        }
    }

    void CpuGeluNewOp::Run(const std::string &opType, const fastllm::DataDict &datas,
                           const fastllm::FloatDict &floatParams, const fastllm::IntDict &intParams) {
        Data &input = *(datas.find("input")->second);
        Data &output = *(datas.find("output")->second);
        output.Allocate();
        AssertInFastLLM(input.dataType == DataType::FLOAT32, "GeluNew error: Data's type should be float32.\n");

        float *inputData = (float*)input.cpuData;
        float *outputData = (float*)output.cpuData;
        int len = input.Count(0);
        int threadNum = GetThreads();
        int per = len / threadNum;
        int last = len - (threadNum - 1) * per;

        if (per <= 256) {
           FloatGeluNewPart(inputData, outputData, len); 
           return;
        }

        auto pool = GetPool();
        std::vector <std::future <void> > futures;
        int start = 0;
        for (int i = 0; i < threadNum - 1; i++) {
            futures.push_back(pool->Submit(FloatGeluNewPart, inputData + start, outputData + start, per));
            start += per;
        }
        FloatGeluNewPart(inputData + start, outputData + start, last);
        for (int i = 0; i < futures.size(); i++) {
            futures[i].get();
        }
    }

    void CpuSwigluOp::Reshape(const std::string &opType, const fastllm::DataDict &datas,
                              const fastllm::FloatDict &floatParams, const fastllm::IntDict &intParams) {
        Data &input = *(datas.find("input")->second);
        Data &output = *(datas.find("output")->second);

        std::vector <int> dims = input.dims;
        dims[dims.size() - 1] /= 2;
        output.dataType = input.dataType;
        output.Resize(dims);
    }

    void FloatSwigluPart(float *input, float *output, int outer, int spatial) {
        int mid = spatial / 2;
        for (int o = 0; o < outer; o++) {
            int i = 0;
#ifdef __aarch64__
            float32x4_t c1 = vdupq_n_f32(1.0f);
            for (; i + 3 < mid; i += 4) {
                float32x4_t vx = vld1q_f32(input + i);
                float32x4_t vy = vld1q_f32(input + i + mid);
                vx = vdivq_f32(vx, vaddq_f32(c1, exp_ps(vnegq_f32(vx))));
                vy = vmulq_f32(vx, vy);
                vst1q_f32(output + i, vy);
            }
#endif
            for (; i < mid; i++) {
                float x = input[i], y = input[i + mid];
                output[i] = (x / (1.0 + expf(-x))) * y;
            }
            input += spatial;
            output += mid;
        }
    }

    void CpuSwigluOp::Run(const std::string &opType, const fastllm::DataDict &datas,
                           const fastllm::FloatDict &floatParams, const fastllm::IntDict &intParams) {
        Data &input = *(datas.find("input")->second);
        Data &output = *(datas.find("output")->second);
        output.Allocate();
        AssertInFastLLM(input.dataType == DataType::FLOAT32 || input.dataType == DataType::FLOAT16,
                        "Swiglu error: Data's type should be float32 or float16.\n");

        float *inputData = (float*)input.cpuData;
        float *outputData = (float*)output.cpuData;

        if (input.dataType == DataType::FLOAT16) {
            int len = input.Count(0);
            inputData = new float[len];
            outputData = new float[output.Count(0)];
            for (int i = 0; i < len; i++) {
                inputData[i] = fp16tofp32.dict[((uint16_t *) input.cpuData)[i]];
            }
        }

        int spatial = input.Count(input.dims.size() - 1), mid = spatial / 2;
        int outer = input.Count(0) / spatial;
        
        int threadNum = GetThreads();
        int per = outer / threadNum;
        int last = outer - (threadNum - 1) * per;

        if (per == 0) {
            FloatSwigluPart(inputData, outputData, outer, spatial);
            return;
        }

        auto pool = GetPool();
        std::vector <std::future<void>> futures;
        for (int i = 0; i < threadNum - 1; i++) {
            FloatSwigluPart(inputData, outputData, per, spatial);
            inputData += per * spatial;
            outputData += per * mid;
        }
        FloatSwigluPart(inputData, outputData, last, spatial);
        for (int i = 0; i < futures.size(); i++) {
            futures[i].get();
        }

        if (input.dataType == DataType::FLOAT16) {
            inputData -= input.Count(0);
            outputData -= output.Count(0);
            int len = output.Count(0);
            for (int i = 0; i < len; i++) {
                ((uint16_t *) output.cpuData)[i] = float_to_half(outputData[i]);
            }

            delete[] inputData;
            delete[] outputData;
        }
    }

    void CpuMulOp::Run(const std::string &opType, const fastllm::DataDict &datas,
                       const fastllm::FloatDict &floatParams, const fastllm::IntDict &intParams) {
        Data &input = *(datas.find("input")->second);
        Data &output = *(datas.find("output")->second);
        output.Allocate();

        float v = floatParams.find("v") != floatParams.end() ? floatParams.find("v")->second : 1.0;
        AssertInFastLLM(input.dataType == DataType::FLOAT32 || input.dataType == DataType::FLOAT16,
                        "Mul error: Data's type should be float32 or float16.\n");

        int len = input.Count(0);

        if (input.dataType == DataType::FLOAT32) {
            float *inputData = (float *) input.cpuData;
            float *outputData = (float *) output.cpuData;
            for (int i = 0; i < len; i++) {
                outputData[i] = inputData[i] * v;
            }
        } else if (input.dataType == DataType::FLOAT16) {
            uint16_t *inputData = (uint16_t *) input.cpuData;
            uint16_t *outputData = (uint16_t *) output.cpuData;
            for (int i = 0; i < len; i++) {
                outputData[i] = float_to_half(fp16tofp32.dict[inputData[i]] * v);
            }
        }
    }

    void CpuMulToOp::Run(const std::string &opType, const fastllm::DataDict &datas,
                    const fastllm::FloatDict &floatParams, const fastllm::IntDict &intParams) {
        Data &input0 = *(datas.find("input0")->second);
        Data &input1 = *(datas.find("input1")->second);
        AssertInFastLLM(input0.dims == input1.dims, "MulTo error: input's shape should be same.\n");

        float *input0Data = (float*)input0.cpuData;
        float *input1Data = (float*)input1.cpuData;

        int len = input0.Count(0);
        int inner = input1.Count(0);
        AssertInFastLLM(len % inner == 0, "MulTo error: Data`s shape can`t perform MulTo operation.\n");
        int round = (len / inner);
        for (int j = 0; j < round; j++) {
            int i = 0;
#ifdef __aarch64__
            for (; i + 3 < len; i += 4) {
                float32x4_t res = vmulq_f32(vld1q_f32(input0Data + i), vld1q_f32(input1Data + i));
                vst1q_f32(input0Data + i, res);
            }
#endif
            for (; i < len; i++) {
               input0Data[i] *= input1Data[i];
            }
            input0Data += inner;
        }
    }

    void CpuScalingOp::Run(const std::string &opType, const fastllm::DataDict &datas,
                           const fastllm::FloatDict &floatParams, const fastllm::IntDict &intParams) {
        Data &data = *(datas.find("input")->second);
        float v = floatParams.find("v") != floatParams.end() ? floatParams.find("v")->second : 1.0;

        AssertInFastLLM(data.dataType == DataType::FLOAT32 || data.dataType == DataType::FLOAT16,
                        "Scaling error: Data's type should be float32 or float16.\n");

        int len = data.Count(0);

        if (data.dataType == DataType::FLOAT32) {
            float *cpuData = (float *) data.cpuData;
            for (int i = 0; i < len; i++) {
                cpuData[i] *= v;
            }
        } else if (data.dataType == DataType::FLOAT16) {
            uint16_t *cpuData = (uint16_t *) data.cpuData;
            for (int i = 0; i < len; i++) {
                cpuData[i] = float_to_half(half_to_float(cpuData[i]) * v);
            }
        }
    }

    void CpuAddToOp::Run(const std::string &opType, const fastllm::DataDict &datas,
                         const fastllm::FloatDict &floatParams, const fastllm::IntDict &intParams) {
        Data &input0 = *(datas.find("input0")->second);
        Data &input1 = *(datas.find("input1")->second);
        float alpha = floatParams.find("alpha") != floatParams.end() ? floatParams.find("alpha")->second : 1.0;

        AssertInFastLLM(input0.dataType == DataType::FLOAT32 || input1.dataType == DataType::FLOAT16,
                        "AddTo error: Data's type should be float32 or float16.\n");

        if (input0.dims == input1.dims) {
            int len = input0.Count(0);
            if (input0.dataType == DataType::FLOAT32) {
                float *input0Data = (float *) input0.cpuData;
                float *input1Data = (float *) input1.cpuData;
                int i = 0;
#ifdef __aarch64__
                float32x4_t alphax4 = vdupq_n_f32(alpha);
                for (; i + 3 < len; i += 4) {
                    float32x4_t res = vaddq_f32(vld1q_f32(input0Data + i), vmulq_f32(vld1q_f32(input1Data + i), alphax4));
                    vst1q_f32(input0Data + i, res);
                }
#endif
                for (; i < len; i++) {
                    input0Data[i] += input1Data[i] * alpha;
                }
            } else if (input0.dataType == DataType::FLOAT16) {
                uint16_t *input0Data = (uint16_t *) input0.cpuData;
                uint16_t *input1Data = (uint16_t *) input1.cpuData;
                for (int i = 0; i < len; i++) {
                    input0Data[i] = float_to_half(fp16tofp32.dict[input0Data[i]] + fp16tofp32.dict[input1Data[i]] * alpha);
                }
            }
        } else {
            int len0 = input0.Count(0);
            int len1 = input1.Count(0);
            int scale = len0 / len1;
            AssertInFastLLM(scale > 1, "AddTo error: Input0 size must be no less than Input1 size.\n");
            if (input0.dataType == DataType::FLOAT32) {
                float *input0Data = (float *) input0.cpuData;
                float *input1Data = (float *) input1.cpuData;
                for (int i = 0; i < len0; i++) {
                    input0Data[i] += input1Data[i / scale] * alpha;
                }
            } else if (input0.dataType == DataType::FLOAT16) {
                uint16_t *input0Data = (uint16_t *) input0.cpuData;
                uint16_t *input1Data = (uint16_t *) input1.cpuData;
                for (int i = 0; i < len0; i++) {
                    input0Data[i] = float_to_half(fp16tofp32.dict[input0Data[i]] + fp16tofp32.dict[input1Data[i / scale]] * alpha);
                }
            }
        }
        
    }

    void CpuAttentionMaskOp::Run(const std::string &opType, const fastllm::DataDict &datas,
                                 const fastllm::FloatDict &floatParams, const fastllm::IntDict &intParams) {
        Data &input = *(datas.find("input")->second);
        Data &mask = *(datas.find("mask")->second);
        float maskValue = floatParams.find("maskValue") != floatParams.end() ? floatParams.find("maskValue")->second : -10000.0;
        int spatial = input.Count(2), n = input.dims[0], m = input.dims[1];

        AssertInFastLLM(mask.dataType == DataType::FLOAT32, "AttentionMask: mask's datatype should be float32.");
        if (input.dataType == DataType::FLOAT32) {
            float *maskData = (float *) mask.cpuData;
            float *attnData = (float *) input.cpuData;
            for (int on = 0; on < n; on++) {
                for (int om = 0; om < m; om++) {
                    int o = on * m + om;
                    for (int i = 0; i < spatial; i++) {
                        if (maskData[on * spatial + i] > 0.99) {
                            attnData[o * spatial + i] = maskValue;
                        }
                    }
                }
            }
        } else if (input.dataType == DataType::FLOAT16) {
            float *maskData = (float *) mask.cpuData;
            uint16_t *attnData = (uint16_t *) input.cpuData;
            uint16_t hMaskValue = float_to_half(maskValue);
            for (int on = 0; on < n; on++) {
                for (int om = 0; om < m; om++) {
                    int o = on * m + om;
                    for (int i = 0; i < spatial; i++) {
                        if (maskData[on * spatial + i] > 0.99) {
                            attnData[o * spatial + i] = hMaskValue;
                        }
                    }
                }
            }
        } else {
            ErrorInFastLLM("AttentionMask error: unsupport input's dataType.\n");
        }
    }

    void CpuAlibiMaskOp::Run(const std::string &opType, const fastllm::DataDict &datas,
                             const fastllm::FloatDict &floatParams, const fastllm::IntDict &intParams) {
        Data &input = *(datas.find("input")->second);
        Data &mask = *(datas.find("mask")->second);
        float maskValue = floatParams.find("maskValue") != floatParams.end() ? floatParams.find("maskValue")->second : -10000.0;
        float *maskData = (float *) mask.cpuData;
        float *attnData = (float *) input.cpuData;
        int n = input.dims[0], m = input.dims[1];
        int spn = input.dims[2], spm = input.dims[3];
        int spatial = input.Count(2);
        for (int on = 0; on < n; on++) {
            for (int om = 0; om < m; om++) {
                float now = maskData[om];
                int o = on * m + om;
                float *inputNow = attnData + o * spatial;
                for (int i = 0; i < spn; i++) {
                    int mid = (spm - spn + i);
                    for (int j = 0; j <= mid; j++) {
                        inputNow[i * spm + j] += now * j;
                    }
                    for (int j = mid + 1; j < spm; j++) {
                        inputNow[i * spm + j] = maskValue;
                    }
                }
            }
        }
    }

    void CpuTopKOp::Reshape(const std::string &opType, const fastllm::DataDict &datas,
                            const fastllm::FloatDict &floatParams, const fastllm::IntDict &intParams) {
        Data &input = *(datas.find("input")->second);
        Data &output = *(datas.find("output")->second);
        int topk = intParams.find("topk") != intParams.end() ? intParams.find("topk")->second : 1;

        AssertInFastLLM(input.dataType == DataType::FLOAT32, "TopK error: Data's type should be float32.\n");
        AssertInFastLLM(topk == 1, "Unsupport topk > 1.");

        int dimsLen = input.dims.size();
        std::vector<int> dims = input.dims;
        dims[dimsLen - 1] = topk * 2;

        output.dataType = input.dataType;
        output.Resize(dims);
    }

    void CpuTopKOp::Run(const std::string &opType, const fastllm::DataDict &datas,
                        const fastllm::FloatDict &floatParams, const fastllm::IntDict &intParams) {
        Data &input = *(datas.find("input")->second);
        Data &output = *(datas.find("output")->second);
        output.Allocate();

        int topk = intParams.find("topk") != intParams.end() ? intParams.find("topk")->second : -1;
        int dimsLen = input.dims.size();
        int outer = input.Count(0) / input.Count(dimsLen - 1);
        int channels = input.dims[dimsLen - 1];

        float *inputData = (float*)input.cpuData;
        float *outputData = (float*)output.cpuData;

        if (topk == 1) {
            for (int o = 0; o < outer; o++) {
                float maxValue = -1e100, idx = -1;
                for (int j = 0; j < channels; j++) {
                    if (inputData[j] > maxValue) {
                        maxValue = inputData[j];
                        idx = j;
                    }
                }
                outputData[0] = idx;
                outputData[1] = maxValue;
                inputData += channels;
                outputData += 2;
            }
        } else {
            ErrorInFastLLM("Unsupport topk > 1.");
        }
    }

    void CpuPermuteOp::Reshape(const std::string &opType, const fastllm::DataDict &datas,
                               const fastllm::FloatDict &floatParams, const fastllm::IntDict &intParams) {
        Data &input = *(datas.find("input")->second);
        Data &output = *(datas.find("output")->second);
        Data &axisData = *(datas.find("axis")->second);
        std::vector <int> axis;
        for (int i = 0; i < axisData.Count(0); i++) {
            axis.push_back(((int32_t *) axisData.cpuData)[i]);
        }

        AssertInFastLLM(input.dataType == DataType::FLOAT32 ||
                        input.dataType == DataType::FLOAT16, "Permute error: datatype should be float32 or float16.");
        AssertInFastLLM(axis.size() == input.dims.size(), "Permute error: axis's size should be equal to data's shape's size.");
        std::vector<int> new_dims;
        for (int i = 0; i < axis.size(); i++) {
            new_dims.push_back(input.dims[axis[i]]);
        }

        output.dataType = input.dataType;
        output.Resize(new_dims);
    }

    void Transpose4x4(float *pDst, float *pSrc, int dstStride, int srcStride, int n, int m) {
        if (n < 4 || m < 4) {
            for (int i = 0; i < n; i++) {
                for (int j = 0; j < m; j++) {
                    pDst[j * dstStride + i] = pSrc[i * srcStride + j];
                }
            }

            return;
        }

#ifdef __aarch64__
        float32x4x2_t q01 = vtrnq_f32(vld1q_f32(pSrc), vld1q_f32(pSrc + srcStride));
        float32x4x2_t q23 = vtrnq_f32(vld1q_f32(pSrc + 2 * srcStride), vld1q_f32(pSrc + 3 * srcStride));

        float32x4_t qq0 = q01.val[0];
        float32x2_t d00 = vget_low_f32(qq0);
        float32x2_t d01 = vget_high_f32(qq0);

        float32x4_t qq1 = q01.val[1];
        float32x2_t d10 = vget_low_f32(qq1);
        float32x2_t d11 = vget_high_f32(qq1);

        float32x4_t qq2 = q23.val[0];
        float32x2_t d20 = vget_low_f32(qq2);
        float32x2_t d21 = vget_high_f32(qq2);

        float32x4_t qq3 = q23.val[1];
        float32x2_t d30 = vget_low_f32(qq3);
        float32x2_t d31 = vget_high_f32(qq3);

        vst1q_f32(pDst, vcombine_f32(d00, d20));
        vst1q_f32(pDst + 1 * dstStride, vcombine_f32(d10, d30));
        vst1q_f32(pDst + 2 * dstStride, vcombine_f32(d01, d21));
        vst1q_f32(pDst + 3 * dstStride, vcombine_f32(d11, d31));
#else
        for (int i = 0; i < n; i++) {
            for (int j = 0; j < m; j++) {
                pDst[j * dstStride + i] = pSrc[i * srcStride + j];
            }
        }
#endif
    }

    void Transpose(float *pDst, float *pSrc, int dstStride, int srcStride, int n, int m) {
        int per = 4;
        for (int i = 0; i < n; i += per) {
            for (int j = 0; j < m; j += per) {
                Transpose4x4(pDst + j * dstStride + i,
                             pSrc + i * srcStride + j,
                             dstStride, srcStride,
                             std::min(per, n - i),
                             std::min(per, m - j));
            }
        }
    }

    void CpuPermuteOp::Run(const std::string &opType, const fastllm::DataDict &datas,
                           const fastllm::FloatDict &floatParams, const fastllm::IntDict &intParams) {
        Data &input = *(datas.find("input")->second);
        Data &output = *(datas.find("output")->second);
        Data &axisData = *(datas.find("axis")->second);
        std::vector <int> axis;
        for (int i = 0; i < axisData.Count(0); i++) {
            axis.push_back(((int32_t *) axisData.cpuData)[i]);
        }

        output.Allocate();
        uint8_t *tmpData = (uint8_t *) output.cpuData;
        uint8_t *curData = (uint8_t *) input.cpuData;

        if (axis == std::vector <int> {1, 2, 0} && input.dataType == DataType::FLOAT32) {
            int n = input.dims[0];
            int m = input.Count(1);

            int threadNum = 1;
            int per = m / threadNum;
            int cur = 0;
            auto pool = GetPool();
            std::vector <std::future <void> > futures;
            for (int i = 0; i < threadNum - 1; i++) {
                int end = cur + per + (cur + per * (threadNum - i) < m);
                futures.push_back(pool->Submit(Transpose, ((float*)tmpData) + cur * n, ((float*)curData) + cur, n, m, n, end - cur));
                cur = end;
            }
            Transpose(((float*)tmpData) + cur * n, ((float*)curData) + cur, n, m, n, m - cur);
            for (int i = 0; i < futures.size(); i++) {
                futures[i].get();
            }
        } else if (axis == std::vector <int> {1, 0, 2}) {
            int n = input.dims[0];
            int m = input.dims[1];
            int k = input.dims[2];
            int unitSize = input.unitSize;
            for (int i = 0; i < n; i++) {
                for (int j = 0; j < m; j++) {
                    memcpy(tmpData + (j * n + i) * k * unitSize, curData + (i * m + j) * k * unitSize, k * unitSize);
                }
            }
        } else if (axis == std::vector <int> {2, 0, 1, 3}) {
            int n = input.dims[0] * input.dims[1];
            int m = input.dims[2];
            int k = input.dims[3];
            int unitSize = input.unitSize;
            for (int i = 0; i < n; i++) {
                for (int j = 0; j < m; j++) {
                    memcpy(tmpData + (j * n + i) * k * unitSize, curData + (i * m + j) * k * unitSize, k * unitSize);
                }
            }
        } else if (axis == std::vector<int> {0, 2, 1, 3}) {
            int b = input.dims[0];
            int n = input.dims[1];
            int m = input.dims[2];
            int k = input.dims[3];
            int unitSize = input.unitSize;
            for (int o = 0; o < b; o++) {
                for (int i = 0; i < n; i++) {
                    for (int j = 0; j < m; j++) {
                        memcpy(tmpData + (j * n + i) * k * unitSize, curData + (i * m + j) * k * unitSize, k * unitSize);
                    }
                }
                tmpData += output.Count(1) * unitSize;
                curData += input.Count(1) * unitSize;
            }
        } else {
            std::vector<int> oldSteps;
            std::vector<int> newSteps;
            int count = input.Count(0);
            auto oldPos = new int[count];
            for (int i = 0; i < axis.size(); i++) {
                oldSteps.push_back(input.Count(i + 1));
                newSteps.push_back(output.Count(i + 1));
            }

            for (int i = 0; i < count; ++i) {
                int old = 0;
                int idx = i;
                for (int j = 0; j < axis.size(); ++j) {
                    int order = axis[j];
                    old += (idx / newSteps[j]) * oldSteps[order];
                    idx %= newSteps[j];
                }
                oldPos[i] = old;
            }

            if (input.unitSize == 4) {
                for (int i = 0; i < count; ++i) {
                    ((float*)tmpData)[i] = ((float*)curData)[oldPos[i]];
                }
            } else if (input.unitSize == 2) {
                for (int i = 0; i < count; ++i) {
                    ((uint16_t*)tmpData)[i] = ((uint16_t*)curData)[oldPos[i]];
                }
            } else if (input.unitSize == 1) {
                for (int i = 0; i < count; ++i) {
                    ((uint8_t*)tmpData)[i] = ((uint8_t*)curData)[oldPos[i]];
                }
            }

            delete[] oldPos;
        }
    }

    void CpuPermuteSelfOp::Run(const std::string &opType, const fastllm::DataDict &datas,
                               const fastllm::FloatDict &floatParams, const fastllm::IntDict &intParams) {
        Data &input = *(datas.find("input")->second);
        Data &axisData = *(datas.find("axis")->second);
        std::vector <int> axis;
        for (int i = 0; i < axisData.Count(0); i++) {
            axis.push_back(((int32_t *) axisData.cpuData)[i]);
        }

        AssertInFastLLM(input.dataType == DataType::FLOAT32 ||
                        input.dataType == DataType::FLOAT16, "Permute error: datatype should be float32 or float16.");
        AssertInFastLLM(axis.size() == input.dims.size(), "Permute error: axis's size should be equal to data's shape's size.");

        bool same = false;
        same |= ((axis == std::vector <int>{1, 2, 0} || axis == std::vector <int>{1, 0, 2}) && (input.dims[0] == 1 || input.dims[1] == 1));
        same |= ((axis == std::vector <int>{2, 0, 1, 3}) && input.dims[2] == 1);
        same |= ((axis == std::vector <int>{0, 2, 1, 3}) && (input.dims[1] == 1 || input.dims[2] == 1));
        if (same) {
            std::vector<int> new_dims;
            for (int i = 0; i < axis.size(); i++) {
                new_dims.push_back(input.dims[axis[i]]);
            }
            input.Resize(new_dims);
            return;
        }

        auto tmp = new Data();
        fastllm::Permute(input, axis, *tmp);

        memcpy(input.cpuData, tmp->cpuData, input.unitSize * input.Count(0));
        input.Resize(tmp->dims);
        delete tmp;
    }

    void CpuRotatePosition2DOp::Run(const std::string &opType, const fastllm::DataDict &datas,
                                    const fastllm::FloatDict &floatParams, const fastllm::IntDict &intParams) {
        Data &data = *(datas.find("input")->second);
        Data &positionIds = *(datas.find("positionIds")->second);
        Data &sinData = *(datas.find("sin")->second);
        Data &cosData = *(datas.find("cos")->second);
        int rotaryDim = intParams.find("rotaryDim") != intParams.end() ? intParams.find("rotaryDim")->second : 64;

        int len = data.dims[0], bs = data.dims[1];
        int spatial = data.Count(2);
        int n = data.dims[2], m = data.dims[3];
        int stride = (int)sinData.dims[1];
        for (int l = 0; l < len; l++) {
            for (int b = 0; b < bs; b++) {
                for (int part = 0; part < 2; part++) {
                    int index = (int) ((float *) positionIds.cpuData)[(b * 2 + part) * positionIds.dims.back() + l];
                    float *sin = ((float*)sinData.cpuData) + stride * index;
                    float *cos = ((float*)cosData.cpuData) + stride * index;
                    float *d = (float *) data.cpuData + (l * bs + b) * spatial + part * m / 2;
                    for (int i = 0; i < n; i++) {
                        for (int j = 0; j < rotaryDim && j < m / 4; j++) {
                            float a = d[j], b = d[j + m / 4];
                            d[j] = a * cos[j] - b * sin[j];
                            d[j + m / 4] = a * sin[j] + b * cos[j];
                        }

                        d += m;
                    }
                }
            }
        }
    }

    void CpuNearlyRotatePosition2DOp::Run(const std::string &opType, const fastllm::DataDict &datas,
                                          const fastllm::FloatDict &floatParams, const fastllm::IntDict &intParams) {
        Data &data = *(datas.find("input")->second);
        Data &positionIds = *(datas.find("positionIds")->second);
        Data &sinData = *(datas.find("sin")->second);
        Data &cosData = *(datas.find("cos")->second);
        int rotaryDim = intParams.find("rotaryDim") != intParams.end() ? intParams.find("rotaryDim")->second : 64;

        int len = data.dims[0], bs = data.dims[1];
        int spatial = data.Count(2);
        int n = data.dims[2], m = data.dims[3];
        int stride = (int)sinData.dims[1];
        for (int l = 0; l < len; l++) {
            for (int b = 0; b < bs; b++) {
                int index = (int) ((float *) positionIds.cpuData)[(b * 2) * positionIds.dims.back() + l];
                float *sin = ((float*)sinData.cpuData) + stride * index;
                float *cos = ((float*)cosData.cpuData) + stride * index;

                if (data.dataType == DataType::FLOAT32) {
                    float *d = (float *) data.cpuData + (l * bs + b) * spatial;
                    for (int i = 0; i < n; i++) {
                        int j = 0;
                        for (; j < rotaryDim; j += 2) {
                            float a = d[j], b = d[j + 1];
                            d[j] = a * cos[j / 2] - b * sin[j / 2];
                            d[j + 1] = a * sin[j / 2] + b * cos[j / 2];
                        }
                        d += m;
                    }
                } else if (data.dataType == DataType::FLOAT16) {
                    uint16_t *d = (uint16_t *) data.cpuData + (l * bs + b) * spatial;
                    for (int i = 0; i < n; i++) {
                        int j = 0;
                        for (; j < rotaryDim; j += 2) {
                            float a = fp16tofp32.dict[d[j]], b = fp16tofp32.dict[d[j + 1]];
                            d[j] = float_to_half(a * cos[j / 2] - b * sin[j / 2]);
                            d[j + 1] = float_to_half(a * sin[j / 2] + b * cos[j / 2]);
                        }
                        d += m;
                    }
                }
            }
        }
    }

    void CpuLlamaRotatePosition2DOp::Run(const std::string &opType, const fastllm::DataDict &datas,
                                    const fastllm::FloatDict &floatParams, const fastllm::IntDict &intParams) {
        Data &data = *(datas.find("input")->second);
        Data &positionIds = *(datas.find("positionIds")->second);
        Data &sinData = *(datas.find("sin")->second);
        Data &cosData = *(datas.find("cos")->second);
        int rotaryDim = intParams.find("rotaryDim") != intParams.end() ? intParams.find("rotaryDim")->second : 128;

        int bs = data.dims[0], len = data.dims[1];
        int spatial = data.Count(2);
        int n = data.dims[2], m = data.dims[3];
        int stride = (int)sinData.dims[1];
        for (int b = 0; b < bs; b++) {
            for (int l = 0; l < len; l++) {
                int index = (int) ((float *) positionIds.cpuData)[b * positionIds.dims.back() + l];
                float *sin = ((float *) sinData.cpuData) + stride * index;
                float *cos = ((float *) cosData.cpuData) + stride * index;
                float *d = (float *) data.cpuData + (b * len + l) * spatial;
                for (int i = 0; i < n; i++) {
                    for (int j = 0; j < rotaryDim && j < m / 2; j++) {
                        float a = d[j], b = d[j + m / 2];
                        d[j] = a * cos[j] - b * sin[j];
                        d[j + m / 2] = a * sin[j] + b * cos[j];
                    }

                    d += m;
                }
            }
        }
    }

    void CpuRepeatPenaltyOp::Run(const std::string &opType, const fastllm::DataDict &datas,
                         const fastllm::FloatDict &floatParams, const fastllm::IntDict &intParams) {
        Data &input = *(datas.find("input")->second);
        Data &penalty = *(datas.find("penalty")->second);
        AssertInFastLLM(input.dataType == DataType::FLOAT32 && penalty.dataType == DataType::FLOAT32,
                        "Repeat Penalty error: Data's type should be float32.\n");
        float *inputData = (float*)input.cpuData;
        float *penaltyData = (float*)penalty.cpuData;

        int len = input.Count(0);
        for (int i = 0; i < len; i++) {
            inputData[i] = inputData[i] < 0 ? inputData[i] * penaltyData[i] : inputData[i] / penaltyData[i];
        }
    }

    void CpuRepeatKVOp::Run(const std::string &opType, const DataDict &datas, 
                            const FloatDict &floatParams, const IntDict &intParams) {
        Data &input = *(datas.find("input")->second);
        int num_key_value_groups = intParams.find("num_key_value_groups")->second;

        auto origin = new uint8_t[input.GetBytes()];
        memcpy(origin, input.cpuData, input.GetBytes());

        std::vector<int> dims = input.dims;
        int batch = dims[0];
        int channel = dims[1];
        int inner = input.Count(2);

        dims[1] *= num_key_value_groups;
        input.Resize(dims);
        input.Allocate();

        if (input.dataType == DataType::FLOAT32) {
            float *inputData = (float *) origin;
            float *outputData = (float *) input.cpuData;

            for (int b = 0; b < batch; b++) {
                for (int c = 0; c < channel; c++) {
                    for (int i = 0; i < num_key_value_groups; i++) {
                        memcpy(outputData + c * num_key_value_groups * inner + i * inner, inputData + c * inner, inner * sizeof(float));
                    }
                }
                
                inputData += channel * inner;
                outputData += num_key_value_groups * channel * inner;
            }
        } else if (input.dataType == DataType::FLOAT16) {
            uint16_t *inputData = (uint16_t *) origin;
            uint16_t *outputData = (uint16_t *) input.cpuData;

            for (int b = 0; b < batch; b++) {
                for (int c = 0; c < channel; c++) {
                    for (int i = 0; i < num_key_value_groups; i++) {
                        memcpy(outputData + c * num_key_value_groups * inner + i * inner, inputData + c * inner, inner * sizeof(uint16_t));
                    }
                }
                
                inputData += channel * inner;
                outputData += num_key_value_groups * channel * inner;
            }
        } else {
            ErrorInFastLLM("Unsupport RepeatKV data type.\n");
        }
        delete[] origin;
    }

    void CpuRepeatOp::Reshape(const std::string &opType, const DataDict &datas, 
                              const FloatDict &floatParams, const IntDict &intParams) {
        Data &input = *(datas.find("input")->second);
        Data &output = *(datas.find("output")->second);
        int axis = intParams.find("axis")->second;
        int repeats = intParams.find("repeats")->second;

        auto dims = input.dims;
        dims[axis] *= repeats;
        output.Resize(dims);
        output.dataType = input.dataType;
    }

    void CpuRepeatOp::Run(const std::string &opType, const DataDict &datas, 
                          const FloatDict &floatParams, const IntDict &intParams) {
        Data &input = *(datas.find("input")->second);
        Data &output = *(datas.find("output")->second);
        int axis = intParams.find("axis")->second;
        int repeats = intParams.find("repeats")->second;

        output.Allocate();

        float *inputData = (float *) input.cpuData;
        float *outputData = (float *) output.cpuData;

        int outer = input.Count(0) / input.Count(axis);
        int dim = input.dims[axis];
        int inner = input.Count(axis + 1);
        for (int i = 0; i < outer; i++) {
            for (int d = 0; d < dim; d++) {
                for (int j = 0; j < repeats; j++) {
                    memcpy(outputData, inputData, inner * sizeof(float));
                    outputData += inner;
                }
                inputData += inner;
            }
        }
    }

    void CpuApplyLognAttnOp::Run(const std::string &opType, const fastllm::DataDict &datas,
                                 const fastllm::FloatDict &floatParams, const fastllm::IntDict &intParams) {
        Data &input = *(datas.find("input")->second);
        Data &lognAttn = *(datas.find("lognAttn")->second);
        Data &positionIds = *(datas.find("positionIds")->second);

        float *inputData = (float *) input.cpuData;
        float *lognData = (float *) lognAttn.cpuData;

        int batch = input.dims[0];
        int seqLen = input.dims[1];
        int spatial = input.Count(2);
        int curPos = (int) ((float *) positionIds.cpuData) [0];
        for (int b = 0; b < batch; b++) {
            float *curInput = inputData + b * seqLen * spatial;
            for (int i = 0; i < seqLen; i++) {
                float logn = lognData[i + curPos];
                for (int s = 0; s < spatial; s++) {
                    curInput[s] *= logn;
                }
                curInput += spatial;
            }
        }
    }

    void CpuLinspaceOp::Reshape(const std::string &opType, const DataDict &datas, 
                                const FloatDict &floatParams, const IntDict &intParams) {
        Data &data = *(datas.find("data")->second);
        float start = floatParams.find("start")->second;
        float end = floatParams.find("end")->second;
        int steps = intParams.find("steps")->second;

        data.Resize({steps});
        data.dataType = DataType::FLOAT32;
    }

    void CpuLinspaceOp::Run(const std::string &opType, const DataDict &datas, 
                            const FloatDict &floatParams, const IntDict &intParams) {
        Data &data = *(datas.find("data")->second);
        float start = floatParams.find("start")->second;
        float end = floatParams.find("end")->second;
        int steps = intParams.find("steps")->second;

        data.Allocate();
        float *cpuData = (float *) data.cpuData;
        float stepSize = (end - start) / (steps - 1);
        for (int i = 0; i < steps; i++) {
            float value = start + i * stepSize;
            cpuData[i] = value;
        }
    }

    void CpuPowOp::Run(const std::string &opType, const DataDict &datas, 
                       const FloatDict &floatParams, const IntDict &intParams) {
        Data &data = *(datas.find("data")->second);
        float p = floatParams.find("p")->second;

        AssertInFastLLM(data.dataType == DataType::FLOAT32, "Pow op only support float32 data.\n");

        int len = data.Count(0);
        float *cpuData = (float *) data.cpuData;
        for (int i = 0; i < len; i++) {
            cpuData[i] = powf(cpuData[i], p);
        }
    }

    void CpuCumProdOp::Run(const std::string &opType, const DataDict &datas, 
                           const FloatDict &floatParams, const IntDict &intParams) {
        Data &input = *(datas.find("input")->second);
        Data &output = *(datas.find("output")->second);
        int axis = intParams.find("axis") != intParams.end() ? intParams.find("axis")->second : 0;

        output.Allocate();

        int outer = input.Count(0) / input.Count(axis);
        int channels = input.dims[axis];
        int inner = input.strides[axis];

        AssertInFastLLM(input.dataType == DataType::FLOAT32, "CumProd op only suport float32 input.\n");

        float *inputData = (float *) input.cpuData;
        float *outputData = (float *) output.cpuData;

        if (outer == 1 && inner == 1) {
            outputData[0] = inputData[0];
            for (int i = 1; i < input.Count(0); i++) {
                outputData[i] = inputData[i] * outputData[i - 1];
            }
            return;
        }
        for (int o = 0; o < outer; o++) {
            for (int c = 0; c < channels; c++) {
                if (c == 0) {
                    for (int i = 0; i < inner; i++) {
                        int x = o * channels * inner + c * inner + i;
                        outputData[x] = inputData[x];
                    }
                } else {
                    for (int i = 0; i < inner; i++) {
                        int x = o * channels * inner + c * inner + i;
                        outputData[x] = inputData[x] * outputData[x - inner];
                    }
                }
            }
        }
    }

    void CpuUniqueOp::Run(const std::string &opType, const DataDict &datas, 
                          const FloatDict &floatParams, const IntDict &intParams) {
        Data &data = *(datas.find("data")->second);

        AssertInFastLLM(data.dataType == DataType::FLOAT32, "Unique op only support float32.\n");

        std::vector<float> v((float *) data.cpuData, (float *) data.cpuData + data.Count(0));
        auto newEnd = std::unique(v.begin(), v.end());
        std::vector<float> newv(v.begin(), newEnd);
        
        data.CopyFrom(Data(DataType::FLOAT32, {(int) newv.size()}, newv));
    }

    void CpuRandnOp::Run(const std::string &opType, const DataDict &datas, 
                         const FloatDict &floatParams, const IntDict &intParams) {
        Data &data = *(datas.find("data")->second);

        AssertInFastLLM(data.dataType == DataType::FLOAT32, "Randn op only support float32.\n");

        int len = data.Count(0);
        std::default_random_engine generator;
        std::normal_distribution<float> dist(0.f, 1.f);

        for (int i = 0; i < len; i++) {
            ((float *) data.cpuData)[i] = dist(generator);
        }
    }

    void CpuImageNormalizeOp::Run(const std::string &opType, const DataDict &datas, 
                                  const FloatDict &floatParams, const IntDict &intParams) {
        Data &image = *(datas.find("image")->second);
        Data &mean = *(datas.find("mean")->second);
        Data &std = *(datas.find("std")->second);
        int toTensor = intParams.find("toTensor") != intParams.end() ? intParams.find("toTensor")->second : 0;

        int batch = image.dims[0];
        int channel = image.dims[1];
        int spatial = image.dims[2] * image.dims[3];
        AssertInFastLLM(channel == mean.Count(0), "Image channel and mean size are different.\n");
        AssertInFastLLM(channel == std.Count(0), "Image channel and std size are different.\n");

        float *imageData = (float *) image.cpuData;
        float *meanData = (float *) mean.cpuData;
        float *stdData = (float *) std.cpuData;
        if (toTensor) {
            for (int i = 0; i < image.Count(0); i++) {
                imageData[i] /= 255.f;
            }
        }

        for (int n = 0; n < batch; n++) {
            for (int c = 0; c < channel; c++) {
                float curMean = meanData[c];
                float curStd = stdData[c];
                float *curImage = imageData + (n * channel + c) * spatial;
                for (int s = 0; s < spatial; s++) {
                    curImage[s] -= curMean;
                    curImage[s] /= curStd;
                }
            }
        }
    }

    void QuantizeInt8Single(float *src, uint8_t *dst, int st, int end, const LowBitConfig &config) {
#ifdef __aarch64__
        quantu8(end - st, config.scale, config.zeroPoint, src + st, dst + st);
#else
        for (int i = st; i < end; i++) {
            dst[i] = config.quantization(src[i]);
        }
#endif
    }

    void DequantizeInt8Single(uint8_t *src, float *dst, int st, int end, const LowBitConfig &config) {
#ifdef __aarch64__
        dequantu8(end - st, config.scale, config.zeroPoint, src + st, dst + st);
#else
        for (int i = st; i < end; i++) {
            dst[i] = config.invQuantization(src[i]);
        }
#endif        
    }

    void CpuQuantizeInt8Op::Reshape(const std::string &opType, const DataDict &datas, 
                                    const FloatDict &floatParams, const IntDict &intParams) {
        Data &input = *(datas.find("input")->second);
        Data &output = *(datas.find("output")->second);

        AssertInFastLLM(input.dataType == DataType::FLOAT32, "QuantizeInt8 op only support float32 input.\n");

        output.Resize(input.dims);
        output.dataType = DataType::INT8;
    }

    void CpuQuantizeInt8Op::Run(const std::string &opType, const DataDict &datas, 
                                const FloatDict &floatParams, const IntDict &intParams) {
        Data &input = *(datas.find("input")->second);
        Data &output = *(datas.find("output")->second);

        float *inputData = (float *) input.cpuData;
        uint8_t *outputData = (uint8_t *) output.cpuData;

        int len = input.Count(0);
        output.Allocate();

        float minValue = inputData[0];
        float maxValue = inputData[0];
        int i = 0;
#ifdef __aarch64__
        float32x4_t minValuex4 = vdupq_n_f32(minValue);
        float32x4_t maxValuex4 = vdupq_n_f32(maxValue);
        for (; i + 3 < len; i += 4) {
            minValuex4 = vminq_f32(minValuex4, vld1q_f32(inputData + i));
            maxValuex4 = vmaxq_f32(maxValuex4, vld1q_f32(inputData + i));
        }
        for (int j = 0; j < 4; j++) {
            minValue = std::min(minValue, minValuex4[j]);
            maxValue = std::max(maxValue, maxValuex4[j]);
        }
#endif
        for (; i < len; i++) {
            minValue = std::min(minValue, inputData[i]);
            maxValue = std::max(maxValue, inputData[i]);
        }

        auto config = LowBitConfig(minValue, maxValue, 8, 0);
        output.perChannelsConfigs.push_back(config);

        int threadNum = GetThreads();
        auto pool = GetPool();
        int per = len / threadNum;

        std::vector<std::future<void>> futures;
        for (int id = 0; id < threadNum - 1; id++) {
            int st = id * per;
            int end = st + per;
            futures.push_back(pool->Submit(QuantizeInt8Single, inputData, outputData, st, end, config));
        }
        QuantizeInt8Single(inputData, outputData, per * (threadNum - 1), len, config);
        for (int o = 0; o < futures.size(); o++) {
            futures[o].get();
        }
    }

    void CpuDequantizeInt8Op::Reshape(const std::string &opType, const DataDict &datas, 
                                      const FloatDict &floatParams, const IntDict &intParams) {
        Data &input = *(datas.find("input")->second);
        Data &output = *(datas.find("output")->second);

        AssertInFastLLM(input.dataType == DataType::INT8, "DequantizeInt8 op only support int8 input.\n");

        output.Resize(input.dims);
        output.dataType = DataType::FLOAT32;
    }

    void CpuDequantizeInt8Op::Run(const std::string &opType, const DataDict &datas, 
                                  const FloatDict &floatParams, const IntDict &intParams) {
        Data &input = *(datas.find("input")->second);
        Data &output = *(datas.find("output")->second);

        uint8_t *inputData = (uint8_t *) input.cpuData;
        float *outputData = (float *) output.cpuData;

        int len = input.Count(0);
        output.Allocate();

        auto config = input.perChannelsConfigs[0];
        int threadNum = GetThreads();
        auto pool = GetPool();
        int per = len / threadNum;

        std::vector<std::future<void>> futures;
        for (int id = 0; id < threadNum - 1; id++) {
            int st = id * per;
            int end = st + per;
            futures.push_back(pool->Submit(DequantizeInt8Single, inputData, outputData, st, end, config));
        }
        DequantizeInt8Single(inputData, outputData, per * (threadNum - 1), len, config);
        for (int o = 0; o < futures.size(); o++) {
            futures[o].get();
        }
    }
}