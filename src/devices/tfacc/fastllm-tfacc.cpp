//
// Created by siemon on 7/24/23.
//

#include "utils.h"
#include "devices/tfacc/fastllm-tfacc.h"

#include <Data.h>
#include <Operations.h>

#include <numa.h>
#include <future>

inline vector<int> ConfigureTFACC(int coreNum, int chipNum) {
    vector<int> tfaccs;
    if (chipNum <= 0) {
        chipNum = TF_TFNN_GetChipNum();
    } else {
        chipNum = min(chipNum, TF_TFNN_GetChipNum());
    }
    assert(coreNum <= chipNum * 8);
    vector<int> chips;
    for (int i = 0; i < TF_TFNN_GetChipNum(); i++) {
        int numaMemMask = numa_get_membind_compat().n[0];
        if (!(numaMemMask & (1 << i))) {
            continue;
        }
        chips.push_back(i);
        if (chips.size() == chipNum) {
            break;
        }
    }
    chipNum = chips.size();
    coreNum = min(coreNum, chipNum * 8);
    int each = coreNum / chipNum;
    int last = coreNum - (chipNum - 1) * each;
    for (int j = 0; j < each; j++) {
        for (int i = 0; i < chipNum; i++) {
            tfaccs.push_back(chips[i] * 8 + j);
        }
    }
    for (int i = 0; i < (last - each); i++) {
        tfaccs.push_back(chips.back() * 8 + (each + i));
    }
    return tfaccs;
}

inline void ConfigureKMRound(int k, int m, int cores, int *_per_k, int *_per_m, int *_k_round, int *_m_round) {
    // 切分查找表，如果当前形状已经计算过，直接查找结果返回
    if (FastllmTfaccKMRoundMap.find(k * m + m) != FastllmTfaccKMRoundMap.end()) {
        *_per_m = FastllmTfaccKMRoundMap[k * m + m].first;    
        *_per_k = FastllmTfaccKMRoundMap[k * m + m].second;
        *_m_round = (m - 1) / *_per_m + 1;
        *_k_round = (k - 1) / *_per_k + 1;
        return;
    }

    // 初始切割条件
    int per_m = 4096;
    int per_k = 4096;
    int m_round = (m - 1) / per_m + 1;
    int k_round = (k - 1) / per_k + 1;
    int per_m_lower_limit = 1024;
    int per_k_lower_limit = 1024;

    // 粗略缩放，按照2倍递减
    while (m_round * k_round <= cores && per_m / 2 >= per_m_lower_limit && per_k / 2 >= per_k_lower_limit) {
        int new_per_m = per_m / 2;
        int new_m_round = (m - 1) / new_per_m + 1;
        if (new_m_round * k_round <= cores) {
            per_m = new_per_m;
            m_round = new_m_round;
        } else {
            break;
        }

        int new_per_k = per_k / 2;
        int new_k_round = (k - 1) / new_per_k + 1;
        if (m_round * new_k_round <= cores) {
            per_k = new_per_k;
            k_round = new_k_round;
        } else {
            break;
        }
    }

    // 进一步缩放，按照64对齐切割
    if (m_round * k_round < cores) {
        if (cores % m_round == 0) {
            int new_k_round = cores / m_round;
            if ((k / new_k_round) % 64 == 0) {
                k_round = new_k_round;
                per_k = k / k_round;
            }
        }
        if (cores % k_round == 0) {
            int new_m_round = cores / k_round;
            if ((m / new_m_round) % 64 == 0) {
                m_round = new_m_round;
                per_m = m / m_round;
            }
        }
        while (true) {
            if (m_round * (k_round + 1) <= cores && k % (64 * (k_round + 1)) == 0) {
                k_round++;
                per_k = k / k_round;
            } else {
                break;
            }
        }
        while (true) {
            if (k_round * (m_round + 1) <= cores && m % (64 * (m_round + 1)) == 0) {
                m_round++;
                per_m = m / m_round;
            } else {
                break;
            }
        }
    }

    // 处理大矩阵
    while (m_round * k_round >= cores * 4 && per_k * 2 <= 65536) {
        per_m *= 2;
        per_k *= 2;
        m_round = (m - 1) / per_m + 1;
        k_round = (k - 1) / per_k + 1;
    }
    while (m_round * k_round >= cores * 2 && per_k * 2 <= 65536) {
        per_k *= 2;
        k_round = (k - 1) / per_k + 1;
    }

    if (m % (64 * m_round) == 0) {
        per_m = m / m_round;
    }
    if (k % (64 * k_round) == 0) {
        per_k = k / k_round;
    }

    *_per_k = per_k;
    *_per_m = per_m;
    *_k_round = k_round;
    *_m_round = m_round;
    FastllmTfaccKMRoundMap[k * m + m].first = per_m;
    FastllmTfaccKMRoundMap[k * m + m].second = per_k;
}

inline void FastllmTfaccAccumulateFloat32(float *x1, float *x2, float *y, size_t len) {
    size_t i = 0;
#ifdef __aarch64__
    for (; i + 3 < len; i += 4) {
        vst1q_f32(y + i, vaddq_f32(vld1q_f32(x1 + i), vld1q_f32(x2 + i)));
    }
#endif
    for (; i < len; i++) {
        y[i] = x1[i] + x2[i];
    }
}

inline void FastllmTfaccAccumulateFloat16(uint16_t *x1, uint16_t *x2, uint16_t *y, size_t len) {
    size_t i = 0;
#ifdef __ARM_FEATURE_FP16_VECTOR_ARITHMETIC
    for (; i + 7 < len; i += 8) {
        vst1q_f16((float16_t *) y + i, vaddq_f16(vld1q_f16((float16_t *) x1 + i), vld1q_f16((float16_t *) x2 + i)));
    }
#endif
    for (; i < len; i++) {
        y[i] = float_to_half(half_to_float(x1[i]) + half_to_float(x2[i]));
    }
}

template<typename pointerType>
void FastllmTfaccCopyStride(pointerType *dst, pointerType *src, int len, int round, int dst_stride, int src_stride) {
    for (int i = 0; i < round; i++) {
        memcpy(dst + i * dst_stride, src + i * src_stride, len * sizeof(pointerType));
    }
}

void FastllmTfaccQuantization(uint8_t *dst, float *src, int len, int round, int dst_stride, int src_stride, 
                              tfdl::PerChannelConfig config) {
    for (int i = 0; i < round; i++) {
        config.configs[i].quantall(len, src + i * src_stride, dst + i * dst_stride);
    }
}

void FastllmTfaccQuantization(uint8_t *dst, uint16_t *src, int len, int round, int dst_stride, int src_stride,
                              tfdl::PerChannelConfig config) {
    for (int i = 0; i < round; i++) {
        uint8_t *dst_walk = dst + i * dst_stride;
        uint16_t *src_walk = src + i * src_stride;
        int j = 0;
#ifdef __ARM_FEATURE_FP16_VECTOR_ARITHMETIC
        float16x8_t vscale = vdupq_n_f16((float16_t) config.configs[i].getScale());
        float16x8_t vzero = vdupq_n_f16((float16_t) (config.configs[i].getZeroPoint() + 0.5f));
        float16x8_t vmax = vdupq_n_f16((float16_t) 255.f);
        float16x8_t vmin = vdupq_n_f16((float16_t) 0.f);
        for (; j + 7 < len; j += 8) {
            float16x8_t vorigin_data = vld1q_f16((float16_t *) src_walk + j);
            float16x8_t vquant = vminq_f16(vmaxq_f16(vaddq_f16(vdivq_f16(vorigin_data, vscale), vzero), vmin), vmax);
            vst1_u8(dst_walk + j, vmovn_u16(vcvtq_u16_f16(vquant)));
        }
#endif 
        for (; j < len; j++) {
            dst_walk[j] = config.configs[i].quantization(half_to_float(src_walk[j]));
        }
    }
}

void FastllmTfaccInitBlasop() {
    if (FastllmTfaccBlasopCache.empty()) {
        int maxCmdNum = 60000;
        int total_cores = 8 * TF_TFNN_GetChipNum();
        for (int i = 0; i < total_cores; i++) {
            FastllmTfaccBlasopCache.push_back(new tfacc40t::BlasopList(TF_TFNN_GetChipId(i), maxCmdNum));
        }
    }
}

void FastllmTfaccClearBlasop() {
    for (auto blasopList : FastllmTfaccBlasopCache) {
        delete blasopList;
    }
    FastllmTfaccBlasopCache.clear();
}

void FastllmTfaccClearMemory() {
    for (auto weightMapPair : FastllmTfaccWeightMap) {
        for (auto weight : weightMapPair.second) {
            delete weight;
        }
    }
    FastllmTfaccWeightMap.clear();
}

void FastllmTfaccReleaseTempMemory() {
    for (auto &weightRealSpace : FastllmTfaccWeightRealSpace) {
        for (auto data : weightRealSpace.second) {
            delete[] (char *) data;
        }
        weightRealSpace.second.clear();
    }
    FastllmTfaccWeightRealSpace.clear();
    // this is a trick move, clear tfacc`s memory will gain some efficiency benefit
    tfacc40t::ReleaseAllDeviceMemory();
}

template <typename pointerType>
std::vector<tfdl::TFDataInt8 *> FastllmLinearPrepareInputData(pointerType *input, int n, int m, int per_m, int m_round, fastllm::ThreadPool *pool) {
    vector<tfdl::TFDataInt8 *> temp_input;
    if (typeid(pointerType) == typeid(float)) {
        if (n == 1) {
            auto input_tf = tfdl::TFDataFloat({n, m}, (float *) input);
            float minValue = input_tf.GetMinValue();
            float maxValue = input_tf.GetMaxValue();
            for (int i = 0; i < m_round; i++) {
                int cur_m = min(per_m, m - (i * per_m));
                temp_input.push_back(new tfdl::TFDataInt8(minValue, maxValue, {n, cur_m}, 
                                     vector<float>((float *) input + i * per_m, (float *) input + i * per_m + cur_m)));
            }
        } else {
            auto input_tf = tfdl::TFDataFloat({n, m}, (float *) input);
            tfdl::PerChannelConfig perChannelConfig;
            for (int i = 0; i < n; i++) {
                int st = i * m;
                int end = i * m + m;
                float minValue = input_tf.GetMinValue(st, end), maxValue = input_tf.GetMaxValue(st, end);
                perChannelConfig.configs.push_back(tfdl::QuantizationConfig(minValue, maxValue));
            }
            for (int i = 0; i < m_round; i++) {
                int cur_m = min(per_m, m - (i * per_m));
                temp_input.push_back(new tfdl::TFDataInt8(0, 0, {n, cur_m}));
                temp_input.back()->SetPerChannelConfig(perChannelConfig);
            }

            // quantize input
            vector<future<void>> futures;
            for (int i = 0; i < m_round; i++) {
                int cur_m = min(per_m, m - (i * per_m));
                futures.push_back(pool->Submit([](uint8_t *dst, float *src, int len, int round, int dst_stride, int src_stride, tfdl::PerChannelConfig config){          
                    FastllmTfaccQuantization(dst, src, len, round, dst_stride, src_stride, config);
                }, temp_input[i]->GetInt8RawData(), (float *) input + i * per_m, cur_m, n, cur_m, m, perChannelConfig));
            }
            for (auto &one_future : futures) {
                one_future.get();
            }
        }
    } else if (typeid(pointerType) == typeid(uint16_t)) {
        if (n == 1) {
            for (int i = 0; i < m_round; i++) {
                int cur_m = min(per_m, m - i * per_m);
                tfdl::TFDataFloat16 origin = tfdl::TFDataFloat16({n, cur_m}, (uint16_t *) input + i * per_m);
                temp_input.push_back(new tfdl::TFDataInt8(&origin));
            }
        } else {
            auto input_tf = tfdl::TFDataFloat16({n, m}, (uint16_t *) input);
            tfdl::PerChannelConfig perChannelConfig;
            for (int i = 0; i < n; i++) {
                int st = i * m;
                int end = i * m + m;
                float minValue = input_tf.GetMinValue(st, end), maxValue = input_tf.GetMaxValue(st, end);
                perChannelConfig.configs.push_back(tfdl::QuantizationConfig(minValue, maxValue));
            }
            for (int i = 0; i < m_round; i++) {
                int cur_m = min(per_m, m - (i * per_m));
                temp_input.push_back(new tfdl::TFDataInt8(0, 0, {n, cur_m}));
                temp_input.back()->SetPerChannelConfig(perChannelConfig);
            }

            // quantize input
            vector<future<void>> futures;
            for (int i = 0; i < m_round; i++) {
                int cur_m = min(per_m, m - (i * per_m));
                futures.push_back(pool->Submit([](uint8_t *dst, uint16_t *src, int len, int round, int dst_stride, int src_stride, tfdl::PerChannelConfig config){          
                    FastllmTfaccQuantization(dst, src, len, round, dst_stride, src_stride, config);
                }, temp_input[i]->GetInt8RawData(), (uint16_t *) input + i * per_m, cur_m, n, cur_m, m, perChannelConfig));
            }
            for (auto &one_future : futures) {
                one_future.get();
            }
        }
    } else {
        ErrorInFastLLM("wrong input data type.\n");
    }

    return temp_input;
}

template <typename tfDataType>
vector<tfDataType *> FastllmLinearPrepareOutputData(int n, int k, int per_k, int m_round, int k_round) {
    std::vector<tfDataType *> temp_output;
    for (int k_iter = 0; k_iter < k_round; k_iter++) {
        int cur_k = min(per_k, k - (k_iter * per_k));
        for (int m_iter = 0; m_iter < m_round; m_iter++) {
            temp_output.push_back(new tfDataType({n, cur_k}));
        }
    }
    return temp_output;
}

void FastllmTfaccLinearFloat32WFloat32D(float *input, float *output, float *weight, float *bias, int n, int m, int k, 
                                        fastllm::ThreadPool *pool) {
    auto t0 = chrono::system_clock::now();

    // collect all available tfacc cores
    std::vector<int> all_tfacc = ConfigureTFACC(fastllm::GetThreads(), TF_TFNN_GetChipNum());

    int per_n = 8 * all_tfacc.size();
    int n_round = (n - 1) / per_n + 1;
    if (n_round > 1) {
        for (int i = 0; i < n_round; i++) {
            int cur_n = min(per_n, n - (i * per_n));
            float *cur_input = input + i * per_n * m;
            float *cur_output = output + i * per_n * k;
            FastllmTfaccLinearFloat32WFloat32D(cur_input, cur_output, weight, bias, cur_n, m, k, pool);
        }
        return;
    }

    int per_k, per_m, k_round, m_round;
    ConfigureKMRound(k, m, all_tfacc.size(), &per_k, &per_m, &k_round, &m_round);

    // create space for blasop
    FastllmTfaccInitBlasop();

    // prepare weight
    long long int weight_key = (long long int) weight;
    if (FastllmTfaccWeightMap[weight_key].empty()) {
        printf("[%d, %d] x [%d, %d] -> [%d, %d]\n", n, m, k, m, n, k);
        printf("prepare weight, n_round: %d, m_round: %d, k_round: %d per_m: %d, per_k: %d\n", n_round, m_round, k_round, per_m, per_k);

        vector<future<void>> futures;
        for (int k_iter = 0; k_iter < k_round; k_iter++) {
            int cur_k = min(per_k, k - (k_iter * per_k));
            for (int m_iter = 0; m_iter < m_round; m_iter++) {
                int cur_m = min(per_m, m - (m_iter * per_m));

                // create weight for tfacc
                auto real_space = new float[cur_k * cur_m];
                auto cur_weight = new tfdl::TFDataFloat({cur_k, cur_m}, real_space);
                
                FastllmTfaccWeightRealSpace[weight_key].push_back((void *) real_space);
                FastllmTfaccWeightMap[weight_key].push_back(cur_weight);

                // copy weight from cpu
                futures.push_back(pool->Submit([](float *dst, float *src, int len, int round, int dst_stride, int src_stride) {
                    FastllmTfaccCopyStride<float>(dst, src, len, round, dst_stride, src_stride);
                }, cur_weight->GetFloatRawData(), 
                   weight + k_iter * per_k * m + m_iter * per_m, 
                   cur_m, cur_k, cur_m, m));
            }
        }
        for (auto &one_future : futures) {
            one_future.get();
        }
    }

    // quantize input data
    vector<tfdl::TFDataInt8 *> temp_input = FastllmLinearPrepareInputData(input, n, m, per_m, m_round, pool);
    
    // prepare temp output buffer
    vector<tfdl::TFDataFloat *> temp_output = FastllmLinearPrepareOutputData<tfdl::TFDataFloat>(n, k, per_k, m_round, k_round);

    auto t1 = chrono::system_clock::now();

    // run with tfacc
    vector<future<void>> futures;
    int i = 0;
    for (int k_iter = 0; k_iter < k_round; k_iter++) {
        for (int m_iter = 0; m_iter < m_round; m_iter++) {
            tfdl::TFDataInt8 *cur_input = temp_input[m_iter];
            tfdl::TFDataFloat *cur_output = temp_output[i];
            tfdl::TFDataFloat *cur_weight = (tfdl::TFDataFloat *) FastllmTfaccWeightMap[weight_key][i];
            int cur_core = all_tfacc[i % all_tfacc.size()];
            auto cur_blasop = FastllmTfaccBlasopCache[cur_core];
            futures.push_back(pool->Submit([](tfdl::TFDataInt8 *input, tfdl::TFDataFloat *output, 
                                              tfdl::TFDataFloat *weight, tfdl::TFDataFloat *bias, int device, void *blasop) {
                tfacc40t::InnerProduct(input, output, weight, bias, device, blasop);
            }, cur_input, cur_output, cur_weight, nullptr, cur_core, cur_blasop));

            i++;
            if (i % all_tfacc.size() == 0) {
                for (auto &one_future : futures) {
                    one_future.get();
                }
                futures.clear();
            }
        }
    }
    for (auto &one_future : futures) {
        one_future.get();
    }
    futures.clear();

    auto t2 = chrono::system_clock::now();

    // post process temp output
    // 1.accumulate m direction
    if (m_round > 1) {
        for (int k_iter = 0; k_iter < k_round; k_iter++) {
            int cur_k = min(per_k, k - (k_iter * per_k));
            float *dst = temp_output[k_iter * m_round]->GetFloatRawData();
            vector<float *> src;
            for (int m_iter = 1; m_iter < m_round; m_iter++) {
                src.push_back(temp_output[k_iter * m_round + m_iter]->GetFloatRawData());
            }
            int per_n = 64;
            for (int n_iter = 0; n_iter < n; n_iter += per_n) {
                int cur_n = min(per_n, n - n_iter);
                futures.push_back(pool->Submit([](float *dst, vector<float *> src, int len, int shift){
                    for (int i = 0; i < src.size(); i++) {
                        FastllmTfaccAccumulateFloat32(src[i] + shift, dst + shift, dst + shift, len);
                    }
                }, dst, src, cur_n * cur_k, n_iter * cur_k));
            }
        }
        for (auto &one_future : futures) {
            one_future.get();
        }
        futures.clear();
    }

    // 2.copy k direction
    for (int k_iter = 0; k_iter < k_round; k_iter++) {
        int cur_k = min(per_k, k - (k_iter * per_k));
        float *dst = output + k_iter * per_k;
        float *src = temp_output[k_iter * m_round]->GetFloatRawData();
        futures.push_back(pool->Submit(FastllmTfaccCopyStride<float>, dst, src, cur_k, n, k, cur_k));
    }
    for (auto &one_future : futures) {
        one_future.get();
    }
    futures.clear();

    // 3.add bias
    if (bias) {
        if (n == 1) {
            FastllmTfaccAccumulateFloat32(bias, output, output, k);
        } else {
            int per = max((int) (n / all_tfacc.size()), 1);
            for (int n_iter = 0; n_iter < n;) {
                int cur_n = min(per, n - n_iter);

                float *bias_walk = bias;
                float *output_walk = output + n_iter * k;
                futures.push_back(pool->Submit([](float *dst, float *src, int round, int len) {
                    for (int i = 0; i < round; i++) {
                        FastllmTfaccAccumulateFloat32(dst + i * len, src, dst + i * len, len);
                    }
                }, output_walk, bias_walk, cur_n, k));
                n_iter += cur_n;
            }
            for (auto &one_future : futures) {
                one_future.get();
            }
            futures.clear();
        }
    }

    // clear temp data
    for (auto in : temp_input) {
        delete in;
    }
    temp_input.clear();

    for (auto out : temp_output) {
        delete out;
    }
    temp_output.clear();

    for (int tfacc : all_tfacc) {
        FastllmTfaccBlasopCache[tfacc]->Clear();
    }

    auto t3 = chrono::system_clock::now();
}

void FastllmTfaccLinearFloat16WFloat32D(float *input, float *output, uint16_t *weight, float *bias, int n, int m, int k,
                                        fastllm::ThreadPool *pool) {
    // collect all available tfacc cores
    std::vector<int> all_tfacc = ConfigureTFACC(fastllm::GetThreads(), TF_TFNN_GetChipNum());

    int per_n = 8 * all_tfacc.size();
    int n_round = (n - 1) / per_n + 1;
    if (n_round > 1) {
        for (int i = 0; i < n_round; i++) {
            int cur_n = min(per_n, n - (i * per_n));
            float *cur_input = input + i * per_n * m;
            float *cur_output = output + i * per_n * k;
            FastllmTfaccLinearFloat16WFloat32D(cur_input, cur_output, weight, bias, cur_n, m, k, pool);
        }
        return;
    }

    int per_k, per_m, k_round, m_round;
    ConfigureKMRound(k, m, all_tfacc.size(), &per_k, &per_m, &k_round, &m_round);

    // create space for blasop
    FastllmTfaccInitBlasop();

    // prepare weight
    long long int weight_key = (long long int) weight;
    if (FastllmTfaccWeightMap[weight_key].empty()) {
        printf("[%d, %d] x [%d, %d] -> [%d, %d]\n", n, m, k, m, n, k);
        printf("prepare weight, n_round: %d, m_round: %d, k_round: %d per_m: %d, per_k: %d\n", n_round, m_round, k_round, per_m, per_k);

        vector<future<void>> futures;
        for (int k_iter = 0; k_iter < k_round; k_iter++) {
            int cur_k = min(per_k, k - k_iter * per_k);
            for (int m_iter = 0; m_iter < m_round; m_iter++) {
                int cur_m = min(per_m, m - m_iter * per_m);

                // create weight for tfacc
                auto real_space = new uint16_t[cur_k * cur_m];
                auto cur_weight = new tfdl::TFDataFloat16({cur_k, cur_m}, real_space);

                FastllmTfaccWeightRealSpace[weight_key].push_back((void *) real_space);
                FastllmTfaccWeightMap[weight_key].push_back(cur_weight);

                futures.push_back(pool->Submit([](uint16_t *dst, uint16_t *src, int len, int round, int dst_stride, int src_stride){
                    FastllmTfaccCopyStride<uint16_t>(dst, src, len, round, dst_stride, src_stride);
                }, cur_weight->GetFloat16RawData(),
                   weight + k_iter * per_k * m + m_iter * per_m,
                   cur_m, cur_k, cur_m, m));
            }
        }
        for (auto &one_future : futures) {
            one_future.get();
        }
    }

    // quantize input data
    vector<tfdl::TFDataInt8 *> temp_input = FastllmLinearPrepareInputData(input, n, m, per_m, m_round, pool);

    // prepare temp output buffer
    vector<tfdl::TFDataFloat *> temp_output = FastllmLinearPrepareOutputData<tfdl::TFDataFloat>(n, k, per_k, m_round, k_round);

    // run with tfacc
    vector<future<void>> futures;
    int i = 0;
    for (int k_iter = 0; k_iter < k_round; k_iter++) {
        for (int m_iter = 0; m_iter < m_round; m_iter++) {
            tfdl::TFDataInt8 *cur_input = temp_input[m_iter];
            tfdl::TFDataFloat *cur_output = temp_output[i];
            tfdl::TFDataFloat16 *cur_weight = (tfdl::TFDataFloat16 *) FastllmTfaccWeightMap[weight_key][i];
            int cur_core = all_tfacc[i % all_tfacc.size()];
            auto cur_blasop = FastllmTfaccBlasopCache[cur_core];
            futures.push_back(pool->Submit([](tfdl::TFDataInt8 *input, tfdl::TFDataFloat *output, 
                                              tfdl::TFDataFloat16 *weight, tfdl::TFDataFloat *bias, int device, void *blasop) {
                tfacc40t::InnerProduct(input, output, weight, bias, device, blasop);
            }, cur_input, cur_output, cur_weight, nullptr, cur_core, cur_blasop));

            i++;
            if (i % all_tfacc.size() == 0) {
                for (auto &one_future : futures) {
                    one_future.get();
                }
                futures.clear();
            }
        }
    }
    for (auto &one_future : futures) {
        one_future.get();
    }
    futures.clear();

    // post process temp output
    // 1.accumulate m direction
    if (m_round > 1) {
        for (int k_iter = 0; k_iter < k_round; k_iter++) {
            int cur_k = min(per_k, k - (k_iter * per_k));
            float *dst = temp_output[k_iter * m_round]->GetFloatRawData();
            vector<float *> src;
            for (int m_iter = 1; m_iter < m_round; m_iter++) {
                src.push_back(temp_output[k_iter * m_round + m_iter]->GetFloatRawData());
            }
            int per_n = 64;
            for (int n_iter = 0; n_iter < n; n_iter += per_n) {
                int cur_n = min(per_n, n - n_iter);
                futures.push_back(pool->Submit([](float *dst, vector<float *> src, int len, int shift){
                    for (int i = 0; i < src.size(); i++) {
                        FastllmTfaccAccumulateFloat32(src[i] + shift, dst + shift, dst + shift, len);
                    }
                }, dst, src, cur_n * cur_k, n_iter * cur_k));
            }
        }
        for (auto &one_future : futures) {
            one_future.get();
        }
        futures.clear();
    }

    // 2.copy k direction
    for (int k_iter = 0; k_iter < k_round; k_iter++) {
        int cur_k = min(per_k, k - (k_iter * per_k));
        float *dst = output + k_iter * per_k;
        float *src = temp_output[k_iter * m_round]->GetFloatRawData();
        futures.push_back(pool->Submit(FastllmTfaccCopyStride<float>, dst, src, cur_k, n, k, cur_k));
    }
    for (auto &one_future : futures) {
        one_future.get();
    }
    futures.clear();

    // 3.add bias
    if (bias) {
        if (n == 1) {
            FastllmTfaccAccumulateFloat32(bias, output, output, k);
        } else {
            int per = max((int) (n / all_tfacc.size()), 1);
            for (int n_iter = 0; n_iter < n;) {
                int cur_n = min(per, n - n_iter);

                float *bias_walk = bias;
                float *output_walk = output + n_iter * k;
                futures.push_back(pool->Submit([](float *dst, float *src, int round, int len) {
                    for (int i = 0; i < round; i++) {
                        FastllmTfaccAccumulateFloat32(dst + i * len, src, dst + i * len, len);
                    }
                }, output_walk, bias_walk, cur_n, k));
                n_iter += cur_n;
            }
            for (auto &one_future : futures) {
                one_future.get();
            }
            futures.clear();
        }
    }

    // clear temp data
    for (auto in : temp_input) {
        delete in;
    }
    temp_input.clear();

    for (auto out : temp_output) {
        delete out;
    }
    temp_output.clear();

    for (int tfacc : all_tfacc) {
        FastllmTfaccBlasopCache[tfacc]->Clear();
    }
}

void FastllmTfaccLinearFloat16WFloat16D(uint16_t *input, uint16_t *output, uint16_t *weight, float *bias, int n, int m, int k,
                                        fastllm::ThreadPool *pool) {
#ifdef __ARM_FEATURE_FP16_VECTOR_ARITHMETIC                                            
    // collect all available tfacc cores
    std::vector<int> all_tfacc = ConfigureTFACC(fastllm::GetThreads(), TF_TFNN_GetChipNum());

    int per_n = 8 * all_tfacc.size();
    int n_round = (n - 1) / per_n + 1;
    if (n_round > 1) {
        for (int i = 0; i < n_round; i++) {
            int cur_n = min(per_n, n - (i * per_n));
            uint16_t *cur_input = input + i * per_n * m;
            uint16_t *cur_output = output + i * per_n * k;
            FastllmTfaccLinearFloat16WFloat16D(cur_input, cur_output, weight, bias, cur_n, m, k, pool);
        }
        return;
    }

    int per_k, per_m, k_round, m_round;
    ConfigureKMRound(k, m, all_tfacc.size(), &per_k, &per_m, &k_round, &m_round);

    // create space for blasop
    FastllmTfaccInitBlasop();

    // prepare weight
    long long int weight_key = (long long int) weight;
    if (FastllmTfaccWeightMap[weight_key].empty()) {
        printf("[%d, %d] x [%d, %d] -> [%d, %d]\n", n, m, k, m, n, k);
        printf("prepare weight, n_round: %d, m_round: %d, k_round: %d per_m: %d, per_k: %d\n", n_round, m_round, k_round, per_m, per_k);

        vector<future<void>> futures;
        for (int k_iter = 0; k_iter < k_round; k_iter++) {
            int cur_k = min(per_k, k - k_iter * per_k);
            for (int m_iter = 0; m_iter < m_round; m_iter++) {
                int cur_m = min(per_m, m - m_iter * per_m);

                // create weight for tfacc
                auto real_space = new uint16_t[cur_k * cur_m];
                auto cur_weight = new tfdl::TFDataFloat16({cur_k, cur_m}, real_space);

                FastllmTfaccWeightRealSpace[weight_key].push_back((void *) real_space);
                FastllmTfaccWeightMap[weight_key].push_back(cur_weight);

                futures.push_back(pool->Submit([](uint16_t *dst, uint16_t *src, int len, int round, int dst_stride, int src_stride){
                    FastllmTfaccCopyStride<uint16_t>(dst, src, len, round, dst_stride, src_stride);
                }, cur_weight->GetFloat16RawData(),
                   weight + k_iter * per_k * m + m_iter * per_m,
                   cur_m, cur_k, cur_m, m));
            }
        }
        for (auto &one_future : futures) {
            one_future.get();
        }
    }

    // quantize input data
    vector<tfdl::TFDataInt8 *> temp_input = FastllmLinearPrepareInputData(input, n, m, per_m, m_round, pool);

    // prepare temp output buffer
    vector<tfdl::TFDataFloat16 *> temp_output = FastllmLinearPrepareOutputData<tfdl::TFDataFloat16>(n, k, per_k, m_round, k_round);

    // run with tfacc
    vector<future<void>> futures;
    int i = 0;
    for (int k_iter = 0; k_iter < k_round; k_iter++) {
        for (int m_iter = 0; m_iter < m_round; m_iter++) {
            tfdl::TFDataInt8 *cur_input = temp_input[m_iter];
            tfdl::TFDataFloat16 *cur_output = temp_output[i];
            tfdl::TFDataFloat16 *cur_weight = (tfdl::TFDataFloat16 *) FastllmTfaccWeightMap[weight_key][i];
            int cur_core = all_tfacc[i % all_tfacc.size()];
            auto cur_blasop = FastllmTfaccBlasopCache[cur_core];
            futures.push_back(pool->Submit([](tfdl::TFDataInt8 *input, tfdl::TFDataFloat16 *output, 
                                              tfdl::TFDataFloat16 *weight, tfdl::TFDataFloat *bias, int device, void *blasop) {
                tfacc40t::InnerProduct(input, output, weight, bias, device, blasop);
            }, cur_input, cur_output, cur_weight, nullptr, cur_core, cur_blasop));

            i++;
            if (i % all_tfacc.size() == 0) {
                for (auto &one_future : futures) {
                    one_future.get();
                }
                futures.clear();
            }
        }
    }
    for (auto &one_future : futures) {
        one_future.get();
    }
    futures.clear();

    // post process temp output
    // 1. accumulate m direction
    if (m_round > 1) {
        for (int k_iter = 0; k_iter < k_round; k_iter++) {
            int cur_k = min(per_k, k - k_iter * per_k);
            uint16_t *dst = temp_output[k_iter * m_round]->GetFloat16RawData();
            vector<uint16_t *> src;
            for (int m_iter = 1; m_iter < m_round; m_iter++) {
                src.push_back(temp_output[k_iter * m_round + m_iter]->GetFloat16RawData());
            }
            int per_n = 64;
            for (int n_iter = 0; n_iter < n; n_iter += per_n) {
                int cur_n = min(per_n, n - n_iter);
                futures.push_back(pool->Submit([](uint16_t *dst, vector<uint16_t *> src, int len, int shift){
                    for (int i = 0; i < src.size(); i++) {
                        FastllmTfaccAccumulateFloat16(src[i] + shift, dst + shift, dst + shift, len);
                    }
                }, dst, src, cur_n * cur_k, n_iter * cur_k));
            }
        }
        for (auto &one_future : futures) {
            one_future.get();
        }
        futures.clear();
    }

    // 2. copy k direction
    for (int k_iter = 0; k_iter < k_round; k_iter++) {
        int cur_k = min(per_k, k - k_iter * per_k);
        uint16_t *dst = output + k_iter * per_k;
        uint16_t *src = temp_output[k_iter * m_round]->GetFloat16RawData();
        futures.push_back(pool->Submit(FastllmTfaccCopyStride<uint16_t>, dst, src, cur_k, n, k, cur_k));
    }
    for (auto &one_future : futures) {
        one_future.get();
    }
    futures.clear();
    
    // add bias
    if (bias) {
        long long int bias_key = (long long int) bias;
        if (FastllmTfaccWeightMap.find(bias_key) == FastllmTfaccWeightMap.end()) {
            uint16_t *bias_half = new uint16_t[k];
            FastllmTfaccWeightMap[bias_key].push_back(new tfdl::TFDataFloat16({k}, bias_half));
            for (int i = 0; i < k; i++) {
                bias_half[i] = float_to_half(bias[i]);
            }
        }

        uint16_t *bias_half = ((tfdl::TFDataFloat16 *) FastllmTfaccWeightMap[bias_key][0])->GetFloat16RawData();

        if (n == 1) {
            FastllmTfaccAccumulateFloat16(bias_half, output, output, k);
        } else {
            int per = max((int) (n / all_tfacc.size()), 1);
            for (int n_iter = 0; n_iter < n;) {
                int cur_n = min(per, n - n_iter);

                uint16_t *bias_walk = bias_half;
                uint16_t *output_walk = output + n_iter * k;
                futures.push_back(pool->Submit([](uint16_t *dst, uint16_t *src, int round, int len){
                    for (int i = 0; i < round; i++) {
                        FastllmTfaccAccumulateFloat16(dst + i * len, src, dst + i * len, len);
                    }
                }, output_walk, bias_walk, cur_n, k));
                n_iter += cur_n;
            }
            for (auto &one_future : futures) {
                one_future.get();
            }
            futures.clear();
        }
    }

    // clear temp data
    for (auto in : temp_input) {
        delete in;
    }
    temp_input.clear();

    for (auto out : temp_output) {
        delete out;
    }
    temp_output.clear();

    for (int tfacc : all_tfacc) {
        FastllmTfaccBlasopCache[tfacc]->Clear();
    }
#else
    ErrorInFastLLM("Float16 Weight Float16 Data Linear not supported.\n");
#endif
}

void FastllmTfaccLinearUint8WFloat32D(float *input, float *output, uint8_t *weight, float *bias, int n, int m, int k,
                                      tfdl::PerChannelConfig tfWeightConfig, fastllm::ThreadPool *pool) {
    auto t0 = chrono::system_clock::now();

    // collect all available tfacc cores
    std::vector<int> all_tfacc = ConfigureTFACC(fastllm::GetThreads(), TF_TFNN_GetChipNum());

    int per_n = 8 * all_tfacc.size();
    int n_round = (n - 1) / per_n + 1;
    if (n_round > 1) {
        for (int i = 0; i < n_round; i++) {
            int cur_n = min(per_n, n - (i * per_n));
            float *cur_input = input + i * per_n * m;
            float *cur_output = output + i * per_n * k;
            FastllmTfaccLinearUint8WFloat32D(cur_input, cur_output, weight, bias, cur_n, m, k, tfWeightConfig, pool);
        }
        return;
    }

    int per_k, per_m, k_round, m_round;
    ConfigureKMRound(k, m, all_tfacc.size(), &per_k, &per_m, &k_round, &m_round);

    // create space for blasop
    FastllmTfaccInitBlasop();

    // prepare weight
    long long int weight_key = (long long int) weight;
    if (FastllmTfaccWeightMap[weight_key].empty()) {
        printf("[%d, %d] x [%d, %d] -> [%d, %d]\n", n, m, k, m, n, k);
        printf("prepare weight, n_round: %d, m_round: %d, k_round: %d per_m: %d, per_k: %d\n", n_round, m_round, k_round, per_m, per_k);

        vector<future<void>> futures;
        for (int k_iter = 0; k_iter < k_round; k_iter++) {
            int cur_k = min(per_k, k - (k_iter * per_k));
            for (int m_iter = 0; m_iter < m_round; m_iter++) {
                int cur_m = min(per_m, m - (m_iter * per_m));

                // create weight for tfacc
                auto real_space = new uint8_t[cur_k * cur_m];
                auto cur_weight = new tfdl::TFDataInt8(0.f, 0.f, {cur_k, cur_m}, real_space);
                cur_weight->SetPerChannelConfig(tfWeightConfig.extract(k_iter * per_k, k_iter * per_k + cur_k));

                FastllmTfaccWeightRealSpace[weight_key].push_back((void *) real_space);
                FastllmTfaccWeightMap[weight_key].push_back(cur_weight);

                // copy weight from cpu
                futures.push_back(pool->Submit([](uint8_t *dst, uint8_t *src, int len, int round, int dst_stride, int src_stride) {
                    FastllmTfaccCopyStride<uint8_t>(dst, src, len, round, dst_stride, src_stride);
                }, cur_weight->GetInt8RawData(), 
                   weight + k_iter * per_k * m + m_iter * per_m, 
                   cur_m, cur_k, cur_m, m));
            }
        }
        for (auto &one_future : futures) {
            one_future.get();
        }
    }

    // quantize input data
    vector<tfdl::TFDataInt8 *> temp_input = FastllmLinearPrepareInputData(input, n, m, per_m, m_round, pool);
    
    // prepare temp output buffer
    vector<tfdl::TFDataFloat *> temp_output = FastllmLinearPrepareOutputData<tfdl::TFDataFloat>(n, k, per_k, m_round, k_round);

    auto t1 = chrono::system_clock::now();

    // run with tfacc
    vector<future<void>> futures;
    int i = 0;
    for (int k_iter = 0; k_iter < k_round; k_iter++) {
        for (int m_iter = 0; m_iter < m_round; m_iter++) {
            tfdl::TFDataInt8 *cur_input = temp_input[m_iter];
            tfdl::TFDataFloat *cur_output = temp_output[i];
            tfdl::TFDataInt8 *cur_weight = (tfdl::TFDataInt8 *) FastllmTfaccWeightMap[weight_key][i];
            int cur_core = all_tfacc[i % all_tfacc.size()];
            auto cur_blasop = FastllmTfaccBlasopCache[cur_core];
            futures.push_back(pool->Submit([](tfdl::TFDataInt8 *input, tfdl::TFDataFloat *output, 
                                              tfdl::TFDataInt8 *weight, tfdl::TFDataFloat *bias, int device, void *blasop) {
                tfacc40t::InnerProduct(input, output, weight, bias, device, blasop);
            }, cur_input, cur_output, cur_weight, nullptr, cur_core, cur_blasop));

            i++;
            if (i % all_tfacc.size() == 0) {
                for (auto &one_future : futures) {
                    one_future.get();
                }
                futures.clear();
            }
        }
    }
    for (auto &one_future : futures) {
        one_future.get();
    }
    futures.clear();

    auto t2 = chrono::system_clock::now();

    // post process temp output
    // 1.accumulate m direction
    if (m_round > 1) {
        for (int k_iter = 0; k_iter < k_round; k_iter++) {
            int cur_k = min(per_k, k - (k_iter * per_k));
            float *dst = temp_output[k_iter * m_round]->GetFloatRawData();
            vector<float *> src;
            for (int m_iter = 1; m_iter < m_round; m_iter++) {
                src.push_back(temp_output[k_iter * m_round + m_iter]->GetFloatRawData());
            }
            int per_n = 64;
            for (int n_iter = 0; n_iter < n; n_iter += per_n) {
                int cur_n = min(per_n, n - n_iter);
                futures.push_back(pool->Submit([](float *dst, vector<float *> src, int len, int shift){
                    for (int i = 0; i < src.size(); i++) {
                        FastllmTfaccAccumulateFloat32(src[i] + shift, dst + shift, dst + shift, len);
                    }
                }, dst, src, cur_n * cur_k, n_iter * cur_k));
            }
        }
        for (auto &one_future : futures) {
            one_future.get();
        }
        futures.clear();
    }

    // 2.copy k direction
    for (int k_iter = 0; k_iter < k_round; k_iter++) {
        int cur_k = min(per_k, k - (k_iter * per_k));
        float *dst = output + k_iter * per_k;
        float *src = temp_output[k_iter * m_round]->GetFloatRawData();
        futures.push_back(pool->Submit(FastllmTfaccCopyStride<float>, dst, src, cur_k, n, k, cur_k));
    }
    for (auto &one_future : futures) {
        one_future.get();
    }
    futures.clear();

    // 3.add bias
    if (bias) {
        if (n == 1) {
            FastllmTfaccAccumulateFloat32(bias, output, output, k);
        } else {
            int per = max((int) (n / all_tfacc.size()), 1);
            for (int n_iter = 0; n_iter < n;) {
                int cur_n = min(per, n - n_iter);

                float *bias_walk = bias;
                float *output_walk = output + n_iter * k;
                futures.push_back(pool->Submit([](float *dst, float *src, int round, int len) {
                    for (int i = 0; i < round; i++) {
                        FastllmTfaccAccumulateFloat32(dst + i * len, src, dst + i * len, len);
                    }
                }, output_walk, bias_walk, cur_n, k));
                n_iter += cur_n;
            }
            for (auto &one_future : futures) {
                one_future.get();
            }
            futures.clear();
        }
    }

    // clear temp data
    for (auto in : temp_input) {
        delete in;
    }
    temp_input.clear();

    for (auto out : temp_output) {
        delete out;
    }
    temp_output.clear();

    for (int tfacc : all_tfacc) {
        FastllmTfaccBlasopCache[tfacc]->Clear();
    }

    auto t3 = chrono::system_clock::now();

    // printf("[%d, %d] * [%d, %d] -> [%d, %d]\n", n, m, k, m, n, k);
    // printf("multicore linear: Cores: %d, Gops: %.2f, time: prepare: %.2fms; run blasop: %.2fms, postprocess: %.2fms\n", 
    //        (int) min(m_round * k_round, (int) all_tfacc.size()), ((float) m * k * n / 1000 / 1000 / 1000) / GetSpan(t0, t3),
    //        GetSpan(t0, t1) * 1000, GetSpan(t1, t2) * 1000, GetSpan(t2, t3) * 1000);
}

void FastllmTfaccLinearUint8WFloat16D(uint16_t *input, uint16_t *output, uint8_t *weight, float *bias, int n, int m, int k,
                                      tfdl::PerChannelConfig tfWeightConfig, fastllm::ThreadPool *pool) {
#ifdef __ARM_FEATURE_FP16_VECTOR_ARITHMETIC
    // collect all available tfacc cores
    std::vector<int> all_tfacc = ConfigureTFACC(fastllm::GetThreads(), TF_TFNN_GetChipNum());

    int per_n = 8 * all_tfacc.size();
    int n_round = (n - 1) / per_n + 1;
    if (n_round > 1) {
        for (int i = 0; i < n_round; i++) {
            int cur_n = min(per_n, n - (i * per_n));
            uint16_t *cur_input = input + i * per_n * m;
            uint16_t *cur_output = output + i * per_n * k;
            FastllmTfaccLinearUint8WFloat16D(cur_input, cur_output, weight, bias, cur_n, m, k, tfWeightConfig, pool);
        }
        return;
    }

    int per_k, per_m, k_round, m_round;
    ConfigureKMRound(k, m, all_tfacc.size(), &per_k, &per_m, &k_round, &m_round);

    // create space for blasop
    FastllmTfaccInitBlasop();

    // prepare weight
    long long int weight_key = (long long int) weight;
    if (FastllmTfaccWeightMap[weight_key].empty()) {
        printf("[%d, %d] x [%d, %d] -> [%d, %d]\n", n, m, k, m, n, k);
        printf("prepare weight, n_round: %d, m_round: %d, k_round: %d per_m: %d, per_k: %d\n", n_round, m_round, k_round, per_m, per_k);

        vector<future<void>> futures;
        for (int k_iter = 0; k_iter < k_round; k_iter++) {
            int cur_k = min(per_k, k - (k_iter * per_k));
            for (int m_iter = 0; m_iter < m_round; m_iter++) {
                int cur_m = min(per_m, m - (m_iter * per_m));

                // create weight for tfacc
                auto real_space = new uint8_t[cur_k * cur_m];
                auto cur_weight = new tfdl::TFDataInt8(0.f, 0.f, {cur_k, cur_m}, real_space);
                cur_weight->SetPerChannelConfig(tfWeightConfig.extract(k_iter * per_k, k_iter * per_k + cur_k));

                FastllmTfaccWeightRealSpace[weight_key].push_back((void *) real_space);
                FastllmTfaccWeightMap[weight_key].push_back(cur_weight);

                // copy weight from cpu
                futures.push_back(pool->Submit([](uint8_t *dst, uint8_t *src, int len, int round, int dst_stride, int src_stride) {
                    FastllmTfaccCopyStride<uint8_t>(dst, src, len, round, dst_stride, src_stride);
                }, cur_weight->GetInt8RawData(), 
                   weight + k_iter * per_k * m + m_iter * per_m, 
                   cur_m, cur_k, cur_m, m));
            }
        }
        for (auto &one_future : futures) {
            one_future.get();
        }
    }

    // quantize input data
    vector<tfdl::TFDataInt8 *> temp_input = FastllmLinearPrepareInputData(input, n, m, per_m, m_round, pool);

    // prepare temp output buffer
    vector<tfdl::TFDataFloat16 *> temp_output = FastllmLinearPrepareOutputData<tfdl::TFDataFloat16>(n, k, per_k, m_round, k_round);

    // run with tfacc
    vector<future<void>> futures;
    int i = 0;
    for (int k_iter = 0; k_iter < k_round; k_iter++) {
        for (int m_iter = 0; m_iter < m_round; m_iter++) {
            tfdl::TFDataInt8 *cur_input = temp_input[m_iter];
            tfdl::TFDataFloat16 *cur_output = temp_output[i];
            tfdl::TFDataInt8 *cur_weight = (tfdl::TFDataInt8 *) FastllmTfaccWeightMap[weight_key][i];
            int cur_core = all_tfacc[i % all_tfacc.size()];
            auto cur_blasop = FastllmTfaccBlasopCache[cur_core];
            futures.push_back(pool->Submit([](tfdl::TFDataInt8 *input, tfdl::TFDataFloat16 *output, 
                                              tfdl::TFDataInt8 *weight, tfdl::TFDataFloat *bias, int device, void *blasop) {
                tfacc40t::InnerProduct(input, output, weight, bias, device, blasop);
            }, cur_input, cur_output, cur_weight, nullptr, cur_core, cur_blasop));

            i++;
            if (i % all_tfacc.size() == 0) {
                for (auto &one_future : futures) {
                    one_future.get();
                }
                futures.clear();
            }
        }
    }
    for (auto &one_future : futures) {
        one_future.get();
    }
    futures.clear();

    // post process temp output
    // 1. accumulate m direction
    if (m_round > 1) {
        for (int k_iter = 0; k_iter < k_round; k_iter++) {
            int cur_k = min(per_k, k - k_iter * per_k);
            uint16_t *dst = temp_output[k_iter * m_round]->GetFloat16RawData();
            vector<uint16_t *> src;
            for (int m_iter = 1; m_iter < m_round; m_iter++) {
                src.push_back(temp_output[k_iter * m_round + m_iter]->GetFloat16RawData());
            }
            int per_n = 64;
            for (int n_iter = 0; n_iter < n; n_iter += per_n) {
                int cur_n = min(per_n, n - n_iter);
                futures.push_back(pool->Submit([](uint16_t *dst, vector<uint16_t *> src, int len, int shift){
                    for (int i = 0; i < src.size(); i++) {
                        FastllmTfaccAccumulateFloat16(src[i] + shift, dst + shift, dst + shift, len);
                    }
                }, dst, src, cur_n * cur_k, n_iter * cur_k));
            }
        }
        for (auto &one_future : futures) {
            one_future.get();
        }
        futures.clear();
    }

    // 2. copy k direction
    for (int k_iter = 0; k_iter < k_round; k_iter++) {
        int cur_k = min(per_k, k - k_iter * per_k);
        uint16_t *dst = output + k_iter * per_k;
        uint16_t *src = temp_output[k_iter * m_round]->GetFloat16RawData();
        futures.push_back(pool->Submit(FastllmTfaccCopyStride<uint16_t>, dst, src, cur_k, n, k, cur_k));
    }
    for (auto &one_future : futures) {
        one_future.get();
    }
    futures.clear();
    
    // add bias
    if (bias) {
        long long int bias_key = (long long int) bias;
        if (FastllmTfaccWeightMap.find(bias_key) == FastllmTfaccWeightMap.end()) {
            uint16_t *bias_half = new uint16_t[k];
            FastllmTfaccWeightMap[bias_key].push_back(new tfdl::TFDataFloat16({k}, bias_half));
            for (int i = 0; i < k; i++) {
                bias_half[i] = float_to_half(bias[i]);
            }
        }

        uint16_t *bias_half = ((tfdl::TFDataFloat16 *) FastllmTfaccWeightMap[bias_key][0])->GetFloat16RawData();

        if (n == 1) {
            FastllmTfaccAccumulateFloat16(bias_half, output, output, k);
        } else {
            int per = max((int) (n / all_tfacc.size()), 1);
            for (int n_iter = 0; n_iter < n;) {
                int cur_n = min(per, n - n_iter);

                uint16_t *bias_walk = bias_half;
                uint16_t *output_walk = output + n_iter * k;
                futures.push_back(pool->Submit([](uint16_t *dst, uint16_t *src, int round, int len){
                    for (int i = 0; i < round; i++) {
                        FastllmTfaccAccumulateFloat16(dst + i * len, src, dst + i * len, len);
                    }
                }, output_walk, bias_walk, cur_n, k));
                n_iter += cur_n;
            }
            for (auto &one_future : futures) {
                one_future.get();
            }
            futures.clear();
        }
    }

    // clear temp data
    for (auto in : temp_input) {
        delete in;
    }
    temp_input.clear();

    for (auto out : temp_output) {
        delete out;
    }
    temp_output.clear();

    for (int tfacc : all_tfacc) {
        FastllmTfaccBlasopCache[tfacc]->Clear();
    }
#else
    ErrorInFastLLM("Uint8 Weight Float16 Data Linear not supported.\n");
#endif
}