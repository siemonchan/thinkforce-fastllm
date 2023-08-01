//
// Created by cjq on 18-10-10.
//

#ifndef PROJECT_NNAPIBUILDWEIGHT_H
#define PROJECT_NNAPIBUILDWEIGHT_H

#include "NNAPI/NNAPI.h"

std::vector<pair<int, const void*>> BuildFloatConvolutionWeight(uint32_t inputCount, const uint32_t *inputs,
                                                                std::vector<uint32_t> dataDimensionCount,
                                                                std::vector<std::vector<uint32_t >> dataDimensions,
                                                                std::vector<size_t> dataLength,
                                                                std::map<uint32_t, const void *> &dataValue,
                                                                bool group, bool reverseWeight);

std::vector<pair<int, const void*>> BuildFloatFullyConnectWeight(uint32_t inputCount, const uint32_t *inputs,
                                                                 std::vector<uint32_t> dataDimensionCount,
                                                                 std::vector<std::vector<uint32_t >> dataDimensions,
                                                                 std::vector<size_t> dataLength,
                                                                 std::map<uint32_t, const void *> &dataValue,
                                                                 vector<uint32_t> FCInputShape, bool reverseWeight);

std::vector<pair<int, const void*>> BuildFloatBatchNormWeight(size_t meanLen, const void *meanValue,
                                                              size_t varLen, const void *varValue,
                                                              float *defaultScale);

std::vector<pair<int, const void*>> BuildFloatBatchNormWeightSUBDIV(size_t meanLen, const void *meanValue,
                                                                    size_t varLen, const void *varValue,
                                                                    float *defaultScale);

std::vector<pair<int, const void*>> BuildFloatScaleWeight(size_t multiplierLen, const void *multiplierValue,
                                                          size_t biastermLen, const void *biastermValue);

/*
std::vector<pair<int, void*>> buildFloatLSTMWeight(uint32_t inputCount, const uint32_t* inputs,
                                                   std::vector<uint32_t> dataDimensionCount,
                                                   std::vector<std::vector<uint32_t >> dataDimensions,
                                                   std::vector<size_t> dataLength,
                                                   std::map<uint32_t, const void*> dataValue);
                                                   */

std::vector<pair<int, const void*>> BuildInt8ConvolutionWeight(uint32_t inputCount, const uint32_t *inputs,
                                                               std::vector<uint32_t> dataDimensionCount,
                                                               std::vector<std::vector<uint32_t >> dataDimensions,
                                                               std::vector<size_t> dataLength,
                                                               std::map<uint32_t, const void *> &dataValue,
                                                               bool group, bool reverseWeight);

std::vector<pair<int, const void*>> BuildInt8FullyConnectWeight(uint32_t inputCount, const uint32_t *inputs,
                                                                std::vector<uint32_t> dataDimensionCount,
                                                                std::vector<std::vector<uint32_t >> dataDimensions,
                                                                std::vector<size_t> dataLength,
                                                                std::map<uint32_t, const void *> &dataValue,
                                                                vector<uint32_t> FCInputShape, bool reverseWeight);

std::vector<pair<int, const void*>> BuildInt8BatchNormWeight(size_t meanLen, const void *meanValue,
                                                             size_t varLen, const void *varValue,
                                                             float *defaultScale);

std::vector<pair<int, const void*>> BuildInt8BatchNormWeightSUBDIV(size_t meanLen, const void *meanValue,
                                                                   size_t varLen, const void *varValue,
                                                                   float *defaultScale);

std::vector<pair<int, const void*>> BuildInt8ScaleWeight(size_t multiplierLen, const void *multiplierValue,
                                                         size_t biastermLen, const void *biastermValue);
#endif //PROJECT_NNAPIBUILDWEIGHT_H
