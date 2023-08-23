//
// Created by siemon on 7/24/23.
//

#pragma once

#include "fastllm.h"

static map<int, pair<int, int>> FastllmTfaccKMRoundMap;
static vector<tfacc40t::BlasopList *> FastllmTfaccBlasopCache;
static map<long long int, vector<void *>> FastllmTfaccWeightRealSpace;
static map<long long int, vector<tfdl::TFData *>> FastllmTfaccWeightMap;

inline void FastllmTfaccAccumulateFloat32(float *x1, float *x2, float *y, size_t len);

inline void FastllmTfaccAccumulateFloat16(uint16_t *x1, uint16_t *x2, uint16_t *y, size_t len);

template<typename pointerType>
void FastllmTfaccCopyStride(pointerType *dst, pointerType *src, int len, int round, int dst_stride, int src_stride);

void FastllmTfaccQuantization(uint8_t *dst, float *src, int len, int round, int dst_stride, int src_stride, 
                              tfdl::PerChannelConfig config);

void FastllmTfaccQuantization(uint8_t *dst, uint16_t *src, int len, int round, int dst_stride, int src_stride,
                              tfdl::PerChannelConfig config);

void FastllmTfaccInitBlasop();

void FastllmTfaccClearBlasop();

void FastllmTfaccClearMemory();

void FastllmTfaccReleaseTempMemory();

void FastllmTfaccLinearFloat32WFloat32D(float *input, float *output, float *weight, float *bias, int n, int m, int k, 
                                       fastllm::ThreadPool *pool);

void FastllmTfaccLinearFloat16WFloat16D(uint16_t *input, uint16_t *output, uint16_t *weight, float *bias, int n, int m, int k,
                                        fastllm::ThreadPool *pool);

void FastllmTfaccLinearUint8WFloat32D(float *input, float *output, uint8_t *weight, float *bias, int n, int m, int k,
                                      tfdl::PerChannelConfig tfWeightConfig, fastllm::ThreadPool *pool);

void FastllmTfaccLinearUint8WFloat16D(uint16_t *input, uint16_t *output, uint8_t *weight, float *bias, int n, int m, int k,
                                      tfdl::PerChannelConfig tfWeightConfig, fastllm::ThreadPool *pool);
