//
// Created by siemon on 7/5/19.
//

#ifndef PROJECT_ROIPOOLINGLAYER_H
#define PROJECT_ROIPOOLINGLAYER_H

#include "../Layer.h"

namespace tfdl {
    template <typename T>
    class ROIPoolingLayer : public Layer<T> {
    public:
        ROIPoolingLayer(const json11::Json &config, string dataType) : Layer<T>(config, dataType) {
            auto &param = config["param"];
            pooledWidth = param["pooledWidth"].int_value();
            pooledHeight = param["pooledHeight"].int_value();
            spatialScale = param["spatialScale"].number_value();
        }

        void Forward();
        void ForwardTF();
        void Reshape();

    private:
        int pooledWidth;
        int pooledHeight;
        float spatialScale;
    };
}

#endif //PROJECT_ROIPOOLINGLAYER_H
