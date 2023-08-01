//
// Created by yuyang.huang on 17-11-14.
//

#ifndef TFDL_PERMUTELAYER_H
#define TFDL_PERMUTELAYER_H


#include "../Layer.h"

namespace tfdl {
    template<typename T>
    class PermuteLayer : public Layer<T> {
    public:
        PermuteLayer(const json11::Json &config, string dataType) : Layer<T>(config, dataType) {
            auto &param = config["param"];
            orders.clear();
            for (const auto &id : param["orders"].array_items()) {
                orders.push_back(id.int_value());
            }
        }
        ~PermuteLayer() {
            delete[] oldPos;
        }


        string orderString();
        string ToJsonParams();
        json11::Json ToJson();
        void Forward();
        void ForwardTF();
        void Reshape();
        string GetParams();
        void SaveMiniNet(std::ofstream &outputFile);
        vector<int> GetOrders();
        void SetOrders(vector<int> orders);
    private:
        vector <int> orders;
        int* oldPos = nullptr;
    };


}

#endif //CAFFE_PERMUTELAYER_H
