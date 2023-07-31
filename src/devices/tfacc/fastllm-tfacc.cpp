//
// Created by siemon on 7/24/23.
//

#include "devices/tfacc/fastllm-tfacc.h"

#include <Data.h>
#include <Operations.h>

#include <numa.h>
#include <future>

vector<int> ConfigureTFACC(int coreNum, int chipNum) {
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

void FastllmTfaccAccumulate(float *x1, float *x2, float *y, size_t len) {
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

void FastllmTfaccLinearMultiCore(float *input, float *output, uint8_t *weight, float *bias, int n, int m, int k,
                                 const tfdl::PerChannelConfig &tfWeightConfig, int threadNum, fastllm::ThreadPool *pool) {
    int chipNum = TF_TFNN_GetChipNum();
    int per = threadNum * 8;
    if (n > per) {
        for (int i = 0; i < n;) {
            int cur = min(per, n - i);
            float *cur_input = input + i * m;
            float *cur_output = output + i * k;
            FastllmTfaccLinearMultiCore(cur_input, cur_output, weight, bias, cur, m, k, tfWeightConfig, threadNum, pool);
            i += cur;
        }
        return;
    }

    auto input_tf = new tfdl::TFDataFloat({n, m}, input);
    auto output_tf = new tfdl::TFDataFloat({n, k}, output);
    auto weight_tf = new tfdl::TFDataInt8(0.f, 0.f, {k, m}, weight);
    auto bias_tf = new tfdl::TFDataFloat({k}, bias);
    auto weight_key = (long long int) weight;
    weight_tf->SetPerChannelConfig(tfWeightConfig);

    if (m < 256 && k < 256) {
        tfacc40t::InnerProduct(input_tf, output_tf, weight_tf, bias_tf, 0);
    }
    
    if (m <= 4096 && k / threadNum < 256) {
        threadNum = min(threadNum, k / 256); // 切out features时，尽量不让spatial < 256
    }
    vector<int> tfaccs = ConfigureTFACC(threadNum, chipNum);

    // create space for blasop
    if (FastllmTfaccBlasopCache.empty()) {
        int maxCmdNum = 60000;
        int total_cores = 8 * TF_TFNN_GetChipNum();
        for (int i = 0; i < total_cores; i++) {
            FastllmTfaccBlasopCache.push_back(new tfacc40t::BlasopList(TF_TFNN_GetChipId(i), maxCmdNum));
        }

        // printf("tfaccs: ");
        // for (auto tfacc : tfaccs) {
        //     printf("%d ", tfacc);
        // }
        // printf("\n");
    }

    // cut input features
    if (m > 4096) {
        auto t0 = chrono::system_clock::now();
        int per_m = m / threadNum;
        int last_m = m - per_m * (threadNum - 1);

        // get original input & output dims
        vector<int> ori_in_dim = input_tf->GetDims();
        ori_in_dim.pop_back();
        ori_in_dim.push_back(per_m);
        vector<int> last_in_dim = input_tf->GetDims();
        last_in_dim.pop_back();
        last_in_dim.push_back(last_m);
        vector<int> ori_out_dim = input_tf->GetDims();
        ori_out_dim.pop_back();
        ori_out_dim.push_back(k);

        // create fake weight
        if (FastllmTfaccWeightMap.find(weight_key) == FastllmTfaccWeightMap.end()) {
            printf("prepare weight, cut input features\n");
            FastllmTfaccWeightMap[weight_key] = vector<tfdl::TFDataInt8 *>(threadNum);
            vector<uint8_t *> fake_weight(threadNum);
            for (int i = 0; i < threadNum - 1; i++) {
                FastllmTfaccWeightMap[weight_key][i] = new tfdl::TFDataInt8(0.f, 0.f, {k, per_m});
                FastllmTfaccWeightMap[weight_key][i]->SetPerChannelConfig(weight_tf->GetPerChannelConfig());
                auto data = FastllmTfaccWeightMap[weight_key][i]->GetRawDataPtr();
                fake_weight[i] = (uint8_t *) data;
            }
            // handle last in features
            FastllmTfaccWeightMap[weight_key][threadNum - 1] = new tfdl::TFDataInt8(0.f, 0.f, {k, last_m});
            FastllmTfaccWeightMap[weight_key][threadNum - 1]->SetPerChannelConfig(weight_tf->GetPerChannelConfig());
            auto data = FastllmTfaccWeightMap[weight_key][threadNum - 1]->GetRawDataPtr();
            fake_weight[threadNum - 1] = (uint8_t *) data;

            vector<thread *> prepare_threads;
            for (int i = 0; i < threadNum - 1; i++) {
                prepare_threads.push_back(new thread([](uint8_t *dst, uint8_t *src, int i, int m, int k, int per_m) {
                    for (int o = 0; o < k; o++) {
                        memcpy(dst + o * per_m, src + o * m + i * per_m, per_m * sizeof(uint8_t));
                    }
                }, fake_weight[i], (uint8_t *) weight_tf->GetRawDataPtr(), i, m, k, per_m));
            }
            prepare_threads.push_back(new thread([](uint8_t *dst, uint8_t *src, int i, int m, int k, int per_m, int last_m){
                for (int o = 0; o < k; o++) {
                    memcpy(dst + o * last_m, src + o * m + i * per_m, last_m * sizeof(uint8_t));
                }
            }, fake_weight[threadNum - 1], (uint8_t *) weight_tf->GetRawDataPtr(), threadNum - 1, m, k, per_m, last_m));

            for (int i = 0; i < prepare_threads.size(); i++) {
                prepare_threads[i]->join();
                delete prepare_threads[i];
            }
            prepare_threads.clear();
        }

        vector<tfdl::TFDataInt8 *> temp_input;
        if (n == 1) {
            for (int i = 0; i < threadNum - 1; i++) {
                tfdl::TFDataFloat origin = tfdl::TFDataFloat(ori_in_dim, input_tf->GetFloatRawData() + i * per_m);
                temp_input.push_back(new tfdl::TFDataInt8(&origin));
            }
            tfdl::TFDataFloat origin = tfdl::TFDataFloat(last_in_dim, input_tf->GetFloatRawData() + (threadNum - 1) * per_m);
            temp_input.push_back(new tfdl::TFDataInt8(&origin));
        } else {
            float min = input_tf->GetMinValue(), max = input_tf->GetMaxValue();
            auto quantization = tfdl::QuantizationConfig(min, max);
            for (int i = 0; i < threadNum - 1; i++) {
                temp_input.push_back(new tfdl::TFDataInt8(min, max, ori_in_dim));
            }
            temp_input.push_back(new tfdl::TFDataInt8(min, max, last_in_dim));

            // quantize input
            vector<future<void>> futures;
            for (int i = 0; i < threadNum - 1; i++) {
                futures.push_back(pool->Submit([](float *src, uint8_t *dst, int n, int m, int per_m, int i, tfdl::QuantizationConfig quantization){          
                    for (int o = 0; o < n; o++) {
                        quantization.quantall(per_m, src + o * m + i * per_m, dst + o * per_m);
                    }
                }, input_tf->GetFloatRawData(), temp_input[i]->GetInt8RawData(), n, m, per_m, i, quantization));
            }
            temp_input.back() = new tfdl::TFDataInt8(min, max, last_in_dim);
            for (int o = 0; o < n; o++) {
                quantization.quantall(last_m, input_tf->GetFloatRawData() + o * m + (threadNum - 1) * per_m, temp_input.back()->GetInt8RawData() + o * last_m);
            }
            for (auto &one_future : futures) {
                one_future.get();
            }
        }

        // create temp output
        vector<tfdl::TFDataFloat *> temp_output;
        for (int i = 0; i < threadNum; i++) {
            temp_output.push_back(new tfdl::TFDataFloat(ori_out_dim));
        }

        auto t1 = chrono::system_clock::now();

        // use multi core do calculate linear
        vector<future<void>> futures;
        for (int i = 0; i < threadNum - 1; i++) {
            futures.push_back(pool->Submit([](tfdl::TFDataInt8 *input, tfdl::TFDataFloat *output, 
                                              tfdl::TFDataInt8 *weight, tfdl::TFDataFloat *bias, int device, void *blasop){
                tfacc40t::InnerProduct(input, output, weight, bias, device, blasop);
                }, temp_input[i], temp_output[i], FastllmTfaccWeightMap[weight_key][i], 
                   nullptr, tfaccs[i], (void *) FastllmTfaccBlasopCache[tfaccs[i]]));
        }
        tfacc40t::InnerProduct(temp_input.back(), temp_output.back(), FastllmTfaccWeightMap[weight_key].back(),
                               nullptr, tfaccs.back(), (void *) FastllmTfaccBlasopCache[tfaccs.back()]);
        for (auto &one_future : futures) {
            one_future.get();
        }

        auto t2 = chrono::system_clock::now();

        // add all temp output together and add bias
        output_tf->Create(ori_out_dim);
        if (bias) {
            for (int o = 0; o < n; o++) {
                memcpy(output_tf->GetFloatRawData() + o * k, bias, k * sizeof(float));
            }
        } else {
            memset(output_tf->GetFloatRawData(), 0, output_tf->Count(0) * sizeof(float));
        }
        if (n >= threadNum) {
            int per_n = n / threadNum;
            int last_n = n - (threadNum - 1) * per_n;

            vector<future<void>> futures;
            for (int i = 0; i < threadNum - 1; i++) {
                float *output_walk = output_tf->GetFloatRawData() + i * per_n * k;
                futures.push_back(pool->Submit([](vector<tfdl::TFDataFloat *> temp_output, float *output_walk, int i, int per_n, int k){
                    for (int j = 0; j < temp_output.size(); j++) {
                        float *temp_walk = temp_output[j]->GetFloatRawData() + i * per_n * k;
                        FastllmTfaccAccumulate(output_walk, temp_walk, output_walk, per_n * k);
                    }
                }, temp_output, output_walk, i, per_n, k));
            }
            for (int j = 0; j < temp_output.size(); j++) {
                float *output_walk = output_tf->GetFloatRawData() + (threadNum - 1) * per_n * k;
                float *temp_walk = temp_output[j]->GetFloatRawData() + (threadNum - 1) * per_n * k;
                FastllmTfaccAccumulate(output_walk, temp_walk, output_walk, last_n * k);
            }
            for (auto &one_future : futures) {
                one_future.get();
            }
        } else {
            for (int i = 0; i < temp_output.size(); i++) {
                float *output_walk = output_tf->GetFloatRawData();
                float *temp_walk = temp_output[i]->GetFloatRawData();
                FastllmTfaccAccumulate(output_walk, temp_walk, output_walk, output_tf->Count(0));
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

        for (int tfacc : tfaccs) {
            FastllmTfaccBlasopCache[tfacc]->Clear();
        }

        auto t3 = chrono::system_clock::now();

        // if (!fake_weight.empty()) {
        //     for (float *data : fake_weight) {
        //         delete[] data;
        //     }
        // }
        // fake_weight.clear();
        // printf("%s * %s -> %s, Cores: %d, Gops: %.2f\n", input_tf->GetShapeString().c_str(), 
        //        weight_tf->GetShapeString().c_str(), output_tf->GetShapeString().c_str(), (int) tfaccs.size(), 
        //        ((float) m * k * n / 1000 / 1000 / 1000) / GetSpan(t0, t3));
        // printf("multicore linear time: prepare: %.2fms; run blasop: %.2fms, postprocess: %.2fms\n", 
        //        GetSpan(t0, t1) * 1000, GetSpan(t1, t2) * 1000, GetSpan(t2, t3) * 1000);
    }
    // cut output features
    else {
        auto t0 = chrono::system_clock::now();
        int per_k = k / threadNum;
        int last_k = k - (threadNum - 1) * per_k;

        vector<int> ori_in_dim = input_tf->GetDims();
        vector<int> ori_out_dim = input_tf->GetDims();
        ori_out_dim.pop_back();
        ori_out_dim.push_back(per_k);
        vector<int> last_out_dim = input_tf->GetDims();
        last_out_dim.pop_back();
        last_out_dim.push_back(last_k);

        if (FastllmTfaccWeightMap.find(weight_key) == FastllmTfaccWeightMap.end()) {
            printf("prepare weight, cut output features\n");
            FastllmTfaccWeightMap[weight_key] = vector<tfdl::TFDataInt8 *>(threadNum);
            for (int i = 0; i < threadNum - 1; i++) {
                uint8_t *cur = ((uint8_t *) weight_tf->GetRawDataPtr()) + i * per_k * m;
                FastllmTfaccWeightMap[weight_key][i] = new tfdl::TFDataInt8(0, 0, {per_k, m}, cur);

                int st = i * per_k;
                int end = st + per_k;
                FastllmTfaccWeightMap[weight_key][i]->SetPerChannelConfig(weight_tf->GetPerChannelConfig(st, end));
            }
            uint8_t *cur = ((uint8_t *) weight_tf->GetRawDataPtr()) + (threadNum - 1) * per_k * m;
            FastllmTfaccWeightMap[weight_key][threadNum - 1] = new tfdl::TFDataInt8(0, 0, {last_k, m}, cur);

            int st = (threadNum - 1) * per_k;
            int end = st + last_k;
            FastllmTfaccWeightMap[weight_key][threadNum - 1]->SetPerChannelConfig(weight_tf->GetPerChannelConfig(st, end));
        }

        auto input_int8 = new tfdl::TFDataInt8(input_tf);
        vector<tfdl::TFDataFloat *> temp_output;
        for (int i = 0; i < threadNum - 1; i++) {
            temp_output.push_back(new tfdl::TFDataFloat(ori_out_dim));
        }
        temp_output.push_back(new tfdl::TFDataFloat(last_out_dim));

        auto t1 = chrono::system_clock::now();

        vector<future<void>> futures;
        for (int i = 0; i < threadNum - 1; i++) {
            futures.push_back(pool->Submit([](tfdl::TFDataInt8 *input, tfdl::TFDataFloat *output, 
                                              tfdl::TFDataInt8 *weight, tfdl::TFDataFloat *bias, int device, void *blasop){
                tfacc40t::InnerProduct(input, output, weight, bias, device, blasop);
            }, input_int8, temp_output[i], FastllmTfaccWeightMap[weight_key][i], 
               nullptr, tfaccs[i], (void *) FastllmTfaccBlasopCache[tfaccs[i]]));
        }
        tfacc40t::InnerProduct(input_int8, temp_output.back(), FastllmTfaccWeightMap[weight_key].back(),
                               nullptr, tfaccs.back(), (void *) FastllmTfaccBlasopCache[tfaccs.back()]);
        for (auto &one_future : futures) {
            one_future.get();
        }

        auto t2 = chrono::system_clock::now();

        ori_out_dim.pop_back();
        ori_out_dim.push_back(k);
        output_tf->Create(ori_out_dim);
        for (int i = 0; i < threadNum - 1; i++) {
            for (int o = 0; o < n; o++) {
                memcpy(output_tf->GetFloatRawData() + o * k + i * per_k,
                        temp_output[i]->GetFloatRawData() + o * per_k,
                        per_k * sizeof(float));
            }
        }
        for (int o = 0; o < n; o++) {
            memcpy(output_tf->GetFloatRawData() + o * k + (threadNum - 1) * per_k,
                    temp_output[threadNum - 1]->GetFloatRawData() + o * last_k,
                    last_k * sizeof(float));
        }
        if (bias) {
            for (int o = 0; o < n; o++) {
                float *bias_walk = bias;
                float *output_walk = output_tf->GetFloatRawData() + o * k;
                FastllmTfaccAccumulate(output_walk, bias_walk, output_walk, k);
            }
        }

        delete input_int8;
        for (auto out : temp_output) {
            delete out;
        }
        temp_output.clear();

        for (int tfacc :tfaccs) {
            FastllmTfaccBlasopCache[tfacc]->Clear();
        }

        auto t3 = chrono::system_clock::now();
        // printf("%s * %s -> %s, Cores: %d, Gops: %.2f\n", input_tf->GetShapeString().c_str(), 
        //        weight_tf->GetShapeString().c_str(), output_tf->GetShapeString().c_str(), (int) tfaccs.size(), 
        //        ((float) m * k * n / 1000 / 1000 / 1000) / GetSpan(t0, t3));
        // printf("multicore linear time: prepare: %.2fms; run blasop: %.2fms, postprocess: %.2fms\n", 
        //        GetSpan(t0, t1) * 1000, GetSpan(t1, t2) * 1000, GetSpan(t2, t3) * 1000);
    }

    delete input_tf;
    delete output_tf;
    delete weight_tf;
    delete bias_tf;
}

void FastllmTfaccLinearMultiCoreAuto(float *input, float *output, uint8_t *weight, float *bias, int n, int m, int k,
                                     tfdl::PerChannelConfig tfWeightConfig, fastllm::ThreadPool *pool) {
    auto t0 = chrono::system_clock::now();

    // collect all available tfacc cores
    std::vector<int> all_tfacc = ConfigureTFACC(fastllm::GetThreads(), TF_TFNN_GetChipNum());

    int per_n = 128;
    int n_round = (n - 1) / per_n + 1;
    if (n_round > 1) {
        for (int i = 0; i < n_round; i++) {
            int cur_n = min(per_n, n - (i * per_n));
            float *cur_input = input + i * per_n * m;
            float *cur_output = output + i * per_n * k;
            FastllmTfaccLinearMultiCoreAuto(cur_input, cur_output, weight, bias, cur_n, m, k, tfWeightConfig, pool);
        }
        return;
    }

    int per_m = 4096;
    int per_k = 4096;
    int m_round = (m - 1) / per_m + 1;
    int k_round = (k - 1) / per_k + 1;
    while (m_round * k_round <= all_tfacc.size() / 4 && per_m / 2 >= 1024) {
        per_m /= 2;
        per_k /= 2;
        m_round = (m - 1) / per_m + 1;
        k_round = (k - 1) / per_k + 1;
    }
    while (m_round * k_round <= all_tfacc.size() / 2 && per_m / 2 >= 1024) {
        per_m /= 2;
        m_round = (m - 1) / per_m + 1;
    }
    while (m_round * k_round >= all_tfacc.size() * 4 && per_k * 2 <= 65536) {
        per_m *= 2;
        per_k *= 2;
        m_round = (m - 1) / per_m + 1;
        k_round = (k - 1) / per_k + 1;
    }
    while (m_round * k_round >= all_tfacc.size() * 2 && per_k * 2 <= 65536) {
        per_k *= 2;
        k_round = (k - 1) / per_k + 1;
    }

    // create space for blasop
    if (FastllmTfaccBlasopCache.empty()) {
        int maxCmdNum = 60000;
        int total_cores = 8 * TF_TFNN_GetChipNum();
        for (int i = 0; i < total_cores; i++) {
            FastllmTfaccBlasopCache.push_back(new tfacc40t::BlasopList(TF_TFNN_GetChipId(i), maxCmdNum));
        }

        printf("tfaccs: ");
        for (auto tfacc : all_tfacc) {
            printf("%d ", tfacc);
        }
        printf("\n");
    }

    // prepare weight
    long long int weight_key = (long long int) weight;
    if (FastllmTfaccWeightMap[weight_key].empty()) {
        printf("[%d, %d] x [%d, %d] -> [%d, %d]\n", n, m, k, m, n, k);
        printf("prepare weight, n_round: %d, m_round: %d, k_round: %d\n", n_round, m_round, k_round);
        FastllmTfaccWeightMap[weight_key] = vector<tfdl::TFDataInt8 *>(k_round * m_round);

        vector<future<void>> futures;
        for (int k_iter = 0; k_iter < k_round; k_iter++) {
            int cur_k = min(per_k, k - (k_iter * per_k));
            for (int m_iter = 0; m_iter < m_round; m_iter++) {
                int cur_m = min(per_m, m - (m_iter * per_m));

                // create weight for tfacc
                auto cur_weight = new tfdl::TFDataInt8(0.f, 0.f, {cur_k, cur_m});
                cur_weight->SetPerChannelConfig(tfWeightConfig.extract(k_iter * per_k, k_iter * per_k + cur_k));
                FastllmTfaccWeightMap[weight_key][k_iter * m_round + m_iter] = cur_weight;

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
    vector<tfdl::TFDataInt8 *> temp_input;
    if (n == 1) {
        for (int i = 0; i < m_round; i++) {
            int cur_m = min(per_m, m - (i * per_m));
            tfdl::TFDataFloat origin = tfdl::TFDataFloat({n, cur_m}, input + i * per_m);
            temp_input.push_back(new tfdl::TFDataInt8(&origin));
        }
    } else {
        auto input_tf = tfdl::TFDataFloat({n, m}, input);
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
            futures.push_back(pool->Submit([](uint8_t *dst, float *src, int len, int round, int dst_stride, int src_stride,tfdl::PerChannelConfig config){          
                FastllmTfaccQuantization(dst, src, len, round, dst_stride, src_stride, config);
            }, temp_input[i]->GetInt8RawData(), input + i * per_m, cur_m, n, cur_m, m, perChannelConfig));
        }
        for (auto &one_future : futures) {
            one_future.get();
        }
    }
    
    // prepare temp output buffer
    vector<tfdl::TFDataFloat *> temp_output;
    for (int k_iter = 0; k_iter < k_round; k_iter++) {
        int cur_k = min(per_k, k - (k_iter * per_k));
        for (int m_iter = 0; m_iter < m_round; m_iter++) {
            temp_output.push_back(new tfdl::TFDataFloat({n, cur_k}));
        }
    }

    auto t1 = chrono::system_clock::now();

    // run with tfacc
    vector<future<void>> futures;
    int i = 0;
    for (int k_iter = 0; k_iter < k_round; k_iter++) {
        for (int m_iter = 0; m_iter < m_round; m_iter++) {
            tfdl::TFDataInt8 *cur_input = temp_input[m_iter];
            tfdl::TFDataFloat *cur_output = temp_output[i];
            tfdl::TFDataInt8 *cur_weight = FastllmTfaccWeightMap[weight_key][i];
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
                        FastllmTfaccAccumulate(src[i] + shift, dst + shift, dst + shift, len);
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
            FastllmTfaccAccumulate(bias, output, output, k);
        } else {
            int per = max((int) (n / all_tfacc.size()), 1);
            for (int n_iter = 0; n_iter < n;) {
                int cur_n = min(per, n - n_iter);

                float *bias_walk = bias;
                float *output_walk = output + n_iter * k;
                futures.push_back(pool->Submit([](float *dst, float *src, int round, int len) {
                    for (int i = 0; i < round; i++) {
                        FastllmTfaccAccumulate(dst + i * len, src, dst + i * len, len);
                    }
                }, output_walk, bias_walk, cur_n, k));
                n_iter += cur_n;
            }
            for (auto &one_future : futures) {
                one_future.get();
            }
            futures.clear();
        }
        // for (int o = 0; o < n; o++) {
        //     float *bias_walk = bias;
        //     float *output_walk = output + o * k;
        //     FastllmTfaccAccumulate(output_walk, bias_walk, output_walk, k);
        // }
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

void FastllmTfaccClearMemory() {
    for (auto blasopList : FastllmTfaccBlasopCache) {
        delete blasopList;
    }
    FastllmTfaccBlasopCache.clear();

    for (auto weightMapPair : FastllmTfaccWeightMap) {
        for (auto weight : weightMapPair.second) {
            delete weight;
        }
    }
    FastllmTfaccWeightMap.clear();
}