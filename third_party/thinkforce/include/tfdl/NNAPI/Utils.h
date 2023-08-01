//
// Created by siemon on 1/9/19.
//

#ifndef PROJECT_UTILS_H
#define PROJECT_UTILS_H

#include "NNAPI/NNAPI.h"

magicType* KernelTransform(magicType *input, vector<uint32_t> shape, bool group, bool reverse);

float* KernelTransform(float *input, vector<uint32_t> shape, bool group, bool reverse);

int32_t* KernelTransform(int32_t *input, vector<uint32_t> shape, bool group, bool reverse);

std::pair<std::pair<float, int>, magicType *> QuantizeBias(std::vector<int> initialBias, float initialScale);

std::pair<std::pair<float, int>, int32_t*> QuantizeBias(std::vector<float> initialBias, float initialScale);

bool WhetherToReverseNetwork(std::vector<json11::Json> netJsonConfig,
                             std::vector<std::vector<uint32_t >> dataDimensions);

#endif //PROJECT_UTILS_H
