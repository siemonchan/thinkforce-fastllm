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
    for (int i = 0; i < chipNum - 1; i++) {
        for (int j = 0; j < each; j++) {
            tfaccs.push_back(chips[i] * 8 + j);
        }
    }
    for (int j = 0; j < last; j++) {
        tfaccs.push_back((chips.back()) * 8 + j);
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

vector<tfacc40t::BlasopList *> FastllmTfaccBlasopCache;
map<long long int, vector<tfdl::TFDataInt8 *>> FastllmTfaccWeightMap;
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

    vector<int> tfaccs = ConfigureTFACC(threadNum, chipNum);

    // create space for blasop
    if (FastllmTfaccBlasopCache.empty()) {
        int maxCmdNum = 60000;
        int total_cores = 8 * TF_TFNN_GetChipNum();
        for (int i = 0; i < total_cores; i++) {
            FastllmTfaccBlasopCache.push_back(new tfacc40t::BlasopList(TF_TFNN_GetChipId(i), maxCmdNum));
        }

        printf("tfaccs: ");
        for (auto tfacc : tfaccs) {
            printf("%d ", tfacc);
        }
        printf("\n");
    }

    // cut m
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

        vector<tfdl::TFDataFloat *> temp_input;
        int outer = input_tf->Count(0) / m;
        if (outer == 1) {
            for (int i = 0; i < threadNum - 1; i++) {
                temp_input.push_back(new tfdl::TFDataFloat(ori_in_dim, input_tf->GetFloatRawData() + i * per_m));
            }
            temp_input.push_back(new tfdl::TFDataFloat(last_in_dim, input_tf->GetFloatRawData() + (threadNum - 1) * per_m));
        } else {
            for (int i = 0; i < threadNum - 1; i++) {
                temp_input.push_back(new tfdl::TFDataFloat(ori_in_dim));
                for (int o = 0; o < outer; o++) {
                    memcpy(temp_input.back()->GetFloatRawData() + o * per_m,
                           input_tf->GetFloatRawData() + o * m + i * per_m, per_m * sizeof(float));
                }
            }
            temp_input.push_back(new tfdl::TFDataFloat(last_in_dim));
            for (int o = 0; o < outer; o++) {
                memcpy(temp_input.back()->GetFloatRawData() + o * per_m,
                       input_tf->GetFloatRawData() + o * m + (threadNum - 1) * per_m, last_m * sizeof(float));
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
            futures.push_back(pool->Submit([](tfdl::TFDataFloat *input, tfdl::TFDataFloat *output, tfdl::TFDataInt8 *weight, tfdl::TFDataFloat *bias, int device, void *blasop){
                tfacc40t::InnerProduct(input, output, weight, bias, device, blasop);
                },temp_input[i], temp_output[i], FastllmTfaccWeightMap[weight_key][i], nullptr, tfaccs[i], (void *) FastllmTfaccBlasopCache[tfaccs[i]]));
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
            for (int o = 0; o < outer; o++) {
                memcpy(output_tf->GetFloatRawData() + o * k, bias, k * sizeof(float));
            }
        } else {
            memset(output_tf->GetFloatRawData(), 0, output_tf->Count(0) * sizeof(float));
        }
        for (int i = 0; i < threadNum; i++) {
            float *output_walk = output_tf->GetFloatRawData();
            float *temp_walk = temp_output[i]->GetFloatRawData();
            FastllmTfaccAccumulate(output_walk, temp_walk, output_walk, output_tf->Count(0));
        }

        // clear temp data
        if (outer > 1) {
            for (auto in : temp_input) {
                delete in;
            }
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
        // printf("%s * %s -> %s, Gops: %.2f\n", input_tf->GetShapeString().c_str(), weight_tf->GetShapeString().c_str(), output_tf->GetShapeString().c_str(), ((float) m * k * outer / 1000 / 1000 / 1000) / GetSpan(t0, t3));
        // printf("multicore linear time: prepare: %.2fms; run blasop: %.2fms, postprocess: %.2fms\n", GetSpan(t0, t1) * 1000, GetSpan(t1, t2) * 1000, GetSpan(t2, t3) * 1000);
    }
    // cut k
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
            futures.push_back(pool->Submit([](tfdl::TFDataInt8 *input, tfdl::TFDataFloat *output, tfdl::TFDataInt8 *weight, tfdl::TFDataFloat *bias, int device, void *blasop){
                tfacc40t::InnerProduct(input, output, weight, bias, device, blasop);
            }, input_int8, temp_output[i], FastllmTfaccWeightMap[weight_key][i], nullptr, tfaccs[i], (void *) FastllmTfaccBlasopCache[tfaccs[i]]));
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
        int outer = input_tf->Count(0) / m;
        for (int i = 0; i < threadNum - 1; i++) {
            for (int o = 0; o < outer; o++) {
                memcpy(output_tf->GetFloatRawData() + o * k + i * per_k,
                        temp_output[i]->GetFloatRawData() + o * per_k,
                        per_k * sizeof(float));
            }
        }
        for (int o = 0; o < outer; o++) {
            memcpy(output_tf->GetFloatRawData() + o * k + (threadNum - 1) * per_k,
                    temp_output[threadNum - 1]->GetFloatRawData() + o * last_k,
                    last_k * sizeof(float));
        }
        if (bias) {
            for (int o = 0; o < outer; o++) {
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
        // printf("%s * %s -> %s, Gops: %.2f\n", input_tf->GetShapeString().c_str(), weight_tf->GetShapeString().c_str(), output_tf->GetShapeString().c_str(), ((float) m * k * outer / 1000 / 1000 / 1000) / GetSpan(t0, t3));
        // printf("multicore linear time: prepare: %.2fms; run blasop: %.2fms, postprocess: %.2fms\n", GetSpan(t0, t1) * 1000, GetSpan(t1, t2) * 1000, GetSpan(t2, t3) * 1000);
    }

    delete input_tf;
    delete output_tf;
    delete weight_tf;
    delete bias_tf;
}