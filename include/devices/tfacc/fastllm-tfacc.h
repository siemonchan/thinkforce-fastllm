//
// Created by siemon on 7/24/23.
//

#pragma once

#include "fastllm.h"

void FastllmTfaccAccumulate(float *x1, float *x2, float *y, size_t len);

void FastllmTfaccLinearMultiCore(float *input, float *output, uint8_t *weight, float *bias, int n, int m, int k,
                                 const tfdl::PerChannelConfig &tfWeightConfig, int threadNum, fastllm::ThreadPool *pool);