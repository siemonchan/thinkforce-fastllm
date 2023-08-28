//
// Created by siemon on 8/25/23.
//

#include "utils.h"

#include "decicoder.h"

#include <cmath>

#include <chrono>

#include <algorithm>

#include <sstream>

#include <unordered_map>

#include <cstring>

#ifdef USE_CUDA
#include "fastllm-cuda.cuh"
#endif

#ifdef USE_TFACC40T
#include "fastllm-tfacc.h"
#endif

namespace fastllm {
    extern double GetSpan(std::chrono::system_clock::time_point time1, std::chrono::system_clock::time_point time2);

    std::vector<float> GetCheck(Data d) {
        float *data = (float *) d.cpuData;
        return std::vector<float>(data, data + d.Count(0));
    }

    DeciCoderModel::DeciCoderModel() {
        this->model_type = "decicoder";
        this->pre_prompt = "";
        this->user_role = "";
        this->bot_role = "";

        embed_dim = 2048;
		num_attention_heads = 32;
        num_key_value_heads = 4;
		head_dim = embed_dim / num_attention_heads;
        num_key_value_groups = num_attention_heads / num_key_value_heads;
		block_cnt = 20;
        rotary_dim = 64;

        sin.resize(max_positions);
        cos.resize(max_positions);
        std::vector <float> invFreq;
        for (int i = 0; i < rotary_dim; i += 2) {
            invFreq.push_back(1.0 / pow(10000, (float)i / rotary_dim));
        }
        for (int i = 0; i < max_positions; i++) {
            sin[i].resize(rotary_dim);
            cos[i].resize(rotary_dim);
            for (int j = 0; j < invFreq.size(); j++) {
                sin[i][j] = ::sin((float)i * invFreq[j]);
                cos[i][j] = ::cos((float)i * invFreq[j]);
            }
        }
        std::vector <float> fsin, fcos;
        for (int i = 0; i < sin.size(); i++) {
            for (int j = 0; j < sin[0].size(); j++) {
                fsin.push_back(sin[i][j]);
                fcos.push_back(cos[i][j]);
            }
        }
        sinData.CopyFrom(Data(DataType::FLOAT32, {(int)this->sin.size(), (int)this->sin[0].size()}, fsin));
        cosData.CopyFrom(Data(DataType::FLOAT32, {(int)this->cos.size(), (int)this->cos[0].size()}, fcos));
        weight.embeddingNames.insert("model.embed_tokens.weight");
    }

    int DeciCoderModel::Forward(const Data &inputIds,
                                const Data &attentionMask,
                                const Data &positionIds,
                                std::vector <std::pair <Data, Data> > &pastKeyValues,
                                const GenerationConfig &generationConfig,
                                const LastTokensManager &lastTokens,
                                std::vector <float> *logits) {
        std::vector <std::vector <float>*> batchLogits;
        batchLogits.push_back(logits);
        return ForwardBatch(1, inputIds, attentionMask, positionIds, pastKeyValues, generationConfig, lastTokens, &batchLogits)[0];
    }

    std::vector <int> DeciCoderModel::ForwardBatch(int batch,
                                                   const Data &inputIds,
                                                   const Data &attentionMask,
                                                   const Data &positionIds,
                                                   std::vector <std::pair <Data, Data> > &pastKeyValues,
                                                   const GenerationConfig &generationConfig,
                                                   const LastTokensManager &lastTokens,
                                                   std::vector <std::vector <float>*> *retLogits) {
        Data hiddenStates;
        Data attenInput;
        Data q, k, v;
        Data attenWeights, attenOutput;
        Data attenLastOutput;
        Data w1, w2, w3;

        Embedding(inputIds, this->weight["model.embed_tokens.weight"], hiddenStates);
        int seqlen = hiddenStates.dims[1];
        for (int i = 0; i < block_cnt; i++) {
            ApplyDeviceMap(this->deviceMap, i + 1, block_cnt);
            RMSNorm(hiddenStates, this->weight["model.layers." + std::to_string(i) + ".input_layernorm.weight"],
                    1e-6, attenInput);
            std::string qWeightName = "model.layers." + std::to_string(i) + ".self_attn.q_proj.weight";
            std::string kWeightName = "model.layers." + std::to_string(i) + ".self_attn.k_proj.weight";
            std::string vWeightName = "model.layers." + std::to_string(i) + ".self_attn.v_proj.weight";
            std::string oWeightName = "model.layers." + std::to_string(i) + ".self_attn.o_proj.weight";

            // 1.1 Get q, k, v
            int bsz = attenInput.dims[0], seqlen = attenInput.dims[1];
            Linear(attenInput, weight[qWeightName], Data(), q);
            Linear(attenInput, weight[kWeightName], Data(), k);
            Linear(attenInput, weight[vWeightName], Data(), v);

            std::vector <int> qSize = {bsz, seqlen, num_attention_heads, -1};
            std::vector <int> kSize = {bsz, seqlen, num_key_value_heads, -1};
            std::vector <int> vSize = {bsz, seqlen, num_key_value_heads, -1};
            q.Reshape(qSize);
            k.Reshape(kSize);
            v.Reshape(vSize);

            fastllm::LlamaRotatePosition2D(q, positionIds, sinData, cosData, rotary_dim);
            fastllm::LlamaRotatePosition2D(k, positionIds, sinData, cosData, rotary_dim);

            PermuteSelf(q, {0, 2, 1, 3});
            PermuteSelf(k, {0, 2, 1, 3});
            PermuteSelf(v, {0, 2, 1, 3});

            RepeatKV(k, num_key_value_groups);
            RepeatKV(v, num_key_value_groups);

            qSize = {bsz * num_attention_heads, seqlen, -1};
            q.Reshape(qSize);
            k.Reshape(qSize);
            v.Reshape(qSize);

            Data &pastKey = pastKeyValues[i].first, &pastValue = pastKeyValues[i].second;
            int unitLen = 64;
#ifdef USE_CUDA
            unitLen = 128;
#endif
            while ((pastKey.dims.size() == 0 && (pastKey.expansionDims.size() == 0 || k.dims[1] > pastKey.expansionDims[1]))
                   || (pastKey.dims.size() > 0 && pastKey.dims[1] + k.dims[1] > pastKey.expansionDims[1])) {
                std::vector <int> newDims;
                if (pastKey.Count(0) == 0 || pastKey.dims.size() == 0) {
                    newDims = std::vector <int> {k.dims[0], ((k.dims[1] - 1) / unitLen + 1) * unitLen, k.dims[2]};
                } else {
                    newDims = pastKey.dims;
                    newDims[1] += ((k.dims[1] - 1) / unitLen + 1) * unitLen;
                }
                pastKey.Expansion(newDims);
            }
            while ((pastValue.dims.size() == 0 && (pastValue.expansionDims.size() == 0 || v.dims[1] > pastValue.expansionDims[1]))
                   || (pastValue.dims.size() > 0 && pastValue.dims[1] + v.dims[1] > pastValue.expansionDims[1])) {
                std::vector <int> newDims;
                if (pastValue.Count(0) == 0 || pastValue.dims.size() == 0) {
                    newDims = std::vector <int> {v.dims[0], ((v.dims[1] - 1) / unitLen + 1) * unitLen, v.dims[2]};
                } else {
                    newDims = pastValue.dims;
                    newDims[1] += ((v.dims[1] - 1) / unitLen + 1) * unitLen;
                }
                pastValue.Expansion(newDims);
            }

            CatDirect(pastKey, k, 1);
            CatDirect(pastValue, v, 1);

            // 1.2 Attention
            // 1.2.0 q * k^T
            MatMulTransB(q, pastKey, attenWeights, 1.0 / sqrt(head_dim));
            attenWeights.Reshape({1, attenWeights.dims[0], attenWeights.dims[1], attenWeights.dims[2]});
            if (!attentionMask.dims.empty()) {
                AttentionMask(attenWeights, attentionMask, -10000);
            }
            Softmax(attenWeights, attenWeights, -1);
            MatMul(attenWeights, pastValue, attenOutput);

            attenOutput.Reshape({attenOutput.dims[1], attenOutput.dims[2], attenOutput.dims[3]});
            PermuteSelf(attenOutput, {1, 0, 2});
            attenOutput.Reshape({seqlen, bsz, -1});
            PermuteSelf(attenOutput, {1, 0, 2});

            Linear(attenOutput, weight[oWeightName], Data(), attenLastOutput);
            AddTo(hiddenStates, attenLastOutput);
            // 2. mlp
            RMSNorm(hiddenStates, this->weight["model.layers." + std::to_string(i) + ".post_attention_layernorm.weight"], 1e-6, attenInput);
            Linear(attenInput, weight["model.layers." + std::to_string(i) + ".mlp.gate_proj.weight"], Data(), w1);
            Linear(attenInput, weight["model.layers." + std::to_string(i) + ".mlp.up_proj.weight"], Data(), w3);
            Silu(w1, w1);
            MulTo(w1, w3);
            Linear(w1, weight["model.layers." + std::to_string(i) + ".mlp.down_proj.weight"], Data(), w2);
            AddTo(hiddenStates, w2);
        }

        RMSNorm(hiddenStates, weight["model.norm.weight"], 1e-6, hiddenStates);
        Data logits;
        Linear(hiddenStates, weight["lm_head.weight"], Data(), logits);
        logits.ToDevice(DataDevice::CPU);

        std::vector <int> lastRet;
        if (generationConfig.IsSimpleGreedy()) {
            for (int b = 0; b < batch; b++) {
                int base = b * logits.dims[1] + logits.dims[1] - 1;
                std::pair <float, int> ret = std::make_pair(-1e9, -1);
                for (int i = 0; i < logits.dims.back(); i++) {
                    ret = max(ret, std::make_pair(((float *) logits.cpuData)[base * logits.dims.back() + i], i));
                }
                lastRet.push_back(ret.second);
            }
        } else {
            for (int b = 0; b < batch; b++) {
                int base = b * logits.dims[1] + logits.dims[1] - 1;
                lastRet.push_back(LLMSampling(logits, base, generationConfig, lastTokens.units[b]));
            }
        }

        return lastRet;
    }
    
    std::vector <int> DeciCoderModel::ForwardBatch(int batch,
                                                   const Data &inputIds,
                                                   const std::vector <Data*> &attentionMask,
                                                   const std::vector <Data*> &positionIds,
                                                   const std::vector <int> &seqLens,
                                                   std::vector <std::pair <Data*, Data*> > &pastKeyValues,
                                                   const std::vector <GenerationConfig> &generationConfigs,
                                                   const LastTokensManager &lastTokens,
                                                   std::vector <std::vector <float>*> *retLogits) {
        Data hiddenStates;
        Data attenInput;
        Data q, k, v, qkv;
        Data attenWeights, curAttenOutput;
        Data attenLastOutput;
        Data w1, w2, w3;

        Embedding(inputIds, this->weight["model.embed_tokens.weight"], hiddenStates);
        int seqlen = hiddenStates.dims[1];
        for (int i = 0; i < block_cnt; i++) {
            ApplyDeviceMap(this->deviceMap, i + 1, block_cnt);
            RMSNorm(hiddenStates, this->weight["model.layers." + std::to_string(i) + ".input_layernorm.weight"],
                    1e-6, attenInput);
            std::string qWeightName = "model.layers." + std::to_string(i) + ".self_attn.q_proj.weight";
            std::string kWeightName = "model.layers." + std::to_string(i) + ".self_attn.k_proj.weight";
            std::string vWeightName = "model.layers." + std::to_string(i) + ".self_attn.v_proj.weight";
            std::string oWeightName = "model.layers." + std::to_string(i) + ".self_attn.o_proj.weight";

            // 1.1 Get q, k, v
            int bsz = attenInput.dims[0], seqlen = attenInput.dims[1];
            Linear(attenInput, weight[qWeightName], Data(), q);
            Linear(attenInput, weight[kWeightName], Data(), k);
            Linear(attenInput, weight[vWeightName], Data(), v);

            Data attenOutput = Data(DataType::FLOAT32);
            int total = 0;
            std::vector <Data> curKs, curVs, curQs;
            curKs.resize(batch);
            curVs.resize(batch);
            curQs.resize(batch);
            for (int b = 0; b < batch; b++) {
                Split(k, 1, total, total + seqLens[b], curKs[b]);
                Split(v, 1, total, total + seqLens[b], curVs[b]);
                Split(q, 1, total, total + seqLens[b], curQs[b]);
                total += seqLens[b];
            }

            for (int b = 0; b < batch; b++) {
                auto &q = curQs[b], &k = curKs[b], &v = curVs[b];

                std::vector <int> qSize = {bsz, seqlen, num_attention_heads, -1};
                std::vector <int> kSize = {bsz, seqlen, num_key_value_heads, -1};
                std::vector <int> vSize = {bsz, seqlen, num_key_value_heads, -1};
                q.Reshape(qSize);
                k.Reshape(kSize);
                v.Reshape(vSize);

                fastllm::LlamaRotatePosition2D(q, *positionIds[b], sinData, cosData, rotary_dim);
                fastllm::LlamaRotatePosition2D(k, *positionIds[b], sinData, cosData, rotary_dim);

                PermuteSelf(q, {0, 2, 1, 3});
                PermuteSelf(k, {0, 2, 1, 3});
                PermuteSelf(v, {0, 2, 1, 3});

                RepeatKV(k, num_key_value_groups);
                RepeatKV(v, num_key_value_groups);

                qSize = {bsz * num_attention_heads, seqLens[b], -1};
                q.Reshape(qSize);
                k.Reshape(qSize);
                v.Reshape(qSize);

                Data &pastKey = *pastKeyValues[b * block_cnt + i].first, &pastValue = *pastKeyValues[b * block_cnt + i].second;
                int unitLen = 64;
#ifdef USE_CUDA
                unitLen = 128;
#endif
                while ((pastKey.dims.size() == 0 &&
                        (pastKey.expansionDims.size() == 0 || k.dims[1] > pastKey.expansionDims[1]))
                       || (pastKey.dims.size() > 0 && pastKey.dims[1] + k.dims[1] > pastKey.expansionDims[1])) {
                    std::vector<int> newDims;
                    if (pastKey.Count(0) == 0 || pastKey.dims.size() == 0) {
                        newDims = std::vector<int>{k.dims[0], ((k.dims[1] - 1) / unitLen + 1) * unitLen, k.dims[2]};
                    } else {
                        newDims = pastKey.dims;
                        newDims[1] += ((k.dims[1] - 1) / unitLen + 1) * unitLen;
                    }
                    pastKey.Expansion(newDims);
                }
                while ((pastValue.dims.size() == 0 &&
                        (pastValue.expansionDims.size() == 0 || v.dims[1] > pastValue.expansionDims[1]))
                       || (pastValue.dims.size() > 0 && pastValue.dims[1] + v.dims[1] > pastValue.expansionDims[1])) {
                    std::vector<int> newDims;
                    if (pastValue.Count(0) == 0 || pastValue.dims.size() == 0) {
                        newDims = std::vector<int>{v.dims[0], ((v.dims[1] - 1) / unitLen + 1) * unitLen, v.dims[2]};
                    } else {
                        newDims = pastValue.dims;
                        newDims[1] += ((v.dims[1] - 1) / unitLen + 1) * unitLen;
                    }
                    pastValue.Expansion(newDims);
                }

                CatDirect(pastKey, k, 1);
                CatDirect(pastValue, v, 1);

                // 1.2 Attention
                // 1.2.0 q * k^T
                MatMulTransB(q, pastKey, attenWeights, 1.0 / sqrt(head_dim));
                attenWeights.Reshape({1, attenWeights.dims[0], attenWeights.dims[1], attenWeights.dims[2]});
                AttentionMask(attenWeights, *attentionMask[b], -10000);

                Softmax(attenWeights, attenWeights, -1);
                MatMul(attenWeights, pastValue, curAttenOutput);
                curAttenOutput.Reshape({curAttenOutput.dims[1], curAttenOutput.dims[2], curAttenOutput.dims[3]});
                PermuteSelf(curAttenOutput, {1, 0, 2});
                curAttenOutput.Reshape({seqLens[b], bsz, -1});
                PermuteSelf(curAttenOutput, {1, 0, 2});
                if (attenOutput.dims.size() == 0) {
                    std::vector <int> dims = curAttenOutput.dims;
                    dims[1] = total;
                    attenOutput.Expansion(dims);
                }
                CatDirect(attenOutput, curAttenOutput, 1);
            }

            Linear(attenOutput, weight[oWeightName], Data(), attenLastOutput);
            AddTo(hiddenStates, attenLastOutput);
            // 2. mlp
            RMSNorm(hiddenStates, this->weight["model.layers." + std::to_string(i) + ".post_attention_layernorm.weight"], 1e-6, attenInput);
            Linear(attenInput, weight["model.layers." + std::to_string(i) + ".mlp.gate_proj.weight"], Data(), w1);
            Linear(attenInput, weight["model.layers." + std::to_string(i) + ".mlp.up_proj.weight"], Data(), w3);
            Silu(w1, w1);
            MulTo(w1, w3);
            Linear(w1, weight["model.layers." + std::to_string(i) + ".mlp.down_proj.weight"], Data(), w2);
            AddTo(hiddenStates, w2);
        }

        RMSNorm(hiddenStates, weight["model.norm.weight"], 1e-6, hiddenStates);
        Data logits;
        Linear(hiddenStates, weight["lm_head.weight"], Data(), logits);
        logits.ToDevice(DataDevice::CPU);
        std::vector <int> lastRet;
        int total = 0;
        for (int b = 0; b < batch; b++) {
            if (generationConfigs[b].output_logits && retLogits != nullptr && (*retLogits)[b] != nullptr) {
                int base = (total + seqLens[b] - 1);
                (*retLogits)[b]->resize(logits.dims.back());
                memcpy((float*)(*retLogits)[b]->data(), (float*)(logits.cpuData + base * logits.dims.back() * logits.unitSize), logits.dims.back() * logits.unitSize);
            }
            if (generationConfigs[b].IsSimpleGreedy()) {
                std::pair<float, int> ret = std::make_pair(-1e9, -1);
                int base = (total + seqLens[b] - 1);
                total += seqLens[b];
                for (int i = 0; i < logits.dims.back(); i++) {
                    ret = max(ret, std::make_pair(((float *) logits.cpuData)[base * logits.dims.back() + i], i));
                }
                lastRet.push_back(ret.second);
            } else {
                int base = (total + seqLens[b] - 1);
                total += seqLens[b];
                lastRet.push_back(LLMSampling(logits, base, generationConfigs[b], lastTokens.units[b]));
            }
        }
        return lastRet;
    }

    std::string DeciCoderModel::MakeInput(const std::string &history, int round, const std::string &input) {
        return (round == 0 ? pre_prompt : history) + user_role + input + bot_role;
    }

    std::string DeciCoderModel::MakeHistory(const std::string &history, int round, 
                                            const std::string &input, const std::string &output) {
        return (round == 0 ? pre_prompt : history) + user_role + input + bot_role + output + history_sep;
    }

    void DeciCoderModel::FillLLMInputs(std::vector <std::vector <float> > &inputTokens,
                                       const std::map <std::string, int> &params,
                                       Data &inputIds, Data &attentionMask, Data &positionIds) {
        int index = params.find("index")->second;
        int promptLen = params.find("promptLen")->second;
        inputIds.ToDevice(DataDevice::CPU);
        attentionMask.ToDevice(DataDevice::CPU);
        positionIds.ToDevice(DataDevice::CPU);
        if (index == 0) {
            int seqLen = inputTokens[0].size();
            std::vector <float> vmask = std::vector <float> (seqLen * seqLen, 0);
            std::vector<float> vpids = std::vector<float>(seqLen, 0);
            for (int i = 0; i < seqLen; i++) {
                vpids[i] = i;
                for (int j = i + 1; j < seqLen; j++) {
                    vmask[i * seqLen + j] = 1;
                }
            }
            inputIds.CopyFrom(Data(DataType::FLOAT32, {1, seqLen}, inputTokens[0]));
            attentionMask.CopyFrom(Data(DataType::FLOAT32, {seqLen, seqLen}, vmask));
            positionIds.CopyFrom(Data(DataType::FLOAT32, {1, seqLen}, vpids));
        } else {
            inputIds.CopyFrom(Data(DataType::FLOAT32, {1, 1}, inputTokens[0]));
            attentionMask.CopyFrom(Data());
            positionIds.CopyFrom(Data(DataType::FLOAT32, {1, 1}, {(float) (promptLen + index - 1)}));
        }
    }

    void DeciCoderModel::FillLLMInputsBatch(std::vector <std::vector <float> > &inputTokens,
                                            const std::vector <std::map <std::string, int> > &params,
                                            Data &inputIds, Data &attentionMask, Data &positionIds) {
        int batch = inputTokens.size();
        int index = params[0].find("index")->second;
        int promptLen = params[0].find("promptLen")->second;

        inputIds.ToDevice(DataDevice::CPU);
        attentionMask.ToDevice(DataDevice::CPU);
        positionIds.ToDevice(DataDevice::CPU);
        
        if (index == 0) {
            int seqLen = inputTokens[0].size();
            std::vector<float> ids = std::vector<float>(batch * seqLen, 0);
            std::vector <float> vmask = std::vector <float> (batch * seqLen * seqLen, 0);
            std::vector<float> vpids = std::vector<float>(batch * seqLen, 0);
            for (int b = 0; b < batch; b++) {
                for (int i = 0; i < seqLen; i++) {
                    ids[b * seqLen + i] = inputTokens[b][i];
                }
            }
            for (int i = 0; i < seqLen; i++) {
                vpids[i] = i;
                for (int j = i + 1; j < seqLen; j++) {
                    vmask[i * seqLen + j] = 1;
                }
            }
            for (int b = 1; b < batch; b++) {
                memcpy(vmask.data() + b * seqLen * seqLen, vmask.data(), seqLen * seqLen * sizeof(float));
                memcpy(vpids.data() + b * seqLen, vpids.data(), seqLen * sizeof(float));
            }
            inputIds.CopyFrom(Data(DataType::FLOAT32, {batch, seqLen}, ids));
            attentionMask.CopyFrom(Data(DataType::FLOAT32, {batch, seqLen, seqLen}, vmask));
            positionIds.CopyFrom(Data(DataType::FLOAT32, {batch, seqLen}, vpids));
        } else {
            std::vector<float> ids = std::vector<float>(batch * 1, 0);
            std::vector<float> vpids = std::vector<float>(batch * 1, 0);
            for (int b = 0; b < batch; b++) {
                ids[b] = inputTokens[b][0];
                vpids[b] = (float) (promptLen + index - 1);
            }
            inputIds.CopyFrom(Data(DataType::FLOAT32, {batch, 1}, ids));
            attentionMask.CopyFrom(Data());
            positionIds.CopyFrom(Data(DataType::FLOAT32, {batch, 1}, vpids));
        }
    }

    void DeciCoderModel::WarmUp() {
        printf("Warmup...\n");
        Data inputIds = Data(DataType::FLOAT32, {1, 1}, {1});
        Data attentionMask = Data(DataType::FLOAT32, {1, 1}, {0});
        Data positionIds = Data(DataType::FLOAT32, {1, 1}, {0, 0});

        std::vector <std::pair <Data, Data> > pastKeyValues;
        for (int i = 0; i < block_cnt; i++) {
            pastKeyValues.push_back(std::make_pair(Data(DataType::FLOAT32),
                                                   Data(DataType::FLOAT32)));
        }
        Forward(inputIds, attentionMask, positionIds, pastKeyValues);
#ifdef USE_TFACC40T
        FastllmTfaccReleaseTempMemory();
#endif
        printf("finish.\n");
    }
}