//
// Created by yuyang.huang on 17-11-14.
//

#ifndef TFDL_PROPOSALLAYER_H
#define TFDL_PROPOSALLAYER_H

#include "../Layer.h"

namespace tfdl {
    template<typename T>
    class ProposalLayer : public Layer<T> {
    public:
        ProposalLayer(const json11::Json &config, string dataType);

        ~ProposalLayer() {
        }

        // string arrayString(vector <float> &array);
        // string ToJsonParams();
        json11::Json ToJson();

        void Forward();
        void ForwardTF();
        void Reshape();
    private:
        int featStride;
        int baseSize;
        int minSize;

        vector<float> anchorScales, anchorRatios;
        vector<array<float, 4>> baseAnchors;

        int preNmsTopN;
        int postNmsTopN;
        float nmsThresh;
        float removeThresh;
    };


}


#endif //CAFFE_PRIORBOXLAYER_H
