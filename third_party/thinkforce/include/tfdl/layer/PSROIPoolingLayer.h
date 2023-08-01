//
// Created by yuyang.huang on 17-11-14.
//

#ifndef TFDL_PSPRIORBOXLAYER_H
#define TFDL_PSPRIORBOXLAYER_H

#include "../Layer.h"

namespace tfdl {
    template<typename T>
    class PSROIPoolingLayer : public Layer<T> {
    public:
        PSROIPoolingLayer(const json11::Json &config, string dataType) : Layer<T>(config, dataType) {
            auto &param = config["param"];

            spatialScale = param["spatialScale"].number_value();
            outputDim = param["outputDim"].int_value();
            groupSize = param["groupSize"].int_value();

            if (outputDim <= 0) throw_exception("outputDim must be > 0",new TFDLInitException());
            if (groupSize <= 0) throw_exception("groupSize must be > 0",new TFDLInitException());

            pooledWidth = pooledHeight = groupSize;
        }

        ~PSROIPoolingLayer() {
        }

        // string ToJsonParams();
        json11::Json ToJson();
        void Forward();
        void ForwardTF() {}
        void Reshape();

    private:
        float spatialScale;
        int outputDim;
        int groupSize;
        int pooledWidth;
        int pooledHeight;
    };


}


#endif //CAFFE_PRIORBOXLAYER_H
