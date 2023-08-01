//
// Created by hyy on 18-12-17.
//

#ifndef PROJECT_MERGEBATCH_H
#define PROJECT_MERGEBATCH_H

#include "../Layer.h"

namespace tfdl {
    template<typename T>
    class MergeBatchLayer : public Layer<T> {
    public:
        MergeBatchLayer(const json11::Json &config, string dataType) : Layer<T>(config, dataType) {
            auto &param = config["param"];
            padding = param["padding"].int_value();
            if (!param["reverse"].is_null()) {
                reverse = param["reverse"].bool_value();
                if (reverse) {
                    if (param["height"].is_null()) {
                        throw_exception("Error in layer " + this->GetName() + ": \"MergeBatchLayer\" with \"reserve\" == 1 should provide height.",new TFDLInitException());
                    } else {
                        height = param["height"].int_value();
                    }
                }
            }
        }

        MergeBatchLayer(string layerName, bool reverse, int padding, int height) : Layer<T>(layerName, "MergeBatch") {
            this->reverse = reverse;
            this->padding = padding;
            this->height = height;
        }

        ~MergeBatchLayer() {

        }

        string ToJsonParams();
        json11::Json ToJson();
        void Forward();
        void ForwardTF();
        void Reshape();
    private:
        int padding; //padding for height
        bool reverse = false;
        int height; //real height
    };

}

#endif //PROJECT_MERGEBATCH_H
