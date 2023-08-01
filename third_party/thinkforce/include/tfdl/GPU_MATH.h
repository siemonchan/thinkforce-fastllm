//
// Created by test on 17-11-29.
//
#ifdef USE_GPU
#ifndef PROJECT_GPU_MATH_H
#define PROJECT_GPU_MATH_H
#include <cblas.h>
#include <stdint.h>
#include <cublas_v2.h>
#include <cuda.h>
#include <cuda_runtime.h>
#include <curand.h>
#include <driver_types.h>
#include <iostream>

#define INSTANTIATE_LAYER_GPU_FORWARD(classname) \
  template void classname<float>::Forward_gpu(); \
  template void classname<uint8_t>::Forward_gpu();

#define CUDA_CHECK(condition) \
  /* Code block avoids redefinition of cudaError_t error */ \
  do { \
    cudaError_t error = condition; \
    if(error!=cudaSuccess) { std::cout<<cublasGetErrorString(error);} \
  } while (0)

#define CUBLAS_CHECK(condition) \
  do { \
    cublasStatus_t status = condition; \
    if(status!= CUBLAS_STATUS_SUCCESS){std::cout<< " " \
      << cublasGetErrorString(status); }\
  } while (0)


#define CUDA_KERNEL_LOOP(i, n) \
  for (int i = blockIdx.x * blockDim.x + threadIdx.x; \
       i < (n); \
       i += blockDim.x * gridDim.x)

namespace tfdl{
    template<typename T>
    void cuda_gemm(const CBLAS_TRANSPOSE TransA,
                   const CBLAS_TRANSPOSE TransB, const int M, const int N, const int K,
                   const T alpha, const T *A, const T *B, const T beta,
                   float *C);


    const char* cublasGetErrorString(cublasStatus_t error);
    extern cublasHandle_t cublas_handle_;

}
#endif //PROJECT_GPU_MATH_H
#endif