//
// Created by yuyang.huang on 17-11-14.
//

#ifndef TFDL_BASEPERMUTELAYER_H
#define TFDL_BASEPERMUTELAYER_H


#include "../Layer.h"

namespace tfdl {
    template<typename T>
    class BasePermuteLayer : public Layer<T> {
    public:
        BasePermuteLayer(const json11::Json &config, string dataType) : Layer<T>(config, dataType) {
        }
        ~BasePermuteLayer() {
        }

        void Forward();
        void ForwardTF();
        void Reshape(
             const std::vector<int>& dims,
             const std::vector<int>& inputStrides,
             const std::vector<int>& outputStrides,
             int inputOffset = 0,
             int outputOffset = 0);

    private:
        vector <int> dims;
        vector <int> inputStrides;
        vector <int> outputStrides;
        std::unique_ptr<int[]> oldPos;
    };


}

#endif //CAFFE_PERMUTELAYER_H
