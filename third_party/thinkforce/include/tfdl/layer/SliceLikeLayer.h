//
// Created by huangyuyang on 11/25/19.
//

#ifndef PROJECT_SLICELIKELAYER_H
#define PROJECT_SLICELIKELAYER_H

#include "../Layer.h"

namespace tfdl {
    template<typename T>
    class SliceLikeLayer : public Layer<T> {
    public:
        SliceLikeLayer(const json11::Json &config, string dataType) : Layer<T>(config, dataType) {
            auto &param = config["param"];
            axis.clear();
            if (!param["axis"].is_null()) {
                for (const auto &id : param["axis"].array_items()) {
                    axis.push_back(id.int_value());
                }
            }

            if (!param["shape0"].is_null()) {
                for (const auto &id : param["shape0"].array_items()) {
                    shape0.push_back(id.int_value());
                }
            }

            if (!param["shape1"].is_null()) {
                for (const auto &id : param["shape1"].array_items()) {
                    shape1.push_back(id.int_value());
                }
            }
        }

        ~SliceLikeLayer() {

        };

        void Forward();

        void ForwardTF();

        void Reshape();

        bool InputIsWeight(int inputIndex); //判断第inputIndex个input是不是weight

        string arrayString(vector <int> &array);

        string ToJsonParams();

        json11::Json ToJson();
    private:
        vector <int> axis;
        vector <int> input0Dims, input1Dims, outputDims;
        vector <int> shape0, shape1;
    };
}

#endif //PROJECT_SLICELIKELAYER_H
