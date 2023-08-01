//
// Created by huangyuyang on 11/25/19.
//

#ifndef PROJECT_BOXNMSLAYER_H
#define PROJECT_BOXNMSLAYER_H

#include "../Layer.h"

namespace tfdl {
    template<typename T>
    class BoxNMSLayer : public Layer<T> {
    public:
        BoxNMSLayer(const json11::Json &config, string dataType) : Layer<T>(config, dataType) {
            auto &param = config["param"];

            overlapThresh = param["overlapThresh"].is_null() ? 0.5f : param["overlapThresh"].number_value();
            validThresh = param["validThresh"].is_null() ? 0.f : param["validThresh"].number_value();
            topK = param["topK"].is_null() ? -1 : param["topK"].int_value();
            coordStart = param["coordStart"].is_null() ? 2 : param["coordStart"].int_value();
            scoreIndex = param["scoreIndex"].is_null() ? 1 : param["scoreIndex"].int_value();
            idIndex = param["idIndex"].is_null() ? -1 : param["idIndex"].int_value();
            backgroundId = param["backgroundId"].is_null() ? -1 : param["backgroundId"].int_value();
            forceSuppress = param["forceSuppress"].is_null() ? 0 : param["forceSuppress"].int_value();
            inFormat = param["inFormat"].is_null() ? "corner" : param["inFormat"].string_value();
            outFormat = param["outFormat"].is_null() ? "cornor" : param["outFormat"].string_value();

            if (inFormat != outFormat) {
                throw_exception("BoxNMSLayer.Init: inFormat and outFormat should be same.", new TFDLInitException());
            }
        }

        ~BoxNMSLayer() {
        };

        void Forward();

        void ForwardTF();

        string ToJsonParams();

        json11::Json ToJson();
    private:
        float overlapThresh, validThresh;
        int topK, coordStart, scoreIndex, idIndex, backgroundId, forceSuppress;
        string inFormat, outFormat;
    };
}

#endif //PROJECT_BOXNMSLAYER_H
