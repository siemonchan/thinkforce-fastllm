//
// Created by yuyang.huang on 18-4-13.
//

#ifndef PROJECT_PADDINGLAYER_H
#define PROJECT_PADDINGLAYER_H

#include "../Layer.h"

namespace tfdl {
    template<typename T>
    class PaddingLayer : public Layer<T> {
    public:
        PaddingLayer(const json11::Json &config, string dataType) : Layer<T>(config, dataType) {
            auto &param = config["param"];
            shapes.clear();
            for (const auto &id : param["shapes"].array_items()) {
                shapes.push_back(id.int_value());
            }
        }

        PaddingLayer(string layerName, vector <int> shapes) : Layer<T>(layerName, "Padding") {
            this->shapes = shapes;
        }

        ~PaddingLayer() {

        }

        static void padding(T *originalPos, T*newPos,
                  const vector<int> &originalDims, const vector<int> &originalCounts,
                  const vector<int> &newDims, const vector<int> &newCounts, int dimIndex);

        string shapeString();
        string ToJsonParams();
        json11::Json ToJson();
        void Forward();
        void ForwardTF();
        void Reshape();
    private:
        vector <int> shapes;
    };
}

#endif //PROJECT_PADDINGLAYER_H
