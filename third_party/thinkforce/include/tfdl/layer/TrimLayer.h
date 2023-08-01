//
// Created by androidserver on 18-4-13.
//

#ifndef PROJECT_TRIMLAYER_H
#define PROJECT_TRIMLAYER_H

#include "../Layer.h"

namespace tfdl {
    template<typename T>
    class TrimLayer : public Layer<T> {
    public:
        TrimLayer(const json11::Json &config, string dataType) : Layer<T>(config, dataType) {
            auto &param = config["param"];
            shapes.clear();
            for (const auto &id : param["shapes"].array_items()) {
                shapes.push_back(id.int_value());
            }
        }

        TrimLayer(string layerName, vector <int> shapes) : Layer<T>(layerName, "Trim"){
            this->shapes = shapes;
        }

        ~TrimLayer() {

        }

        static void trim(T *originalPos, T*newPos,
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

#endif //PROJECT_TRIMLAYER_H
