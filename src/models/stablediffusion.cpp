//
// Created by siemon on 9/5/23.
//

#include "utils.h"

#include "stablediffusion.h"

#include <cmath>

#include <chrono>

#include <algorithm>

#include <map>

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
    void SaveFile(Data &data, std::string name) {
        int len = data.Count(0);
        float *ptr = (float *) data.cpuData;
        FILE *outfile = fopen(name.c_str(), "w");
        for (int i = 0; i < len; i++) {
            fprintf(outfile, "%f\n", ptr[i]);
        }
        fclose(outfile);
    }

    DPMSolverMultistepScheduler::DPMSolverMultistepScheduler() {
        beta_schedule = "scaled_linear";
        num_train_steps = 1000;
        init_noise_sigma = 1.f;

        solver_order = 2;
        lower_order_nums = 0;

        beta_start = 0.00085;
        beta_end = 0.012;
    }

    void DPMSolverMultistepScheduler::Init() {
        if (beta_schedule == "linear") {
            Linspace(betas, beta_start, beta_end, num_train_steps);
        } else if (beta_schedule == "scaled_linear") {
            Linspace(betas, sqrtf(beta_start), sqrtf(beta_end), num_train_steps);
            Pow(betas, 2);
        } else {
            // error here
        }
        float *beta_ptr = (float *) betas.cpuData;

        alphas.Resize(betas.dims);
        alphas.Allocate();
        float *alpha_ptr = (float *) alphas.cpuData;
        for (int i = 0; i < alphas.Count(0); i++) {
            alpha_ptr[i] = 1 - beta_ptr[i];
        }
        CumProd(alphas, alphas_cumprod, 0);

        alpha_t.Resize(betas.dims);
        alpha_t.Allocate();
        float *alpha_t_ptr = (float *) alpha_t.cpuData;
        float *alphas_cumprod_ptr = (float *) alphas_cumprod.cpuData;
        for (int i = 0; i < alpha_t.Count(0); i++) {
            alpha_t_ptr[i] = sqrtf(alphas_cumprod_ptr[i]);
        }

        sigma_t.Resize(betas.dims);
        sigma_t.Allocate();
        float *sigma_t_ptr = (float *) sigma_t.cpuData;
        for (int i = 0; i < sigma_t.Count(0); i++) {
            sigma_t_ptr[i] = sqrtf(1.f - alphas_cumprod_ptr[i]);
        }

        lambda_t.Resize(betas.dims);
        lambda_t.Allocate();
        float *lambda_t_ptr = (float *) lambda_t.cpuData;
        for (int i = 0; i < betas.Count(0); i++) {
            lambda_t_ptr[i] = logf(alpha_t_ptr[i]) - logf(sigma_t_ptr[i]);
        }

        for (int i = 0; i < solver_order; i++) {
            model_outputs.push_back(nullptr);
        }
    }

    void DPMSolverMultistepScheduler::SetTimeSteps(int num_inference_steps) {
        Data temp;
        Linspace(temp, 0, num_train_steps - 1, num_inference_steps + 1);
        float *timesteps_ptr = (float *) temp.cpuData;
        for (int i = 0; i < temp.Count(0); i++) {
            timesteps_ptr[i] = roundf(timesteps_ptr[i]);
        }
        timesteps.Resize({num_inference_steps});
        timesteps.Allocate();
        for (int i = 0; i < num_inference_steps; i++) {
            ((float *) timesteps.cpuData)[i] = timesteps_ptr[num_inference_steps - i];
        }
        Unique(timesteps);
        num_inference_steps = timesteps.Count(0);
    }

    void DPMSolverMultistepScheduler::Step(Data &modelOutput,
                                           Data &sample,
                                           Data &result,
                                           int timestep) {
        int step_index = -1, prev_timestep;
        float *timestepsData = (float *) timesteps.cpuData;
        for (int i = 0; i < timesteps.Count(0); i++) {
            if (timestep == (int) timestepsData[i]) {
                step_index = i;
                prev_timestep = (int) timestepsData[i + 1];
                break;
            }
        }
        if (step_index == -1) {
            step_index = timesteps.Count(0) - 1;
            prev_timestep = 0;
        }

        bool lower_order_final = step_index == (timesteps.Count(0) - 1) && timesteps.Count(0) < 15;
        bool lower_order_second = step_index == (timesteps.Count(0) - 2) && timesteps.Count(0) < 15;

        Data *x0_pred = new Data();
        float alpha_t = ((float *) this->alpha_t.cpuData)[timestep];
        float sigma_t = ((float *) this->sigma_t.cpuData)[timestep];
        Mul(sample, alpha_t, *x0_pred);
        AddTo(*x0_pred, modelOutput, -sigma_t);

        if (model_outputs[0]) {
            delete model_outputs[0];
        }
        for (int i = 0; i < solver_order - 1; i++) {
            model_outputs[i] = model_outputs[i + 1];
        }
        model_outputs[model_outputs.size() - 1] = x0_pred;

        if (solver_order == 1 || lower_order_nums < 1 || lower_order_final) {
            float lambda_t = ((float *) this->lambda_t.cpuData)[prev_timestep];
            float lambda_s = ((float *) this->lambda_t.cpuData)[timestep];
            float alpha_t = ((float *) this->alpha_t.cpuData)[prev_timestep];
            float alpha_s = ((float *) this->alpha_t.cpuData)[timestep];
            float sigma_t = ((float *) this->sigma_t.cpuData)[prev_timestep];
            float sigma_s = ((float *) this->sigma_t.cpuData)[timestep];
            float h = lambda_t - lambda_s;

            // printf("lambda_t %f\n", lambda_t);
            // printf("lambda_s %f\n", lambda_s);
            // printf("alpha_t %f\n", alpha_t);
            // printf("alpha_s %f\n", alpha_s);
            // printf("sigma_t %f\n", sigma_t);
            // printf("sigma_s %f\n", sigma_s);
            // printf("h %f\n", h);

            Mul(sample, (sigma_t / sigma_s), result);
            AddTo(result, *x0_pred, -(alpha_t * (expf(-h) - 1.f)));
        } else if (solver_order == 2 || lower_order_nums < 2 || lower_order_second) {
            int t = prev_timestep;
            int s0 = timestep;
            int s1 = (int) timestepsData[step_index - 1];
            float lambda_t = ((float *) this->lambda_t.cpuData)[t];
            float lambda_s0 = ((float *) this->lambda_t.cpuData)[s0];
            float lambda_s1 = ((float *) this->lambda_t.cpuData)[s1];
            float alpha_t = ((float *) this->alpha_t.cpuData)[t];
            float alpha_s0 = ((float *) this->alpha_t.cpuData)[s0];
            float sigma_t = ((float *) this->sigma_t.cpuData)[t];
            float sigma_s0 = ((float *) this->sigma_t.cpuData)[s0];
            float h = lambda_t - lambda_s0;
            float h_0 = lambda_s0 - lambda_s1;
            float r0 = h_0 / h;

            // printf("lambda_t %f\n", lambda_t);
            // printf("lambda_s0 %f\n", lambda_s0);
            // printf("lambda_s1 %f\n", lambda_s1);
            // printf("alpha_t %f\n", alpha_t);
            // printf("alpha_s0 %f\n", alpha_s0);
            // printf("sigma_t %f\n", sigma_t);
            // printf("sigma_s0 %f\n", sigma_s0);
            // printf("h %f\n", h);
            // printf("h_0 %f\n", h_0);
            // printf("r0 %f\n", r0);

            Data D0 = Data(*model_outputs[1]);
            Data D1 = Data();
            D1.Resize(model_outputs[1]->dims);
            D1.Allocate(0);
            AddTo(D1, *model_outputs[1]);
            AddTo(D1, *model_outputs[0], -1);
            Scaling(D1, 1.f / r0);

            Mul(sample, sigma_t / sigma_s0, result);
            AddTo(result, D0, -(alpha_t * (expf(-h) - 1.f)));
            AddTo(result, D1, -0.5 * (alpha_t * (expf(-h) - 1.f)));
        } else {
            // not implemented
        }

        if (lower_order_nums < solver_order) {
            lower_order_nums++;
        }
    }
    
    StableDiffusionModel::StableDiffusionModel() {
        prompt_len = 77;
        embed_dim = 1024;
        num_hidden_layers = 23;
        head_dim = 64;
        num_head = embed_dim / head_dim;

        num_inference_steps = 50;

        scale = powf((float) head_dim, -0.5f);

        vae_scale_factor = 8;   
        unet_sample_size = 96;
        channels = 4;
        height = vae_scale_factor * unet_sample_size;
        width = vae_scale_factor * unet_sample_size;

        unet_block_oc = {320, 640, 1280, 1280};
        unet_down_block_size = unet_block_oc.size();
        unet_up_block_size = unet_block_oc.size();
        unet_layers_per_block = 2;
        unet_down_attn_head_num = {5, 10, 20, 20};
        unet_mid_attn_head_num = {20};
        unet_up_attn_head_num = {20, 20, 10, 5};
        unet_down_sample_size = 1;
        unet_mid_layer_size = 1;
        unet_up_sample_size = 1;

        vae_sacling_factor = 0.18125f;
        vae_mid_layer_size = 1;
        vae_up_layer_size = 4;
        vae_layer_per_block = 3;
    }

    void StableDiffusionModel::SetImageSize(int size) {
        AssertInFastLLM(size % vae_scale_factor == 0, "Image size of StableDiffusion must align to vae_scale_factor.\n");
        unet_sample_size = size / vae_scale_factor;
        height = size;
        width = size;
    }

    void MakeCausalAttentionMask(const Data &inputIds, Data &mask) {
        int bsz = inputIds.dims[0];
        int tgt_len = inputIds.dims[1];
        std::vector<float> mask_value;
        for (int b = 0; b < bsz; b++) {
            for (int i = 0; i < tgt_len; i++) {
                for (int j = 0; j < tgt_len; j++) {
                    if (j > i) {
                        mask_value.push_back(1);
                    } else {
                        mask_value.push_back(0);
                    }
                }
            }
        }

        mask.CopyFrom(Data(DataType::FLOAT32, {bsz, tgt_len, tgt_len}, mask_value));
    }

    void StableDiffusionModel::TextEncoder(const Data &inputIds,
                                           const Data &positionIds,
                                           bool useAttentionMask,
                                           bool useCausalAttentionMask,
                                           Data &hiddenState) {
        int bsz = 1, seqlen = inputIds.dims[1];

        Data positionEmbds, attnInput, q, k, v;
        Data attnWeight, attnProbs, attnOutput, attnFinalOutput;
        Data mlpInput, mlpTemp, mlpOutput;

        Data attentionMask, causalAttentionMask;
        if (useAttentionMask) {
            // todo: create attention mask
        }
        if (useCausalAttentionMask) {
            MakeCausalAttentionMask(inputIds, causalAttentionMask);
        }

        std::string token_embedding_weight = "text_model.embeddings.token_embedding.weight";
        std::string position_embedding_weight = "text_model.embeddings.position_embedding.weight";
        Embedding(inputIds, weight[token_embedding_weight], hiddenState);
        Embedding(positionIds, weight[position_embedding_weight], positionEmbds);
        AddTo(hiddenState, positionEmbds);

        for (int i = 0; i < num_hidden_layers; i++) {
            ApplyDeviceMap(this->deviceMap, i + 1, num_hidden_layers);

            printf("%d\n", i);

            std::string layer_norm1_weight_name = "text_model.encoder.layers." + std::to_string(i) + ".layer_norm1.weight";
            std::string layer_norm1_bias_name = "text_model.encoder.layers." + std::to_string(i) + ".layer_norm1.bias";
            LayerNorm(hiddenState, weight[layer_norm1_weight_name], weight[layer_norm1_bias_name], -1, attnInput);

            std::string q_proj_weight_name = "text_model.encoder.layers." + std::to_string(i) + ".self_attn.q_proj.weight";
            std::string k_proj_weight_name = "text_model.encoder.layers." + std::to_string(i) + ".self_attn.k_proj.weight";
            std::string v_proj_weight_name = "text_model.encoder.layers." + std::to_string(i) + ".self_attn.v_proj.weight";
            std::string q_proj_bias_name = "text_model.encoder.layers." + std::to_string(i) + ".self_attn.q_proj.bias";
            std::string k_proj_bias_name = "text_model.encoder.layers." + std::to_string(i) + ".self_attn.k_proj.bias";
            std::string v_proj_bias_name = "text_model.encoder.layers." + std::to_string(i) + ".self_attn.v_proj.bias";
            Linear(attnInput, weight[q_proj_weight_name], weight[q_proj_bias_name], q);
            Linear(attnInput, weight[k_proj_weight_name], weight[k_proj_bias_name], k);
            Linear(attnInput, weight[v_proj_weight_name], weight[v_proj_bias_name], v);

            Scaling(q, scale);
            q.Reshape({bsz, seqlen, num_head, head_dim});
            k.Reshape({bsz, seqlen, num_head, head_dim});
            v.Reshape({bsz, seqlen, num_head, head_dim});
            PermuteSelf(q, {0, 2, 1, 3});
            PermuteSelf(k, {0, 2, 1, 3});
            PermuteSelf(v, {0, 2, 1, 3});

            std::vector<int> proj_shape = {bsz * num_head, -1, head_dim};
            q.Reshape(proj_shape);
            k.Reshape(proj_shape);
            v.Reshape(proj_shape);

            MatMulTransB(q, k, attnWeight);
            attnWeight.Reshape({bsz, num_head, seqlen, seqlen});
            if (useCausalAttentionMask) {
                AttentionMask(attnWeight, causalAttentionMask, -10000);
            }
            if (useAttentionMask) {
                AttentionMask(attnWeight, attentionMask, -10000);
            }
            attnWeight.Reshape({bsz * num_head, seqlen, seqlen});
            Softmax(attnWeight, attnProbs, -1);

            MatMul(attnProbs, v, attnOutput);
            attnOutput.Reshape({bsz, num_head, seqlen, head_dim});
            PermuteSelf(attnOutput, {0, 2, 1, 3});
            attnOutput.Reshape({bsz, seqlen, embed_dim});

            std::string out_proj_weight_name = "text_model.encoder.layers." + std::to_string(i) + ".self_attn.out_proj.weight";
            std::string out_proj_bias_name = "text_model.encoder.layers." + std::to_string(i) + ".self_attn.out_proj.bias";
            Linear(attnOutput, weight[out_proj_weight_name], weight[out_proj_bias_name], attnFinalOutput);

            AddTo(hiddenState, attnFinalOutput);

            std::string layer_norm2_weight_name = "text_model.encoder.layers." + std::to_string(i) + ".layer_norm2.weight";
            std::string layer_norm2_bias_name = "text_model.encoder.layers." + std::to_string(i) + ".layer_norm2.bias";
            LayerNorm(hiddenState, weight[layer_norm2_weight_name], weight[layer_norm2_bias_name], -1, mlpInput);

            std::string mlp_fc1_weight_name = "text_model.encoder.layers." + std::to_string(i) + ".mlp.fc1.weight";
            std::string mlp_fc1_bias_name = "text_model.encoder.layers." + std::to_string(i) + ".mlp.fc1.bias";
            std::string mlp_fc2_weight_name = "text_model.encoder.layers." + std::to_string(i) + ".mlp.fc2.weight";
            std::string mlp_fc2_bias_name = "text_model.encoder.layers." + std::to_string(i) + ".mlp.fc2.bias";

            Linear(mlpInput, weight[mlp_fc1_weight_name], weight[mlp_fc1_bias_name], mlpTemp);
            GeluNew(mlpTemp, mlpTemp);
            Linear(mlpTemp, weight[mlp_fc2_weight_name], weight[mlp_fc2_bias_name], mlpOutput);
            AddTo(hiddenState, mlpOutput);
        }

        std::string final_layer_norm_weight_name = "text_model.final_layer_norm.weight";
        std::string final_layer_norm_bias_name = "text_model.final_layer_norm.bias";
        LayerNorm(hiddenState, weight[final_layer_norm_weight_name], weight[final_layer_norm_bias_name], -1, hiddenState);

        hiddenState.Reshape({bsz, seqlen, -1});
    }

    void StableDiffusionModel::PrepareLatents(int batch,
                                              int channels,
                                              int height,
                                              int width,
                                              Data &latent) {
        latent.Resize({batch, channels, height / vae_scale_factor, width / vae_scale_factor});
        latent.Allocate();

        Randn(latent);
        if (scheduler.init_noise_sigma != 1.f) {
            Scaling(latent, scheduler.init_noise_sigma);
        }
    }

    void StableDiffusionModel::ResBlock(Data &hiddenStates,
                                        Data &emb,
                                        Data &result,
                                        std::string pre) {
        // copy hidden states
        result.Resize(hiddenStates.dims);
        result.Allocate();

        Data norm1, conv1, norm2, conv2, temb, tproj;
        std::string norm1_gamma_weight_name = pre + "norm1.weight";
        std::string norm1_beta_weight_name = pre + "norm1.bias";
        std::string norm2_gamma_weight_name = pre + "norm2.weight";
        std::string norm2_beta_weight_name = pre + "norm2.bias";
        std::string down_conv1_weight_name = pre + "conv1.weight";
        std::string down_conv1_bias_name = pre + "conv1.bias";
        std::string time_emb_proj_weight_name = pre + "time_emb_proj.weight";
        std::string time_emb_proj_bias_name = pre + "time_emb_proj.bias";
        std::string down_conv2_weight_name = pre + "conv2.weight";
        std::string down_conv2_bias_name = pre + "conv2.bias";
        std::string conv_short_cut_weight_name = pre + "conv_shortcut.weight";
        std::string conv_short_cut_bias_name = pre + "conv_shortcut.bias";

        if (weight.weight.find(conv_short_cut_weight_name) != weight.weight.end()) {
            Conv2d(hiddenStates, weight[conv_short_cut_weight_name], weight[conv_short_cut_bias_name], result);
        } else {
            memcpy(result.cpuData, hiddenStates.cpuData, hiddenStates.Count(0) * hiddenStates.unitSize);
        }

        GroupNorm(hiddenStates, weight[norm1_gamma_weight_name], weight[norm1_beta_weight_name], 32, norm1);
        Silu(norm1, norm1);
        Conv2d(norm1, weight[down_conv1_weight_name], weight[down_conv1_bias_name], conv1, 1, 1);
        if (!emb.dims.empty()) {
            Silu(emb, temb);
            Linear(temb, weight[time_emb_proj_weight_name], weight[time_emb_proj_bias_name], tproj);
            AddTo(conv1, tproj);
        }
        GroupNorm(conv1, weight[norm2_gamma_weight_name], weight[norm2_beta_weight_name], 32, norm2);
        Silu(norm2, norm2);
        Conv2d(norm2, weight[down_conv2_weight_name], weight[down_conv2_bias_name], conv2, 1, 1);
        AddTo(result, conv2);
    }

    void StableDiffusionModel::Transformer(Data &hiddenStates,
                                           Data &encoderHiddenStates,
                                           Data &result,
                                           std::string pre,
                                           int headNum) {
        // copy hidden states
        result.Resize(hiddenStates.dims);
        result.Allocate();
        memcpy(result.cpuData, hiddenStates.cpuData, hiddenStates.Count(0) * hiddenStates.unitSize);

        Data projin, projout, ffoutput;
        Data norm, query, key, value, attnWeight, attnOut, attnFinalOut;
        int batch, inner_dim, height, width;
        batch = hiddenStates.dims[0];
        inner_dim = hiddenStates.dims[1];
        height = hiddenStates.dims[2];
        width = hiddenStates.dims[3];
        std::string norm_gamma_name = pre + "norm.weight";
        std::string norm_beta_name = pre + "norm.bias";
        std::string proj_in_wight_name = pre + "proj_in.weight";
        std::string proj_in_bias_name = pre + "proj_in.bias";
        std::string layernorm1_gamma_name = pre + "transformer_blocks.0.norm1.weight";
        std::string layernorm1_beta_name = pre + "transformer_blocks.0.norm1.bias";
        std::string layernorm2_gamma_name = pre + "transformer_blocks.0.norm2.weight";
        std::string layernorm2_beta_name = pre + "transformer_blocks.0.norm2.bias";
        std::string layernorm3_gamma_name = pre + "transformer_blocks.0.norm3.weight";
        std::string layernorm3_beta_name = pre + "transformer_blocks.0.norm3.bias";

        GroupNorm(hiddenStates, weight[norm_gamma_name], weight[norm_beta_name], 32, norm);
        PermuteSelf(norm, {0, 2, 3, 1});
        norm.Reshape({batch, height * width, inner_dim});
        Linear(norm, weight[proj_in_wight_name], weight[proj_in_bias_name], projin);
        
        // attn1        
        std::string attn1_to_q_weight_name = pre + "transformer_blocks.0.attn1.to_q.weight";
        std::string attn1_to_k_weight_name = pre + "transformer_blocks.0.attn1.to_k.weight";
        std::string attn1_to_v_weight_name = pre + "transformer_blocks.0.attn1.to_v.weight";
        std::string attn1_to_out_weight_name = pre + "transformer_blocks.0.attn1.to_out.0.weight";
        std::string attn1_to_out_bias_name = pre + "transformer_blocks.0.attn1.to_out.0.bias";
        LayerNorm(projin, weight[layernorm1_gamma_name], weight[layernorm1_beta_name], -1, norm);
        Linear(norm, weight[attn1_to_q_weight_name], Data(), query);
        Linear(norm, weight[attn1_to_k_weight_name], Data(), key);
        Linear(norm, weight[attn1_to_v_weight_name], Data(), value);
        query.Reshape({batch, -1, headNum, inner_dim / headNum});
        key.Reshape({batch, -1, headNum, inner_dim / headNum});
        value.Reshape({batch, -1, headNum, inner_dim / headNum});
        PermuteSelf(query, {0, 2, 1, 3});
        PermuteSelf(key, {0, 2, 1, 3});
        PermuteSelf(value, {0, 2, 1, 3});
        MatMulTransB(query, key, attnWeight, 1 / sqrtf(query.dims.back()));
        Softmax(attnWeight, attnWeight, -1);
        MatMul(attnWeight, value, attnOut);
        PermuteSelf(attnOut, {0, 2, 1, 3});
        attnOut.Reshape({batch, -1, inner_dim});
        Linear(attnOut, weight[attn1_to_out_weight_name], weight[attn1_to_out_bias_name], attnFinalOut);
        AddTo(projin, attnFinalOut);

        // attn2
        std::string attn2_to_q_weight_name = pre + "transformer_blocks.0.attn2.to_q.weight";
        std::string attn2_to_k_weight_name = pre + "transformer_blocks.0.attn2.to_k.weight";
        std::string attn2_to_v_weight_name = pre + "transformer_blocks.0.attn2.to_v.weight";
        std::string attn2_to_out_weight_name = pre + "transformer_blocks.0.attn2.to_out.0.weight";
        std::string attn2_to_out_bias_name = pre + "transformer_blocks.0.attn2.to_out.0.bias";
        LayerNorm(projin, weight[layernorm2_gamma_name], weight[layernorm2_beta_name], -1, norm);
        Linear(norm, weight[attn2_to_q_weight_name], Data(), query);
        Linear(encoderHiddenStates, weight[attn2_to_k_weight_name], Data(), key);
        Linear(encoderHiddenStates, weight[attn2_to_v_weight_name], Data(), value);
        query.Reshape({batch, -1, headNum, inner_dim / headNum});
        key.Reshape({batch, -1, headNum, inner_dim / headNum});
        value.Reshape({batch, -1, headNum, inner_dim / headNum});
        PermuteSelf(query, {0, 2, 1, 3});
        PermuteSelf(key, {0, 2, 1, 3});
        PermuteSelf(value, {0, 2, 1, 3});
        MatMulTransB(query, key, attnWeight, 1 / sqrtf(query.dims.back()));
        Softmax(attnWeight, attnWeight, -1);
        MatMul(attnWeight, value, attnOut);
        PermuteSelf(attnOut, {0, 2, 1, 3});
        attnOut.Reshape({batch, -1, inner_dim});
        Linear(attnOut, weight[attn2_to_out_weight_name], weight[attn2_to_out_bias_name], attnFinalOut);
        AddTo(projin, attnFinalOut);

        // feed forward
        Data gate0, gate1;
        std::string ff_geglu_weight_name = pre + "transformer_blocks.0.ff.net.0.proj.weight";
        std::string ff_geglu_bias_name = pre + "transformer_blocks.0.ff.net.0.proj.bias";
        std::string ff_weight_name = pre + "transformer_blocks.0.ff.net.2.weight";
        std::string ff_bias_name = pre + "transformer_blocks.0.ff.net.2.bias";
        LayerNorm(projin, weight[layernorm3_gamma_name], weight[layernorm3_beta_name], -1, norm);
        Linear(norm, weight[ff_geglu_weight_name], weight[ff_geglu_bias_name], ffoutput);
        Split(ffoutput, -1, 0, ffoutput.dims.back() / 2, gate0);
        Split(ffoutput, -1, ffoutput.dims.back() / 2, ffoutput.dims.back(), gate1);
        GeluNew(gate1, gate1);
        MulTo(gate0, gate1);
        Linear(gate0, weight[ff_weight_name], weight[ff_bias_name], ffoutput);
        AddTo(projin, ffoutput);

        // annt output
        std::string proj_out_weight_name = pre + "proj_out.weight";
        std::string proj_out_bias_name = pre + "proj_out.bias";
        Linear(projin, weight[proj_out_weight_name], weight[proj_out_bias_name], projout);
        projout.Reshape({batch, height, width, inner_dim});
        PermuteSelf(projout, {0, 3, 1, 2});
        
        AddTo(result, projout);
    }

    void StableDiffusionModel::Unet(Data &sample,
                                    Data &encoderHiddenStates,
                                    Data &result,
                                    int timestep,
                                    bool flipSinToCos,
                                    int round) {
        // 1. time
        int embedding_dim = unet_block_oc[0];
        Data exponent;
        exponent.Resize({embedding_dim / 2});
        exponent.Allocate();
        for (int i = 0; i < embedding_dim / 2; i++) {
            ((float *) exponent.cpuData)[i] = expf(i * (-logf(10000)) / (embedding_dim / 2));
        }

        Data emb;
        emb.Resize({sample.dims[0], embedding_dim});
        emb.Allocate();
        float *emb_ptr = (float *) emb.cpuData;
        float *exponent_ptr = (float *) exponent.cpuData;
        if (flipSinToCos) {
            for (int i = 0; i < sample.dims[0]; i++) {
                for (int j = 0; j < exponent.dims[0]; j++) {
                    emb_ptr[i * embedding_dim + j] = cosf(timestep * exponent_ptr[j]);
                    emb_ptr[i * embedding_dim + j + exponent.dims[0]] = sinf(timestep * exponent_ptr[j]);
                }
            }
        } else {
            for (int i = 0; i < sample.dims[0]; i++) {
                for (int j = 0; j < exponent.dims[0]; j++) {
                    emb_ptr[i * embedding_dim + j] = sinf(timestep * exponent_ptr[j]);
                    emb_ptr[i * embedding_dim + j + exponent.dims[0]] = cosf(timestep * exponent_ptr[j]);
                }
            }
        }
        
        Data emb_tmp;
        std::string time_embd_linear1_weight_name = "time_embedding.linear_1.weight";
        std::string time_embd_linear1_bias_name = "time_embedding.linear_1.bias";
        std::string time_embd_linear2_weight_name = "time_embedding.linear_2.weight";
        std::string time_embd_linear2_bias_name = "time_embedding.linear_2.bias";
        Linear(emb, weight[time_embd_linear1_weight_name], weight[time_embd_linear1_bias_name], emb_tmp);
        Silu(emb_tmp, emb_tmp);
        Linear(emb_tmp, weight[time_embd_linear2_weight_name], weight[time_embd_linear2_bias_name], emb);

        // 2. pre-process
        Data *hiddenStates = new Data();
        Data *temp = new Data();
        std::string conv_in_weight_name = "conv_in.weight";
        std::string conv_in_bias_name = "conv_in.bias";
        Conv2d(sample, weight[conv_in_weight_name], weight[conv_in_bias_name], *hiddenStates, 1, 1);

        // 3. down
        std::vector<Data *> downBlockResSamples;
        Data *oneSample = new Data(*hiddenStates);
        downBlockResSamples.push_back(oneSample);
        printf("Down\n");
        for (int i = 0; i < unet_down_block_size - 1; i++) {
            // CrossAttnDownBlock2D
            for (int j = 0; j < unet_layers_per_block; j++) {
                // resnet
                printf("resnet[%d/%d]\n", j + 1, unet_layers_per_block);
                ResBlock(*hiddenStates, emb, *temp, "down_blocks." + std::to_string(i) + ".resnets." + std::to_string(j) + ".");
                
                // attention
                printf("transformer[%d/%d]\n", j + 1, unet_layers_per_block);
                Transformer(*temp, encoderHiddenStates, *hiddenStates, "down_blocks." + std::to_string(i) + ".attentions." + std::to_string(j) + ".", unet_down_attn_head_num[i]);
                
                Data *oneSample = new Data(*hiddenStates);
                downBlockResSamples.push_back(oneSample);
            }

            // down sampler
            for (int j = 0; j < unet_down_sample_size; j++) {
                std::string down_sampler_weight_name = "down_blocks." + std::to_string(i) + ".downsamplers." + std::to_string(j) +".conv.weight";
                std::string down_sampler_bias_name = "down_blocks." + std::to_string(i) + ".downsamplers." + std::to_string(j) +".conv.bias";
                Conv2d(*hiddenStates, weight[down_sampler_weight_name], weight[down_sampler_bias_name], *temp, 2, 1);
                std::swap(hiddenStates, temp);

                Data *oneSample = new Data(*hiddenStates);
                downBlockResSamples.push_back(oneSample);
            }
        }
        // DownBlock2D
        for (int j = 0; j < unet_layers_per_block; j++) {
            ResBlock(*hiddenStates, emb, *temp, "down_blocks." + std::to_string(unet_down_block_size - 1) + ".resnets." + std::to_string(j) + ".");
            std::swap(hiddenStates, temp);

            Data *oneSample = new Data(*hiddenStates);
            downBlockResSamples.push_back(oneSample);
        }
            
        // 4. mid
        // resnet
        printf("Mid\n");
        ResBlock(*hiddenStates, emb, *temp, "mid_block.resnets.0.");
        std::swap(hiddenStates, temp);
        for (int j = 0; j < unet_mid_layer_size; j++) {
            // attn
            Transformer(*hiddenStates, encoderHiddenStates, *temp, "mid_block.attentions." + std::to_string(j) + ".", unet_mid_attn_head_num[0]);
            // resnet
            ResBlock(*temp, emb, *hiddenStates, "mid_block.resnets." + std::to_string(j + 1) + ".");
        }

        // 5. up
        printf("Up\n");
        int res_sample_size = downBlockResSamples.size() / unet_up_block_size;
        // UpBlock2D
        std::vector<Data *> resSample;
        resSample = std::vector<Data *>(downBlockResSamples.end() - res_sample_size, downBlockResSamples.end());
        for (int j = 0; j < res_sample_size; j++) {
            Data *resHiddenStates = resSample.back();
            resSample.pop_back();
            Cat(*hiddenStates, *resHiddenStates, 1, *temp);
            ResBlock(*temp, emb, *hiddenStates, "up_blocks.0.resnets." + std::to_string(j) + ".");

            if (j == res_sample_size - 1) {
                Data resized;
                std::string up_sampler_weight_name = "up_blocks.0.upsamplers.0.conv.weight";
                std::string up_sampler_bias_name = "up_blocks.0.upsamplers.0.conv.bias";

                Interpolate(*hiddenStates, resized, 2, 1, 0);
                Conv2d(resized, weight[up_sampler_weight_name], weight[up_sampler_bias_name], *hiddenStates, 1, 1);
            }
        }
        // clear res samples
        resSample.clear();
        for (int j = 0; j < res_sample_size; j++) {
            delete downBlockResSamples.back();
            downBlockResSamples.pop_back();
        }

        for (int i = 1; i < unet_up_block_size; i++) {
            // CrossAttnUpBlock2D
            resSample = std::vector<Data *>(downBlockResSamples.end() - res_sample_size, downBlockResSamples.end());
            for (int j = 0; j < res_sample_size; j++) {
                Data *resHiddenStates = resSample.back();
                resSample.pop_back();
                Cat(*hiddenStates, *resHiddenStates, 1, *temp);
                std::swap(hiddenStates, temp);
                // resnet
                printf("resnet[%d/%d]\n", j + 1, res_sample_size);
                ResBlock(*hiddenStates, emb, *temp, "up_blocks." + std::to_string(i) + ".resnets." + std::to_string(j) + ".");
                // attn
                printf("transformer[%d/%d]\n", j + 1, res_sample_size);
                Transformer(*temp, encoderHiddenStates, *hiddenStates, "up_blocks." + std::to_string(i) + ".attentions." + std::to_string(j) + ".", unet_up_attn_head_num[i]);
            }

            // upsampler
            if (i < unet_up_block_size - 1) {
                for (int j = 0; j < unet_up_sample_size; j++) {
                    Data resized;
                    std::string up_sampler_weight_name = "up_blocks." + std::to_string(i) + ".upsamplers." + std::to_string(j) + ".conv.weight";
                    std::string up_sampler_bias_name = "up_blocks." + std::to_string(i) + ".upsamplers." + std::to_string(j) + ".conv.bias";

                    Interpolate(*hiddenStates, resized, 2, 1, 0);
                    Conv2d(resized, weight[up_sampler_weight_name], weight[up_sampler_bias_name], *hiddenStates, 1, 1);
                }
            }
            // clear res samples
            resSample.clear();
            for (int j = 0; j < res_sample_size; j++) {
                delete downBlockResSamples.back();
                downBlockResSamples.pop_back();
            }
        }

        // 6. post-process
        Data norm;
        std::string conv_norm_out_gamma_name = "conv_norm_out.weight";
        std::string conv_norm_out_beta_name = "conv_norm_out.bias";
        std::string conv_out_weight_name = "conv_out.weight";
        std::string conv_out_bias_name = "conv_out.bias";
        GroupNorm(*hiddenStates, weight[conv_norm_out_gamma_name], weight[conv_norm_out_beta_name], 32, norm);
        Silu(norm, norm);
        Conv2d(norm, weight[conv_out_weight_name], weight[conv_out_bias_name], result, 1, 1);

        delete hiddenStates;
        delete temp;
    }

    void StableDiffusionModel::AttentionBlock(Data &hiddenStates,
                                              Data &result,
                                              std::string pre) {
        Data norm, query, key, value;
        Data attnWeight, attnOut;

        int batch, channel, height, width;
        batch = hiddenStates.dims[0];
        channel = hiddenStates.dims[1];
        height = hiddenStates.dims[2];
        width = hiddenStates.dims[3];
        std::string group_norm_gamma_name = pre + "group_norm.weight";
        std::string group_norm_beta_name = pre + "group_norm.bias";
        std::string query_proj_weight_name = pre + "query.weight";
        std::string query_proj_bias_name = pre + "query.bias";
        std::string key_proj_weight_name = pre + "key.weight";
        std::string key_proj_bias_name = pre + "key.bias";
        std::string value_proj_weight_name = pre + "value.weight";
        std::string value_proj_bias_name = pre + "value.bias";
        std::string proj_attn_weight_name = pre + "proj_attn.weight";
        std::string proj_attn_bias_name = pre + "proj_attn.bias";

        GroupNorm(hiddenStates, weight[group_norm_gamma_name], weight[group_norm_beta_name], 32, norm);
        norm.Reshape({batch, channel, height * width});
        PermuteSelf(norm, {0, 2, 1});

        Linear(norm, weight[query_proj_weight_name], weight[query_proj_bias_name], query);
        Linear(norm, weight[key_proj_weight_name], weight[key_proj_bias_name], key);
        Linear(norm, weight[value_proj_weight_name], weight[value_proj_bias_name], value);

        int seqlen, dim;
        seqlen = query.dims[1];
        dim = query.dims[2];

        query.Reshape({batch, seqlen, 1, dim});
        key.Reshape({batch, seqlen, 1, dim});
        value.Reshape({batch, seqlen, 1, dim});
        PermuteSelf(query, {0, 2, 1, 3});
        PermuteSelf(key, {0, 2, 1, 3});
        PermuteSelf(value, {0, 2, 1, 3});

        MatMulTransB(query, key, attnWeight, 1 / sqrtf(query.dims.back()));
        Softmax(attnWeight, attnWeight, -1);
        MatMul(attnWeight, value, attnOut);
        PermuteSelf(attnOut, {0, 2, 3, 1});
        attnOut.Reshape({batch, seqlen, dim});
        Linear(attnOut, weight[proj_attn_weight_name], weight[proj_attn_bias_name], result);
        PermuteSelf(result, {0, 2, 1});
        result.Reshape({batch, channel, height, width});
        AddTo(result, hiddenStates);
    }

    void StableDiffusionModel::DecodeLatent(Data &latent, Data &image) {
        Data z, sample;
        std::string post_quant_conv_weight_name = "post_quant_conv.weight";
        std::string post_quant_conv_bias_name = "post_quant_conv.bias";

        Scaling(latent, 1 / vae_sacling_factor);
        Conv2d(latent, weight[post_quant_conv_weight_name], weight[post_quant_conv_bias_name], z);

        // vae decoder
        std::string decoder_conv_in_weight_name = "decoder.conv_in.weight";
        std::string decoder_conv_in_bias_name = "decoder.conv_in.bias";
        Conv2d(z, weight[decoder_conv_in_weight_name], weight[decoder_conv_in_bias_name], sample, 1, 1);
        
        // middle
        Data empty_emb;
        Data *dec = new Data();
        Data *temp = new Data();
        ResBlock(sample, empty_emb, *dec, "decoder.mid_block.resnets.0.");
        for (int i = 0; i < vae_mid_layer_size; i++) {
            // attn
            printf("attention[%d/%d]\n", i + 1, vae_mid_layer_size);
            AttentionBlock(*dec, *temp, "decoder.mid_block.attentions." + std::to_string(i) + ".");
            // resnet
            printf("resnet[%d/%d]\n", i + 1, vae_mid_layer_size);
            ResBlock(*temp, empty_emb, *dec, "decoder.mid_block.resnets." + std::to_string(i + 1) + ".");
        }

        // up
        for (int i = 0; i < vae_up_layer_size; i++) {
            // resnet
            printf("vae up[%d/%d]\n", i + 1, vae_up_layer_size);
            for (int j = 0; j < vae_layer_per_block; j++) {
                printf("resnet[%d/%d]\n", j + 1, vae_layer_per_block);
                ResBlock(*dec, empty_emb, *temp, "decoder.up_blocks." + std::to_string(i) + ".resnets." + std::to_string(j) + ".");
                std::swap(dec, temp);
            }

            if (i != vae_up_layer_size - 1) {
                // upsample2d
                Data resized;
                std::string vae_up_sampler_weight_name = "decoder.up_blocks." + std::to_string(i) + ".upsamplers.0.conv.weight";
                std::string vae_up_sampler_bias_name = "decoder.up_blocks." + std::to_string(i) + ".upsamplers.0.conv.bias";
                Interpolate(*dec, resized, 2, 1, 0);
                Conv2d(resized, weight[vae_up_sampler_weight_name], weight[vae_up_sampler_bias_name], *dec, 1, 1);
            }
        }

        // post-process
        Data norm;
        std::string vae_conv_nrom_out_gamma_name = "decoder.conv_norm_out.weight";
        std::string vae_conv_nrom_out_beta_name = "decoder.conv_norm_out.bias";
        std::string vae_conv_out_weight_name = "decoder.conv_out.weight";
        std::string vae_conv_out_bias_name = "decoder.conv_out.bias";
        GroupNorm(*dec, weight[vae_conv_nrom_out_gamma_name], weight[vae_conv_nrom_out_beta_name], 32, norm);
        Silu(norm, norm);
        Conv2d(norm, weight[vae_conv_out_weight_name], weight[vae_conv_out_bias_name], image, 1, 1);
        PermuteSelf(image, {0, 2, 3, 1});
        
        float *imageData = (float *) image.cpuData;
        for (int i = 0; i < image.Count(0); i++) {
            imageData[i] = std::max(0.f, std::min(1.f, imageData[i] / 2.f + 0.5f));
        }

        delete dec;
        delete temp;
    }

    void StableDiffusionModel::Forward(const Data &inputIds,
                                       const Data &uncondToken,
                                       const Data &positionIds,
                                       Data &image,
                                       float guidanceScale) {
        bool doClassifierFreeGuidance = guidanceScale > 1.0f;
        // Encode input prompt
        Data textPromptEmbeds, negativePromptEmbeds, embeds;
        TextEncoder(inputIds, positionIds, false, true, textPromptEmbeds);
        if (doClassifierFreeGuidance) {
            TextEncoder(uncondToken, positionIds, false, true, negativePromptEmbeds);
            Cat(negativePromptEmbeds, textPromptEmbeds, 0, embeds);
        } else {
            embeds = textPromptEmbeds;
        }
        
        scheduler.SetTimeSteps(num_inference_steps);
        Data timesteps = scheduler.timesteps;

        // Prepare latent variables
        Data *latent = new Data();
        Data *latentModelInput = new Data();
        PrepareLatents(1, channels, height, width, *latent);

        // Denoising loop
        int num_warmup_steps = timesteps.Count(0) - num_inference_steps;
        for (int i = 0; i < timesteps.Count(0); i++) {
            printf("Loop [%d/%d]\n", i + 1, timesteps.Count(0));
            int t = (int) ((float *) timesteps.cpuData)[i];

            if (doClassifierFreeGuidance) {
                Cat(*latent, *latent, 0, *latentModelInput);
            } else {
                latentModelInput->CopyFrom(*latent);
            }

            // unet
            Data noisePred;
            Unet(*latentModelInput, embeds, noisePred, t, true, i);

            // preform guidance
            if (doClassifierFreeGuidance) {
                Data noisePredUncond, noisePredText;
                Split(noisePred, 0, 0, 1, noisePredUncond);
                Split(noisePred, 0, 1, 2, noisePredText);

                noisePred.Resize(noisePredText.dims);
                noisePred.Allocate();
                float *noisePredData = (float *) noisePred.cpuData;
                float *noisePredUncondData = (float *) noisePredUncond.cpuData;
                float *noisePredTextData = (float *) noisePredText.cpuData;
                for (int i = 0; i < noisePred.Count(0); i++) {
                    noisePredData[i] = noisePredUncondData[i] + guidanceScale * (noisePredTextData[i] - noisePredUncondData[i]);
                }
            }
            scheduler.Step(noisePred, *latent, *latentModelInput, t);
            std::swap(latent, latentModelInput);
        }

        DecodeLatent(*latent, image);
        delete latent, latentModelInput;
    }

    void StableDiffusionModel::Pipeline(const std::string &positivePrompt,
                                        const std::string &negativePrompt,
                                        const int imageSize,
                                        const int inferenceRound,
                                        const float guidanceScale,
                                        Data &image) {
        // reset image szie    
        SetImageSize(imageSize);
        
        // tokenize
        Data positiveToken = this->weight.tokenizer.Encode(MakeInput(positivePrompt));
        Data negativeToken = this->weight.tokenizer.Encode(MakeInput(negativePrompt));
        Data positionIds = Data();

        // padding to prompt_len
        std::vector<float> positiveData;
        std::vector<float> negativeData;
        std::vector<float> positionData;
        for (int i = 0; i < prompt_len; i++) {
            positiveData.push_back(i < positiveToken.Count(0) ? ((float *) positiveToken.cpuData)[i] : 0.f);
            negativeData.push_back(i < negativeToken.Count(0) ? ((float *) negativeToken.cpuData)[i] : 0.f);
            positionData.push_back((float) i);
        }
        positiveToken.CopyFrom(Data(DataType::FLOAT32, {1, prompt_len}, positiveData));
        negativeToken.CopyFrom(Data(DataType::FLOAT32, {1, prompt_len}, negativeData));
        positionIds.CopyFrom(Data(DataType::FLOAT32, {1, prompt_len}, positionData));

        // todo: set imageSize
        num_inference_steps = inferenceRound;
        Forward(positiveToken, negativeToken, positionIds, image, guidanceScale);
    }

    std::string StableDiffusionModel::MakeInput(const std::string &input) {
        return textstart + input + textend;
    }
}