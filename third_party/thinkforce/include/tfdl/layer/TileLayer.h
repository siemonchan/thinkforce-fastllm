//
// Created by huangyuyang on 11/25/19.
//

#ifndef PROJECT_TILELAYER_H
#define PROJECT_TILELAYER_H

#include "../Layer.h"

namespace tfdl {
    template<typename T>
    class TileLayer : public Layer<T> {
    public:
        TileLayer(const json11::Json &config, string dataType) : Layer<T>(config, dataType) {
            auto &param = config["param"];

            //有reps则用reps，否则用axis和tiles
            reps.clear();
            if (!param["reps"].is_null()) {
                for (const auto &id : param["reps"].array_items()) {
                    reps.push_back(id.int_value());
                }
            } else {
                axis = param["axis"].int_value();
                tiles = param["tiles"].int_value();
            }
        }
        ~TileLayer() {
        }

        void Make(magicType *input, magicType *output, int axis, int step);

        string repsString();
        string ToJsonParams();
        json11::Json ToJson();
        void Forward();
        void ForwardTF();
        void Reshape();
    private:
        int dims;
        int axis, tiles;
        vector <int> reps;
        vector <int> inputDims, outputDims;
        vector <int> inputSpatial, outputSpatial;
        vector <int> realReps;
    };
}

#endif //PROJECT_TILELAYER_H
