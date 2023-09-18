//
// Created by siemon on 9/5/23.
//

#ifndef STABLEDIFFUSION_H
#define STABLEDIFFUSION_H

#include "basellm.h"

namespace fastllm {
    struct DPMSolverMultistepScheduler {
        DPMSolverMultistepScheduler();

        void Init();

        void SetTimeSteps(
            int num_inference_steps);

        void Step(
            Data &modelOutput,
            Data &sample,
            Data &result,
            int timestep);

        int num_inference_steps;
        int num_train_steps;
        Data timesteps;

        std::string beta_schedule;
        float beta_start;
        float beta_end;
        float init_noise_sigma;

        Data betas;
        
        Data alphas;
        Data alphas_cumprod;
        Data alpha_t;

        Data sigma_t;
        Data lambda_t;

        int solver_order;
        int lower_order_nums;
        std::vector<Data *> model_outputs;
    };

    class StableDiffusionModel {
    public:
        StableDiffusionModel();

        SetImageSize(int size);

        void TextEncoder(
            const Data &inputIds,
            const Data &positionIds,
            bool useAttentionMask,
            bool useCausalAttentionMask,
            Data &hiddenState);
        
        void PrepareLatents(
            int batch,
            int channels,
            int height,
            int width,
            Data &latent);

        void ResBlock(
            Data &hiddenStates,
            Data &emb,
            Data &result,
            std::string pre);

        void Transformer(
            Data &hiddenStates,
            Data &encoderHiddenStates,
            Data &result,
            std::string pre,
            int headNum);
        
        void Unet(
            Data &sample,
            Data &encoderHiddenStates,
            Data &result,
            int timestep,
            bool flipSinToCos,
            int round);
            
        void AttentionBlock(
            Data &hiddenStates,
            Data &result,
            std::string pre);

        void DecodeLatent(
            Data &latent,
            Data &image);

        // 推理
		void Forward(
            const Data &inputIds,
            const Data &uncondToken,
            const Data &positionIds,
            Data &image,
            float guidanceScale);
        
        void Pipeline(
            const std::string &positivePrompt,
            const std::string &negativePrompt,
            const int imageSize,
            const int inferenceRound,
            const float guidanceScale,
            Data &image);

        std::string MakeInput(const std::string &input);
        
        int prompt_len;
        int embed_dim;
        int num_hidden_layers;
        int head_dim;
        int num_head;

        int num_inference_steps;

        float scale;
        int channels, height, width;
        int vae_scale_factor;

        int unet_saple_size;
        std::vector<int> unet_block_oc;
        int unet_down_block_size;
        int unet_up_block_size;
        int unet_layers_per_block;
        std::vector<int> unet_down_attn_head_num;
        std::vector<int> unet_mid_attn_head_num;
        std::vector<int> unet_up_attn_head_num;
        int unet_down_sample_size;
        int unet_mid_layer_size;
        int unet_up_sample_size;

        float vae_sacling_factor;
        int vae_mid_layer_size;
        int vae_up_layer_size;
        int vae_layer_per_block;

        std::string textstart = "<FLM_FIX_TOKEN_49406>";
        std::string textend = "<FLM_FIX_TOKEN_49407>";

        DPMSolverMultistepScheduler scheduler;
        WeightMap weight;

        std::map <std::string, int> deviceMap;
    };
}

#endif // STABLEDIFFUSION_H