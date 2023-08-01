//
// Created by siemon on 7/5/19.
//

#ifndef PROJECT_ROIALIGNLAYER_H
#define PROJECT_ROIALIGNLAYER_H

#include "Layer.h"

namespace tfdl {
    template <typename T>
    class ROIAlignLayer : public Layer<T> {
    public:
        ROIAlignLayer(const json11::Json &config, string dataType) : Layer<T>(config, dataType) {
            auto &param = config["param"];
            pooledWidth = param["pooledWidth"].int_value();
            pooledHeight = param["pooledHeight"].int_value();
            spatialScale = param["spatialScale"].number_value();
        }

        ~ROIAlignLayer(){

        }

        void Forward();
        void ForwardTF();
        void Reshape();

    private:
        int pooledWidth;
        int pooledHeight;
        float spatialScale;

        float BilinearInterpolate(const float* ptr, int w, int h, float x, float y);
    };
}

#endif //PROJECT_ROIALIGNLAYER_H
