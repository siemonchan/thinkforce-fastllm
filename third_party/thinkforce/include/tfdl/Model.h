//
// Created by hyy on 19-1-17.
//

#ifndef PROJECT_MODEL_H
#define PROJECT_MODEL_H

#include "common.h"

using namespace std;

namespace tfdl {
    struct Int8Weight {
        float min, max;
        vector <vector <uint8_t> > weights;

        Int8Weight () {}

        Int8Weight(float min, float max) {
            this->min = min;
            this->max = max;
        }
    };

    struct Int8LayerWeight {
        string name;

        vector <Int8Weight> weights;

        Int8LayerWeight(string name) {
            this->name = name;
        }
    };

    struct Int8Data {
        string name;
        float min, max;

        Int8Data(string name, float min, float max) {
            this->name = name;
            this->min = min;
            this->max = max;
        }
    };

    struct Int8Model {
        vector <Int8LayerWeight> weights;
        vector <Int8Data> datas;
    };
}

#endif //PROJECT_MODEL_H
