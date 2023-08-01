//
// Created by yuyang.huang on 17-11-22.
//

#ifndef TFDL_SLICELAYER_H
#define TFDL_SLICELAYER_H

#include "../Layer.h"

namespace tfdl {
    template<typename T>
    class SliceLayer : public Layer<T> {
    public:
        SliceLayer(const json11::Json &config, string dataType) : Layer<T>(config, dataType) {
            auto &param = config["param"];
            axis = param["axis"].int_value();
            slicePoints.clear();
            for (const auto &id : param["slicePoints"].array_items()) {
                slicePoints.push_back(id.int_value());
            }
        }
        ~SliceLayer() {

        }

        string slicePointsString();
        string ToJsonParams();
        json11::Json ToJson();
        void Forward();
        void ForwardTF();
        void Reshape();
        int GetAxis();
        vector<int> GetSlicePoints();
        void SetSkip(bool skip);

    private:
        int axis;
        vector <int> slicePoints;
        bool skip = false; //接在Convolution后的Slice层，如果axis = 1可以直接把Conv的结果写入相应位置。但需要batch = 1，否则不能跳过
    };


}

#endif //CAFFE_SLICELAYER_H
