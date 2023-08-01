//
// Created by huangyuyang on 12/24/21.
//

#ifndef PROJECT_CAFFE_H
#define PROJECT_CAFFE_H

#include <string>

#include "json11.hpp"

namespace tfdl {
    struct CaffeModelLayer {
        string name;
        string type;
        vector <vector <float> > blobs;
    };
    using CaffeModel = vector <CaffeModelLayer>;

    void ReadCaffePrototxt(const std::string &fileName, json11::Json &net, const std::vector <std::string> &ignoreLayers);
    void ReadCaffeModel(const std::string &fileName, CaffeModel &model);
}

#endif //PROJECT_CAFFE_H
