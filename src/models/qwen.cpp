//
// Created by siemon on 8/9/23.
//

#include "utils.h"

#include "qwen.h"

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

    void SaveFile(const Data &data, const std::string &fileName) {
        float *ptr = (float *) data.cpuData;
        int len = data.Count(0);

        FILE *outfile = std::fopen(fileName.c_str(), "w");
        for (int i = 0; i < len; i++) {
            fprintf(outfile, "%f\n", ptr[i]);
        }
        std::fclose(outfile);
    }

    inline bool StringCompLoc(const std::string &str, const std::string &comp, size_t loc) {
        if (loc >= str.size()) {
            return false;
        }
        if (comp.size() + loc >= str.size()) {
            return false;
        }
        for (size_t i = 0; i < comp.size(); i++) {
            if (str[loc + i] != comp[i]) {
                return false;
            }
        }
        return true;
    }

    std::string VisualModel::FromListFormat(const visualDictData &data) {
        std::string text;
        int num_images = 0;
        for (auto input : data) {
            if (input.find("image") != input.end()) {
                num_images += 1;
                text += "Pictures ";
                text += std::to_string(num_images);
                text += image_start_tag;
                text += input["image"];
                text += image_end_tag;
                text += "\n";
            } else if (input.find("text") != input.end()) {
                text += input["text"];
            } else if (input.find("box") != input.end()) {
                if (input.find("ref") != input.end()) {
                    text += ref_start_tag;
                    text += input["ref"];
                    text += ref_end_tag;
                }
                text += box_start_tag;
                text += input["box"];
                text += box_end_tag;
            }
        }
        return text;
    }

    visualDictData VisualModel::ToListFormat(const std::string &text) {
        visualDictData data;
        int i = 0;
        while (i < text.size()) {
            std::string key;
            std::string value;
            if (StringCompLoc(text, "<ref>", i)) {
                key = "ref";
                i += 5;
                while (!StringCompLoc(text, "</ref>", i)) {
                    value += text[i];
                    i++;
                }
                i += 6;
                // after ref, there is box
                AssertInFastLLM(StringCompLoc(text, "<box>", i), "There must be <box> after <ref>.\n");
                std::string secondkey;
                std::string secondvalue;
                secondkey = "box";
                i += 5;
                while (!StringCompLoc(text, "</box>", i)) {
                    secondvalue += text[i];
                    i++;
                }
                i += 6;
                data.push_back({{key, value}, {secondkey, secondvalue}});
            } else if (StringCompLoc(text, "<box>", i)) {
                key = "box";
                i += 5;
                while (!StringCompLoc(text, "</box>", i)) {
                    value += text[i];
                    i++;
                }
                i += 6;
                data.push_back({{key, value}});
            } else if (StringCompLoc(text, "<quad>", i)) {
                key = "quad";
                i += 6;
                while (!StringCompLoc(text, "</quad>", i)) {
                    value += text[i];
                    i++;
                }
                i += 7;
                data.push_back({{key, value}});
            } else if (StringCompLoc(text, "<img>", i)) {
                key = "image";
                i += 5;
                while (!StringCompLoc(text, "</img>", i)) {
                    value += text[i];
                    i++;
                }
                i += 6;
                data.push_back({{key, value}});
            } else {
                key = "text";
                while (i < text.size() &&
                       !StringCompLoc(text, "<ref>", i) &&
                       !StringCompLoc(text, "<box>", i) &&
                       !StringCompLoc(text, "<quad>", i) &&
                       !StringCompLoc(text, "<img>", i)) {
                    value += text[i];
                    i++;
                }
                data.push_back({{key, value}});
            }
        }
        return data;
    }

    VisualModel::VisualModel(WeightMap *weight) {
        width = 1664;
        imageSize = 448;
        nQueries = 256;
        outputDim = 4096;
        patchSize = 14;
        layers = 48;

        heads = 16;
        grid_size = 16;

        samplerNumHead = 32;
        samplerHeadDim = 128;

        this->weight = weight;
        // parse weight dict
        std::string visual_dict = weight->dicts["visual"];

        this->weight->tokenizer.specialTokenToStringDict[151851] = "<ref>";
        this->weight->tokenizer.specialTokenToStringDict[151852] = "</ref>";
        this->weight->tokenizer.specialTokenToStringDict[151853] = "<box>";
        this->weight->tokenizer.specialTokenToStringDict[151854] = "</box>";
        this->weight->tokenizer.specialTokenToStringDict[151855] = "<quad>";
        this->weight->tokenizer.specialTokenToStringDict[151856] = "</quad>";
        this->weight->tokenizer.specialTokenToStringDict[151857] = "<img>";
        this->weight->tokenizer.specialTokenToStringDict[151858] = "</img>";
        this->weight->tokenizer.specialTokenToStringDict[151859] = "<imgpad>";
        for (auto specialPair : this->weight->tokenizer.specialTokenToStringDict) {
            this->weight->tokenizer.specialStringToTokenDict[specialPair.second] = specialPair.first;
        }
        
        // initialize mean and std
        std::vector<float> meanData = {0.48145466, 0.4578275, 0.40821073};
        std::vector<float> stdData = {0.26862954, 0.26130258, 0.27577711};
        mean.CopyFrom(Data(DataType::FLOAT32, {1, 3}, meanData));
        std.CopyFrom(Data(DataType::FLOAT32, {1, 3}, stdData));

        Data temp = Data(DataType::FLOAT32);
        int src_size = (int) sqrt(nQueries);
        int tgt_size = (int) (imageSize / patchSize);

        // initialize positionalEmbedding
        temp.CopyFrom((*weight)["transformer.visual.positional_embedding"]);
        if (src_size != tgt_size) {
            temp.Reshape({1, src_size, src_size, -1});
            PermuteSelf(temp, {0, 3, 1, 2});
            Interpolate(temp, positionalEmbedding, tgt_size / src_size, 0, false);
            PermuteSelf(positionalEmbedding, {0, 2, 3, 1});
            positionalEmbedding.Reshape({tgt_size * tgt_size, -1});
        } else {
            positionalEmbedding.CopyFrom(temp);
        }
        positionalEmbedding.Unsqueeze(0);

        // initialize samplerPosEmbed
        std::vector<float> gridData(2 * grid_size * grid_size);
        float *grid_h = gridData.data();
        float *grid_w = gridData.data() + grid_size * grid_size;
        for (int i = 0; i < grid_size; i++) {
            for (int j = 0; j < grid_size; j++) {
                grid_h[i * grid_size + j] = j;
                grid_w[i * grid_size + j] = i;
            }
        }

        std::vector<float> omega;
        for (int i = 0; i < outputDim / 4; i++) {
            omega.push_back(1.0 / pow(10000.f, (float) i / (outputDim / 4)));
        }

        std::vector<float> samplerPosEmbedData(grid_size * grid_size * outputDim);
        for (int i = 0; i < grid_size * grid_size; i++) {
            for (int j = 0; j < outputDim / 4; j++) {
                samplerPosEmbedData[i * outputDim + outputDim / 4 * 0 + j] = ::sin(grid_h[i] * omega[j]);
                samplerPosEmbedData[i * outputDim + outputDim / 4 * 1 + j] = ::cos(grid_h[i] * omega[j]);
                samplerPosEmbedData[i * outputDim + outputDim / 4 * 2 + j] = ::sin(grid_w[i] * omega[j]);
                samplerPosEmbedData[i * outputDim + outputDim / 4 * 3 + j] = ::cos(grid_w[i] * omega[j]);
            }
        }
        samplerPosEmbed.CopyFrom(Data(DataType::FLOAT32, {grid_size * grid_size, outputDim}, samplerPosEmbedData));
        if (src_size != tgt_size) {
            temp.CopyFrom(samplerPosEmbed);
            temp.Reshape({1, src_size, src_size, -1});
            PermuteSelf(temp, {0, 3, 1, 2});
            Interpolate(temp, samplerPosEmbedAbsPos, tgt_size / src_size, 0, false);
            PermuteSelf(samplerPosEmbedAbsPos, {0, 2, 3, 1});
            samplerPosEmbedAbsPos.Reshape({tgt_size * tgt_size, -1});
        } else {
            samplerPosEmbedAbsPos.CopyFrom(samplerPosEmbed);
        }
        samplerPosEmbed.Unsqueeze(1);
        samplerPosEmbedAbsPos.Unsqueeze(1);

        return;
    }

    void VisualModel::Forward(const Data &images, Data *decodes) {
        Data hiddenStates;
        Data q, k, v;
        Data attnInput, attnOutput, attnWeights, attnFinalOutput;

        Conv2d(const_cast<Data &>(images), (*weight)["transformer.visual.conv1.weight"], Data(), hiddenStates, patchSize);
        hiddenStates.Reshape({hiddenStates.dims[0], hiddenStates.dims[1], -1});
        PermuteSelf(hiddenStates, {0, 2, 1});
        
        AddTo(hiddenStates, positionalEmbedding);
        LayerNorm(hiddenStates, (*weight)["transformer.visual.ln_pre.weight"], 
                                (*weight)["transformer.visual.ln_pre.bias"], -1, hiddenStates);
        PermuteSelf(hiddenStates, {1, 0, 2});

        int emb_dim = width / heads;
        for (int i = 0; i < layers; i++) {
            printf("image decode [%d/%d]\r", i + 1, layers);
            fflush(stdout);

            std::string pre = "transformer.visual.transformer.resblocks." + std::to_string(i) + ".";
            LayerNorm(hiddenStates, (*weight)[pre + "ln_1.weight"], (*weight)[pre + "ln_1.bias"], -1, attnInput);

            // attention
            int sq = attnInput.dims[0], b = attnInput.dims[1];
            Linear(attnInput, (*weight)[pre + "attn.in_proj.weight"], (*weight)[pre + "attn.in_proj.bias"], attnOutput);
            attnOutput.Reshape({sq, b, heads, 3 * emb_dim});
            Split(attnOutput, 3, 0, emb_dim, q);
            Split(attnOutput, 3, emb_dim, 2 * emb_dim, k);
            Split(attnOutput, 3, 2 * emb_dim, 3 * emb_dim, v);

            q.Reshape({sq, b * heads, emb_dim});
            k.Reshape({sq, b * heads, emb_dim});
            v.Reshape({sq, b * heads, emb_dim});
            PermuteSelf(q, {1, 0, 2});
            PermuteSelf(k, {1, 0, 2});
            PermuteSelf(v, {1, 0, 2});

            MatMulTransB(q, k, attnWeights, 1.0 / sqrt(emb_dim));
            Softmax(attnWeights, attnWeights, -1);
            MatMul(attnWeights, v, attnOutput);

            attnOutput.Reshape({b, heads, sq, emb_dim});
            PermuteSelf(attnOutput, {2, 0, 1, 3});
            attnOutput.Reshape({sq, b, width});

            Linear(attnOutput, (*weight)[pre + "attn.out_proj.weight"], (*weight)[pre + "attn.out_proj.bias"], attnFinalOutput);
            AddTo(hiddenStates, attnFinalOutput);

            // mlp
            LayerNorm(hiddenStates, (*weight)[pre + "ln_2.weight"], (*weight)[pre + "ln_2.bias"], -1, attnInput);
            Linear(attnInput, (*weight)[pre + "mlp.c_fc.weight"], (*weight)[pre + "mlp.c_fc.bias"], attnOutput);
            GeluNew(attnOutput, attnOutput);
            Linear(attnOutput, (*weight)[pre + "mlp.c_proj.weight"], (*weight)[pre + "mlp.c_proj.bias"], attnFinalOutput);
            AddTo(hiddenStates, attnFinalOutput);
        }
        PermuteSelf(hiddenStates, {1, 0, 2});

        // attn pool
        Data sampInput;
        Linear(hiddenStates, (*weight)["transformer.visual.attn_pool.kv_proj.weight"], Data(), sampInput);
        LayerNorm(sampInput, (*weight)["transformer.visual.attn_pool.ln_kv.weight"], 
                             (*weight)["transformer.visual.attn_pool.ln_kv.bias"], -1, sampInput);
        PermuteSelf(sampInput, {1, 0, 2});

        Data query, key, value;
        LayerNorm((*weight)["transformer.visual.attn_pool.query"], 
                  (*weight)["transformer.visual.attn_pool.ln_q.weight"], 
                  (*weight)["transformer.visual.attn_pool.ln_q.bias"], -1, query);
        query.Unsqueeze(1);
        Repeat(query, 1, sampInput.dims[1], q);
        AddTo(q, samplerPosEmbed);
        
        k.CopyFrom(sampInput);
        AddTo(k, samplerPosEmbedAbsPos);

        v.CopyFrom(sampInput);

        // multihead attention
        int tgt_len = q.dims[0];
        int src_len = k.dims[0];
        int bsz = q.dims[1];

        if (samplerWQ.dims.empty()) {
            std::string w_str = "transformer.visual.attn_pool.attn.in_proj_weight";
            std::string b_str = "transformer.visual.attn_pool.attn.in_proj_bias";
            Split((*weight)[w_str], 0, 0 * outputDim, 1 * outputDim, samplerWQ);
            Split((*weight)[w_str], 0, 1 * outputDim, 2 * outputDim, samplerWK);
            Split((*weight)[w_str], 0, 2 * outputDim, 3 * outputDim, samplerWV);
            Split((*weight)[b_str], 0, 0 * outputDim, 1 * outputDim, samplerBQ);
            Split((*weight)[b_str], 0, 1 * outputDim, 2 * outputDim, samplerBK);
            Split((*weight)[b_str], 0, 2 * outputDim, 3 * outputDim, samplerBV);
        }
        Linear(q, samplerWQ, samplerBQ, query);
        Linear(k, samplerWK, samplerBK, key);
        Linear(v, samplerWV, samplerBV, value);

        query.Reshape({tgt_len, bsz * samplerNumHead, samplerHeadDim});
        key.Reshape({src_len, bsz * samplerNumHead, samplerHeadDim});
        value.Reshape({src_len, bsz * samplerNumHead, samplerHeadDim});
        PermuteSelf(query, {1, 0, 2});
        PermuteSelf(key, {1, 0, 2});
        PermuteSelf(value, {1, 0, 2});

        MatMulTransB(query, key, attnWeights, 1.f / sqrtf(query.dims.back()));
        Softmax(attnWeights, attnWeights, -1);
        MatMul(attnWeights, value, attnOutput);
        PermuteSelf(attnOutput, {1, 0, 2});
        attnOutput.Reshape({tgt_len * bsz, outputDim});
        Linear(attnOutput, (*weight)["transformer.visual.attn_pool.attn.out_proj.weight"], 
                           (*weight)["transformer.visual.attn_pool.attn.out_proj.bias"], attnFinalOutput);
        attnFinalOutput.Reshape({tgt_len, bsz, attnFinalOutput.dims[1]});
        PermuteSelf(attnFinalOutput, {1, 0, 2});

        // ln_post & proj
        LayerNorm(attnFinalOutput, (*weight)["transformer.visual.ln_post.weight"], 
                                   (*weight)["transformer.visual.ln_post.bias"], -1, hiddenStates);
        
        if ((*weight)["transformer.visual.proj"].dims.size() == 2) {
            (*weight)["transformer.visual.proj"].Unsqueeze(0);
        }
        if (hiddenStates.dims[0] == 1) {
            MatMul(hiddenStates, (*weight)["transformer.visual.proj"], *decodes);
        } else {
            Data proj;
            Repeat((*weight)["transformer.visual.proj"], 0, hiddenStates.dims[0], proj);
            MatMul(hiddenStates, proj, *decodes);
        }

        return;
    }

    void VisualModel::Decode(const std::vector<std::string> &imagePaths, Data *decodes) {
        Data allImage = Data(DataType::FLOAT32);
        int n = (int) imagePaths.size();
        allImage.Expansion({n, 3, imageSize, imageSize});

        for (auto path : imagePaths) {
            Data image;
            LoadImageData(path, "RGB", imageSize, image);
            ImageNormalize(image, mean, std, true);
            CatDirect(allImage, image, 0);
        }

        Forward(allImage, decodes);

        // 缓存图片feature
        for (int i = 0; i < imagePaths.size(); i++) {
            imageMap[imagePaths[i]] = new Data(DataType::FLOAT32);
            Split(*decodes, 0, i, i + 1, *imageMap[imagePaths[i]]);
        }

        return;
    } 

    QWenModel::QWenModel() {
        this->model_type = "qwen";
        this->pre_prompt = "You are a helpful assistant.";
        this->user_role = "user";
        this->bot_role = "assistant";

        this->weight.tokenizer.specialTokenToStringDict[151643] = "<|endoftext|>";
        this->weight.tokenizer.specialTokenToStringDict[151644] = "<|im_start|>";
        this->weight.tokenizer.specialTokenToStringDict[151645] = "<|im_end|>";
        for (auto specialPair : this->weight.tokenizer.specialTokenToStringDict) {
            this->weight.tokenizer.specialStringToTokenDict[specialPair.second] = specialPair.first;
        }

        embed_dim = 4096;
		num_attention_heads = 32;
		head_dim = embed_dim / num_attention_heads;
		block_cnt = 32;
        rotary_dim = 128;
        seq_length = 2048;
        use_log_attn = true;

        ntk_alpha = 1.f;
        UpdateRotaryPosEmb(ntk_alpha);

        if (use_log_attn) {
            logn_list = Data(DataType::FLOAT32);
            logn_list.Resize({1, max_positions, 1, 1});
            logn_list.Allocate();
            float *logn = (float *) logn_list.cpuData;
            for (int i = 0; i < seq_length; i++) {
                logn[i] = 1;
            }
            for (int i = seq_length; i < max_positions; i++) {
                logn[i] = std::log(i) / std::log(seq_length);
            }
        }

        weight.embeddingNames.insert("transformer.wte.weight");
    }

    void QWenModel::LoadFromFile(const std::string &fileName) {
        this->weight.LoadFromFile(fileName);
        this->InitParams();
        if (weight.dicts.find("visual") != weight.dicts.end()) {
            this->visual = new VisualModel(&this->weight);
        }
    }

    int QWenModel::Forward(const Data &inputIds,
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

    std::vector <int> QWenModel::ForwardBatch(int batch,
                                              const Data &inputIds,
                                              const Data &attentionMask,
                                              const Data &positionIds,
                                              std::vector <std::pair <Data, Data> > &pastKeyValues,
                                              const GenerationConfig &generationConfig,
                                              const LastTokensManager &lastTokens,
                                              std::vector <std::vector <float>*> *retLogits) {
        int maxLen = inputIds.dims[1];                                        
        Data hiddenStates;
        Data attnInput, attnOutput;
        Data query, key, value;
        Data attnWeights, attnLastOutput;
        Data a1, a2, mlpOutput;

        // printf("input id: ");
        // for (int i = 0; i < inputIds.Count(0); i++) {
        //     printf("%d ", (int )((float *) inputIds.cpuData)[i]);
        // }
        // printf("\n");
        Embedding(inputIds, this->weight["transformer.wte.weight"], hiddenStates);
        if (visual) {
            float *inputIdsData = (float *) inputIds.cpuData;
            float *hiddenStatesData = (float *) hiddenStates.cpuData;
            for (int b = 0; b < batch; b++) {
                std::vector<std::string> imagePath;
                std::vector<int> imageLocs;
                for (int i = 0; i < inputIds.Count(0) / batch; i++) {
                    if ((int) inputIdsData[i] == image_start_id) {
                        std::string path;
                        for (int j = i + 1; j <= i + 256; j++) {
                            if (inputIdsData[j]) {
                                path += (int) inputIdsData[j];
                            }
                        }
                        if (visual->imageMap.find(path) != visual->imageMap.end()) {
                            auto image = visual->imageMap[path];
                            memcpy(hiddenStatesData + b * hiddenStates.Count(1) + (i + 1) * embed_dim,
                                   (float *) image->cpuData, image->Count(0) * sizeof(float));
                        } else {
                            imageLocs.push_back(i + 1);
                            imagePath.push_back(path);
                        }
                        i += 256;
                    }    
                }
                inputIdsData += inputIds.Count(1);

                if (imagePath.empty()) {
                    continue;
                }

                Data images;
                visual->Decode(imagePath, &images);

                // 将image encoding嵌入到hidden states中
                for (int i = 0; i < imageLocs.size(); i++) {
                    int loc = imageLocs[i];
                    memcpy(hiddenStatesData + b * hiddenStates.Count(1) + loc * embed_dim, 
                           (float *) images.cpuData + i * images.Count(1), images.Count(1) * sizeof(float));
                }
            }
        }
        for (int i = 0; i < this->block_cnt; i++) {
            ApplyDeviceMap(this->deviceMap, i + 1, block_cnt);
            int seqlen = hiddenStates.dims[1];

            std::string ln_1_name = "transformer.h." + std::to_string(i) + ".ln_1.weight";
            std::string attn_weight_name = "transformer.h." + std::to_string(i) + ".attn.c_attn.weight";
            std::string attn_bias_name = "transformer.h." + std::to_string(i) + ".attn.c_attn.bias";

            RMSNorm(hiddenStates, weight[ln_1_name], 1e-6, attnInput);
            Linear(attnInput, weight[attn_weight_name], weight[attn_bias_name], attnOutput); // attnOutput [batch, seqlen, embed_dim * 3]
            Split(attnOutput, 2, 0, embed_dim, query);
            Split(attnOutput, 2, embed_dim, 2 * embed_dim, key);
            Split(attnOutput, 2, embed_dim * 2, embed_dim * 3, value);

            query.Reshape({query.dims[0], query.dims[1], num_attention_heads, head_dim});
            key.Reshape({key.dims[0], key.dims[1], num_attention_heads, head_dim});
            value.Reshape({value.dims[0], value.dims[1], num_attention_heads, head_dim});

            Data &pastKey = pastKeyValues[i].first, &pastValue = pastKeyValues[i].second;
            if (pastKey.dims.empty()) {
                // 计算new_ntk_alpha
                float context_value = std::log2((float) seqlen / seq_length) + 1;
                float new_ntk_alpha = std::max(std::pow(2, std::ceil(context_value) - 1), 1.);
                if (new_ntk_alpha != ntk_alpha) {
                    UpdateRotaryPosEmb(new_ntk_alpha);
                }
            }

            LlamaRotatePosition2D(query, positionIds, sinData, cosData, rotary_dim);
            LlamaRotatePosition2D(key, positionIds, sinData, cosData, rotary_dim);

            if (use_log_attn) {
                ApplyLognAttn(query, logn_list, positionIds);
            }

            PermuteSelf(query, {0, 2, 1, 3});
            PermuteSelf(key, {0, 2, 1, 3});
            PermuteSelf(value, {0, 2, 1, 3});

            std::vector<int> qkvSize = {batch * num_attention_heads, seqlen, -1};
            query.Reshape(qkvSize);
            key.Reshape(qkvSize);
            value.Reshape(qkvSize);

            int unitLen = 64;
#ifdef USE_CUDA
            unitLen = 128;
#endif
            while ((pastKey.dims.size() == 0 && (pastKey.expansionDims.size() == 0 || key.dims[1] > pastKey.expansionDims[1]))
                   || (pastKey.dims.size() > 0 && pastKey.dims[1] + key.dims[1] > pastKey.expansionDims[1])) {
                std::vector <int> newDims;
                if (pastKey.Count(0) == 0 || pastKey.dims.size() == 0) {
                    newDims = std::vector <int> {key.dims[0], ((key.dims[1] - 1) / unitLen + 1) * unitLen, key.dims[2]};
                } else {
                    newDims = pastKey.dims;
                    newDims[1] += ((key.dims[1] - 1) / unitLen + 1) * unitLen;
                }
                pastKey.Expansion(newDims);
            }
            while ((pastValue.dims.size() == 0 && (pastValue.expansionDims.size() == 0 || value.dims[1] > pastValue.expansionDims[1]))
                   || (pastValue.dims.size() > 0 && pastValue.dims[1] + value.dims[1] > pastValue.expansionDims[1])) {
                std::vector <int> newDims;
                if (pastValue.Count(0) == 0 || pastValue.dims.size() == 0) {
                    newDims = std::vector <int> {value.dims[0], ((value.dims[1] - 1) / unitLen + 1) * unitLen, value.dims[2]};
                } else {
                    newDims = pastValue.dims;
                    newDims[1] += ((value.dims[1] - 1) / unitLen + 1) * unitLen;
                }
                pastValue.Expansion(newDims);
            }
            CatDirect(pastKey, key, 1);
            CatDirect(pastValue, value, 1);

            // Attention
            MatMulTransB(query, pastKey, attnWeights, 1.0 / sqrt(head_dim));
            attnWeights.Reshape({1, attnWeights.dims[0], attnWeights.dims[1], attnWeights.dims[2]});
            if (!attentionMask.dims.empty()) {
                AttentionMask(attnWeights, attentionMask, -10000);
            }

            Softmax(attnWeights, attnWeights, -1);
            MatMul(attnWeights, pastValue, attnOutput);

            attnOutput.Reshape({attnOutput.dims[1], attnOutput.dims[2], attnOutput.dims[3]});
            PermuteSelf(attnOutput, {1, 0, 2});
            attnOutput.Reshape({seqlen, batch, -1});
            PermuteSelf(attnOutput, {1, 0, 2});

            std::string proj_weight_name = "transformer.h." + std::to_string(i) + ".attn.c_proj.weight";
            Linear(attnOutput, weight[proj_weight_name], Data(), attnLastOutput);
            AddTo(hiddenStates, attnLastOutput);

            std::string ln_2_name = "transformer.h." + std::to_string(i) + ".ln_2.weight";
            RMSNorm(hiddenStates, weight[ln_2_name], 1e-6, attnInput);

            std::string mlp_w1_weight_name = "transformer.h." + std::to_string(i) + ".mlp.w1.weight";
            std::string mlp_w2_weight_name = "transformer.h." + std::to_string(i) + ".mlp.w2.weight";
            std::string mlp_proj_weight_name = "transformer.h." + std::to_string(i) + ".mlp.c_proj.weight";
            Linear(attnInput, weight[mlp_w1_weight_name], Data(), a1);
            Linear(attnInput, weight[mlp_w2_weight_name], Data(), a2);
            Silu(a2, a2);
            MulTo(a1, a2);
            Linear(a1, weight[mlp_proj_weight_name], Data(), mlpOutput);
            AddTo(hiddenStates, mlpOutput);
        }

        RMSNorm(hiddenStates, weight["transformer.ln_f.weight"], 1e-6, hiddenStates);
        Data logits, topk;
        Linear(hiddenStates, weight["lm_head.weight"], Data(), logits);

        std::vector <int> lastRet;
        int total = 0;
        Data curLogitTemp, curLogit;
        for (int b = 0; b < batch; b++) {
            Split(logits, 0, b, b + 1, curLogitTemp);
            Split(curLogitTemp, 1, maxLen - 1, maxLen, curLogit);
            if (generationConfig.output_logits && retLogits != nullptr && (*retLogits)[b] != nullptr) {
                curLogit.ToDevice(DataDevice::CPU);
                (*retLogits)[b]->resize(curLogit.Count(0));
                memcpy((float*)(*retLogits)[b]->data(), (float*)curLogit.cpuData, curLogit.GetBytes());
            }
            if (generationConfig.IsSimpleGreedy()) {
                Data topk;
                TopK(curLogit, topk, 1);
                topk.ToDevice(DataDevice::CPU);
                lastRet.push_back((int) (((float *) topk.cpuData)[0] + 1e-3));
            } else {
                lastRet.push_back(LLMSampling(curLogit, 0, generationConfig, lastTokens.units[b]));
            }
            total += maxLen;
        }
        return lastRet;
    }
    
    std::vector <int> QWenModel::ForwardBatch(int batch,
                                              const Data &inputIds,
                                              const std::vector <Data*> &attentionMask,
                                              const std::vector <Data*> &positionIds,
                                              const std::vector <int> &seqLens,
                                              std::vector <std::pair <Data*, Data*> > &pastKeyValues,
                                              const std::vector <GenerationConfig> &generationConfigs,
                                              const LastTokensManager &lastTokens,
                                              std::vector <std::vector <float>*> *retLogits) {
        int maxLen = inputIds.dims[1];
        Data hiddenStates;
        Data attnInput, attnOutput;
        Data query, key, value;
        Data attnWeights, attnLastOutput;
        Data a1, a2, mlpOutput;

        Embedding(inputIds, this->weight["transformer.wte.weight"], hiddenStates);
        if (visual) {
            float *inputIdsData = (float *) inputIds.cpuData;
            float *hiddenStatesData = (float *) hiddenStates.cpuData;
            for (int b = 0; b < batch; b++) {
                std::vector<std::string> imagePath;
                std::vector<int> imageLocs;
                for (int i = 0; i < seqLens[b]; i++) {
                    if ((int) inputIdsData[i] == image_start_id) {
                        std::string path;
                        for (int j = i + 1; j <= i + 256; j++) {
                            if (inputIdsData[j]) {
                                path += (int) inputIdsData[j];
                            }
                        }
                        if (visual->imageMap.find(path) != visual->imageMap.end()) {
                            auto image = visual->imageMap[path];
                            memcpy(hiddenStatesData + b * hiddenStates.Count(1) + (i + 1) * embed_dim,
                                   (float *) image->cpuData, image->Count(0) * sizeof(float));
                        } else {
                            imageLocs.push_back(i + 1);
                            imagePath.push_back(path);
                        }
                        i += 256;
                    }   
                }
                inputIdsData += inputIds.Count(1);

                if (imagePath.empty()) {
                    continue;
                }

                Data images;
                visual->Decode(imagePath, &images);

                // 将image encoding嵌入到hidden states中
                for (int i = 0; i < imageLocs.size(); i++) {
                    int loc = imageLocs[i];
                    memcpy(hiddenStatesData + b * hiddenStates.Count(1) + loc * embed_dim, 
                           (float *) images.cpuData + i * images.Count(1), images.Count(1) * sizeof(float));
                }
            }
        }
        for (int i = 0; i < this->block_cnt; i++) {
            ApplyDeviceMap(this->deviceMap, i + 1, block_cnt);

            std::string ln_1_name = "transformer.h." + std::to_string(i) + ".ln_1.weight";
            std::string attn_weight_name = "transformer.h." + std::to_string(i) + ".attn.c_attn.weight";
            std::string attn_bias_name = "transformer.h." + std::to_string(i) + ".attn.c_attn.bias";

            RMSNorm(hiddenStates, weight[ln_1_name], 1e-6, attnInput);
            Linear(attnInput, weight[attn_weight_name], weight[attn_bias_name], attnOutput); // attnOutput [batch, seqlen, embed_dim * 3]
            Split(attnOutput, 2, 0, embed_dim, query);
            Split(attnOutput, 2, embed_dim, 2 * embed_dim, key);
            Split(attnOutput, 2, embed_dim * 2, embed_dim * 3, value);

            std::vector<Data> curKs, curVs, curQs;
            curKs.resize(batch);
            curVs.resize(batch);
            curQs.resize(batch);
            int total = 0;
            for (int b = 0; b < batch; b++) {
                Split(query, 1, total, total + seqLens[b], curQs[b]);
                Split(key, 1, total, total + seqLens[b], curKs[b]);
                Split(value, 1, total, total + seqLens[b], curVs[b]);
                total += seqLens[b];
            }

            Data attnOutputAll = Data(DataType::FLOAT32);
            for (int b = 0; b < batch; b++) {
                // in this loop, batch = 1
                auto &query = curQs[b];
                auto &key = curKs[b];
                auto &value = curVs[b];

                query.Reshape({1, seqLens[b], num_attention_heads, head_dim});
                key.Reshape({1, seqLens[b], num_attention_heads, head_dim});
                value.Reshape({1, seqLens[b], num_attention_heads, head_dim});

                Data &pastKey = *pastKeyValues[b * block_cnt + i].first, &pastValue = *pastKeyValues[b * block_cnt + i].second;
                if (pastKey.dims.empty()) {
                    // 计算new_ntk_alpha
                    float context_value = std::log2((float) seqLens[b] / seq_length) + 1;
                    float new_ntk_alpha = std::max(std::pow(2, std::ceil(context_value) - 1), 1.);
                    if (new_ntk_alpha != ntk_alpha) {
                        UpdateRotaryPosEmb(new_ntk_alpha);
                    }
                }

                LlamaRotatePosition2D(query, *positionIds[b], sinData, cosData, rotary_dim);
                LlamaRotatePosition2D(key, *positionIds[b], sinData, cosData, rotary_dim);

                if (use_log_attn) {
                    ApplyLognAttn(query, logn_list, *positionIds[b]);
                }

                PermuteSelf(query, {0, 2, 1, 3});
                PermuteSelf(key, {0, 2, 1, 3});
                PermuteSelf(value, {0, 2, 1, 3});

                std::vector<int> qkvSize = {num_attention_heads, seqLens[b], -1};
                query.Reshape(qkvSize);
                key.Reshape(qkvSize);
                value.Reshape(qkvSize);

                int unitLen = 64;
    #ifdef USE_CUDA
                unitLen = 128;
    #endif
                while ((pastKey.dims.size() == 0 && (pastKey.expansionDims.size() == 0 || key.dims[1] > pastKey.expansionDims[1]))
                    || (pastKey.dims.size() > 0 && pastKey.dims[1] + key.dims[1] > pastKey.expansionDims[1])) {
                    std::vector <int> newDims;
                    if (pastKey.Count(0) == 0 || pastKey.dims.size() == 0) {
                        newDims = std::vector <int> {key.dims[0], ((key.dims[1] - 1) / unitLen + 1) * unitLen, key.dims[2]};
                    } else {
                        newDims = pastKey.dims;
                        newDims[1] += ((key.dims[1] - 1) / unitLen + 1) * unitLen;
                    }
                    pastKey.Expansion(newDims);
                }
                while ((pastValue.dims.size() == 0 && (pastValue.expansionDims.size() == 0 || value.dims[1] > pastValue.expansionDims[1]))
                    || (pastValue.dims.size() > 0 && pastValue.dims[1] + value.dims[1] > pastValue.expansionDims[1])) {
                    std::vector <int> newDims;
                    if (pastValue.Count(0) == 0 || pastValue.dims.size() == 0) {
                        newDims = std::vector <int> {value.dims[0], ((value.dims[1] - 1) / unitLen + 1) * unitLen, value.dims[2]};
                    } else {
                        newDims = pastValue.dims;
                        newDims[1] += ((value.dims[1] - 1) / unitLen + 1) * unitLen;
                    }
                    pastValue.Expansion(newDims);
                }
                CatDirect(pastKey, key, 1);
                CatDirect(pastValue, value, 1);


                MatMulTransB(query, pastKey, attnWeights, 1.0 / sqrt(head_dim));
                attnWeights.Reshape({1, attnWeights.dims[0], attnWeights.dims[1], attnWeights.dims[2]});
                if (attentionMask[b]) {
                    AttentionMask(attnWeights, *attentionMask[b], -10000);
                }

                Softmax(attnWeights, attnWeights, -1);
                MatMul(attnWeights, pastValue, attnOutput);

                attnOutput.Reshape({attnOutput.dims[1], attnOutput.dims[2], attnOutput.dims[3]});
                PermuteSelf(attnOutput, {1, 0, 2});
                attnOutput.Reshape({seqLens[b], 1, -1});
                PermuteSelf(attnOutput, {1, 0, 2});


                if (attnOutputAll.dims.size() == 0) {
                    std::vector <int> dims = attnOutput.dims;
                    dims[1] = total;
                    attnOutputAll.Expansion(dims);
                }
                CatDirect(attnOutputAll, attnOutput, 1);
            }

            std::string proj_weight_name = "transformer.h." + std::to_string(i) + ".attn.c_proj.weight";
            Linear(attnOutputAll, weight[proj_weight_name], Data(), attnLastOutput);
            AddTo(hiddenStates, attnLastOutput);

            std::string ln_2_name = "transformer.h." + std::to_string(i) + ".ln_2.weight";
            RMSNorm(hiddenStates, weight[ln_2_name], 1e-6, attnInput);

            std::string mlp_w1_weight_name = "transformer.h." + std::to_string(i) + ".mlp.w1.weight";
            std::string mlp_w2_weight_name = "transformer.h." + std::to_string(i) + ".mlp.w2.weight";
            std::string mlp_proj_weight_name = "transformer.h." + std::to_string(i) + ".mlp.c_proj.weight";
            Linear(attnInput, weight[mlp_w1_weight_name], Data(), a1);
            Linear(attnInput, weight[mlp_w2_weight_name], Data(), a2);
            Silu(a2, a2);
            MulTo(a1, a2);
            Linear(a1, weight[mlp_proj_weight_name], Data(), mlpOutput);
            AddTo(hiddenStates, mlpOutput);
        }

        RMSNorm(hiddenStates, weight["transformer.ln_f.weight"], 1e-6, hiddenStates);
        Data logits;
        Linear(hiddenStates, weight["lm_head.weight"], Data(), logits);

        std::vector <int> lastRet;
        int total = 0;
        Data curLogit;
        for (int b = 0; b < batch; b++) {
            Split(logits, 1, total + seqLens[b] - 1, total + seqLens[b], curLogit);
            if (generationConfigs[b].output_logits && retLogits != nullptr && (*retLogits)[b] != nullptr) {
                curLogit.ToDevice(DataDevice::CPU);
                (*retLogits)[b]->resize(curLogit.Count(0));
                memcpy((float*)(*retLogits)[b]->data(), (float*)curLogit.cpuData, curLogit.GetBytes());
            }
            if (generationConfigs[b].IsSimpleGreedy()) {
                Data topk;
                TopK(curLogit, topk, 1);
                topk.ToDevice(DataDevice::CPU);
                lastRet.push_back((int) (((float *) topk.cpuData)[0] + 1e-3));
            } else {
                lastRet.push_back(LLMSampling(curLogit, 0, generationConfigs[b], lastTokens.units[b]));
            }
            total += seqLens[b];
        }
        return lastRet;
    }

    std::string QWenModel::MakeInput(const std::string &history, int round, const std::string &input) {
        std::string res;
        if (weight.dicts["chat_format"] == "chatml") {
            res = (round == 0 ? im_start + "system" + "\n" + pre_prompt + im_end : history) + 
                "\n" + im_start + user_role + "\n" + input + im_end + "\n" + im_start + bot_role + "\n";
        } else if (weight.dicts["chat_format"] == "raw") {
            res = history + input;
        } else {
            ErrorInFastLLM("Unknown char_format for QWen: " + weight.dicts["chat_format"]);
        }
        return res;
    }

    std::string QWenModel::MakeHistory(const std::string &history, int round, 
                                       const std::string &input, const std::string &output) {
        std::string res;
        if (weight.dicts["chat_format"] == "chatml") {
            res = (round == 0 ? im_start + "system" + "\n" + pre_prompt + im_end : history) + 
                "\n" + im_start + user_role + "\n" + input + im_end + "\n" + im_start + bot_role + "\n" + output + im_end;
        } else if (weight.dicts["chat_format"] == "raw") {
            res = history + input + output;
        } else {
            ErrorInFastLLM("Unknown char_format for QWen: " + weight.dicts["chat_format"]);
        }
        return res;
    }

    std::string QWenModel::MakeInputVL(const std::string &history, int round, const visualDictData &input) {
        return MakeInput(history, round, visual->FromListFormat(input));
    }

    std::string QWenModel::MakeHistoryVL(const std::string &history, int round, 
                                         const visualDictData &input, const std::string &output) {
        return MakeHistory(history, round, visual->FromListFormat(input), output);
    }

    void QWenModel::FillLLMInputs(std::vector <std::vector <float> > &inputTokens,
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

    void QWenModel::FillLLMInputsBatch(std::vector <std::vector <float> > &inputTokens,
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

    void QWenModel::WarmUp() {
        printf("Warmup...\n");
        Data inputIds = Data(DataType::FLOAT32, {1, 1}, {1});
        Data attentionMask = Data(DataType::FLOAT32, {1, 1}, {0});
        Data positionIds = Data(DataType::FLOAT32, {1, 1}, {0, 0});

        std::vector <std::pair <Data, Data> > pastKeyValues;
        for (int i = 0; i < block_cnt; i++) {
            pastKeyValues.push_back(std::make_pair(Data(DataType::FLOAT32),
                                                   Data(DataType::FLOAT32)));
        }
        if (visual) {
            Data dummyImage = Data(DataType::FLOAT32, {1, 3, visual->imageSize, visual->imageSize});
            dummyImage.Allocate(0);
            visual->Forward(dummyImage, &dummyImage);
        }
        Forward(inputIds, attentionMask, positionIds, pastKeyValues);
#ifdef USE_TFACC40T
        FastllmTfaccReleaseTempMemory();
#endif
        printf("finish.\n");
    }

    void QWenModel::UpdateRotaryPosEmb(float ntk_alpha) {
        float base = 10000 * pow(ntk_alpha, (float) rotary_dim / (rotary_dim - 2));

        if (sin.empty() || cos.empty()) {
            sin.resize(max_positions);
            cos.resize(max_positions);
        }
        
        std::vector <float> invFreq;
        for (int i = 0; i < rotary_dim; i += 2) {
            invFreq.push_back(1.0 / pow(base, (float)i / rotary_dim));
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

        sinData.ToDevice(DataDevice::CPU);
        cosData.ToDevice(DataDevice::CPU);
        sinData.CopyFrom(Data(DataType::FLOAT32, {(int)this->sin.size(), (int)this->sin[0].size()}, fsin));
        cosData.CopyFrom(Data(DataType::FLOAT32, {(int)this->cos.size(), (int)this->cos[0].size()}, fcos));
    }
}