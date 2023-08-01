//
// Created by huangyuyang on 11/19/19.
//

#ifndef PROJECT_BINARYOPLAYER_H
#define PROJECT_BINARYOPLAYER_H

#include "../Layer.h"

namespace tfdl {
    template<typename T>
    class BinaryOpLayer : public Layer<T> {
    public:
        BinaryOpLayer(const json11::Json &config, string dataType) : Layer<T>(config, dataType) {
            auto &param = config["param"];
            op = param["op"].string_value();
            for (const auto &id : param["values"].array_items()) {
                values.push_back(id.number_value());
            }
        }

        ~BinaryOpLayer() {

        };

        float opFunc(float x);

        void Forward();

        void ForwardTF();

        void Prepare();

        void Reshape();

        string ValuesString();

        string ToJsonParams();

        json11::Json ToJson();

        string GetPrintType();

    private:
        bool isEqual;
        string op;
        vector <float> values;
    };
}

#endif //PROJECT_BINARYOPLAYER_H
