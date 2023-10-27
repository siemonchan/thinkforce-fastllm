//
// Created by siemon on 8/9/23.
//

#ifndef TEST_QWEN_H
#define TEST_QWEN_H

#include "basellm.h"

namespace fastllm {
    class VisualModel {
    public:
        VisualModel(WeightMap *weight);

        void Forward(const Data &allImage, Data *images);

        void Decode(const std::vector<std::string> &imagePaths, Data *images);
        
        std::string FromListFormat(const visualDictData &data);

        visualDictData ToListFormat(const std::string &text);
        
        int width;
        int imageSize;
        int nQueries;
        int outputDim;
        int patchSize;
        int layers;

        int heads;
        int grid_size;

        int samplerNumHead;
        int samplerHeadDim;

        Data mean, std;
        Data positionalEmbedding;
        Data samplerPosEmbed, samplerPosEmbedAbsPos;

        Data samplerWQ, samplerWK, samplerWV;
        Data samplerBQ, samplerBK, samplerBV;

        std::map<std::string, Data *> imageMap;

        WeightMap *weight;

        std::string image_start_tag = "<img>";
        std::string image_end_tag = "</img>";
        std::string ref_start_tag = "<ref>";
        std::string ref_end_tag = "</ref>";
        std::string box_start_tag = "<box>";
        std::string box_end_tag = "</box>";
        std::string quad_start_tag = "<quad>";
        std::string quad_end_tag = "</quad>";
    };

    class QWenModel : public basellm {
    public:
        QWenModel();

        void LoadFromFile(const std::string &fileName);

        // 推理
		virtual int Forward(
                const Data &inputIds,
                const Data &attentionMask,
                const Data &positionIds,
                std::vector <std::pair <Data, Data> > &pastKeyValues,
                const GenerationConfig &generationConfig = GenerationConfig(),
                const LastTokensManager &lastTokens = LastTokensManager(),
                std::vector <float> *logits = nullptr);

        std::vector <int> ForwardBatch(
                int batch,
                const Data &inputIds,
                const Data &attentionMask,
                const Data &positionIds,
                std::vector <std::pair <Data, Data> > &pastKeyValues,
                const GenerationConfig &generationConfig = GenerationConfig(),
                const LastTokensManager &lastTokens = LastTokensManager(),
                std::vector <std::vector <float>*> *logits = nullptr);
        
        std::vector <int> ForwardBatch(
                int batch,
                const Data &inputIds,
                const std::vector <Data*> &attentionMask,
                const std::vector <Data*> &positionIds,
                const std::vector <int> &seqLens,
                std::vector <std::pair <Data*, Data*> > &pastKeyValues,
                const std::vector <GenerationConfig> &generationConfigs,
                const LastTokensManager &lastTokens = LastTokensManager(),
                std::vector <std::vector <float>*> *retLogits = nullptr);

        virtual std::string MakeInput(const std::string &history, int round, const std::string &input);

        virtual std::string MakeHistory(const std::string &history, int round, const std::string &input, const std::string &output);

        virtual std::string MakeInputVL(const std::string &history, int round, const visualDictData &input);

        virtual std::string MakeHistoryVL(const std::string &history, int round, const visualDictData &input, const std::string &output);

        virtual void FillLLMInputs(std::vector <std::vector <float> > &inputTokens,
                                   const std::map <std::string, int> &params,
                                   Data &inputIds, Data &attentionMask, Data &positionIds);

        virtual void FillLLMInputsBatch(std::vector <std::vector <float> > &inputTokens,
                                        const std::vector <std::map <std::string, int> > &params,
                                        Data &inputIds, Data &attentionMask, Data &positionIds);
        
        virtual void WarmUp();

        void UpdateRotaryPosEmb(float ntk_alpha);

        int seq_length;
        float ntk_alpha;

        bool use_log_attn;
        Data logn_list;

        VisualModel *visual = nullptr;
    
    private:
        std::string im_start = "<|im_start|>";
        std::string im_end = "<|im_end|>";
        int image_start_id = 151857;
    };
}

#endif //TEST_QWEN_H