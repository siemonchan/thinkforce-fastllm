//
// Created by yuyang.huang on 17-11-15.
//

#ifndef TFDL_ReshapeCLASS_H
#define TFDL_ReshapeCLASS_H

#include "../Layer.h"

namespace tfdl {
    template<typename T>
    class ReshapeLayer : public Layer<T> {
    public:
        ReshapeLayer(const json11::Json &config, string dataType) : Layer<T>(config, dataType) {
            auto &param = config["param"];
            shapes.clear();
            for (const auto &id : param["shapes"].array_items()) {
                shapes.push_back(id.int_value());
            }
        }
        ~ReshapeLayer() {

        }

        string shapeString();
        string ToJsonParams();
        json11::Json ToJson();
        void SaveMiniNet(std::ofstream &outputFile);
        void Forward();
        void ForwardTF();
        void Reshape();
    private:
        vector <int> shapes;
    };


}

#endif //CAFFE_ReshapeCLASS_H
