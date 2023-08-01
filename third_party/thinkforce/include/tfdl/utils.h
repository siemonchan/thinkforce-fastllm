//
// Created by test on 18-12-17.
//

#ifndef PROJECT_UTILS_H
#define PROJECT_UTILS_H
#include <common.h>

string ToLongString(float value);

void ReverseData(float * indata,int N,int C,int H,int W, int spatial);
void ReverseData(uint8_t * indata,int N,int C,int H,int W, int spatial);

string jsonFormat(string str);

// For Add, Sub, Mul ops.
struct ArithmeticParams {
    // uint8 inference params.
    int32_t input1_offset;
    int32_t input2_offset;
    int32_t output_offset;
    int32_t output_multiplier;
    int output_shift;
    // Add / Sub, not Mul, uint8 inference params.
    int left_shift;
    int32_t input1_multiplier;
    int input1_shift;
    int32_t input2_multiplier;
    int input2_shift;
    // uint8, etc, activation params.
    int32_t quantized_activation_min;
    int32_t quantized_activation_max;
    // float activation params.
    float float_activation_min;
    float float_activation_max;
};
void QuantizeMultiplierSmallerThanOneExp(double double_multiplier,
                                         int32_t* quantized_multiplier,
                                         int* left_shift);
void AddElementwise(const ArithmeticParams& params,
                           const uint8_t input1_data, const uint8_t input2_data,
                           uint8_t& output_data);
void SimpleMap(magicType *mapTable, magicType *input, magicType *output, int len);
void SimpleMap(float *mapTable, magicType *input, float *output, int len);
void SimpleMap_two(magicType *mapTable, magicType *input0, magicType *input1, magicType *output, int len);

#define LASTSHIFT 4
#define SHIFT 20
void Eltwise(magicType *input0, magicType *input1, magicType* output, int len, int m0, int m1, int offset);
#define FASTSHIFT 6
void FastEltwise(magicType *input0, magicType *input1, magicType* output, int len, int m0, int m1, int offset);
void FastEltwise(magicType *input0, magicType *input1, magicType* output, int len, int m0, int m1, int offset, magicType outputZero);
void FastEltwise(magicType *input0, magicType *input1, magicType* output, int len, int m0, int m1, int offset, magicType outputZero, magicType outputMax);
void Multiply(uint8_t *vec, uint8_t *mat, int32_t *ret, int n, int m, int k);
void Multiply(uint8_t *vec, uint16_t *mat, int32_t *ret, int n, int m, int k);
void MulElementwise(int size, const ArithmeticParams &params, const uint8_t *input1_data, const uint8_t *input2_data, uint8_t *output_data);
void MulSimpleBroadcast(int size, const ArithmeticParams &params, uint8_t broadcast_value, const uint8_t *input2_data, uint8_t *output_data);
#ifdef ARM
void transpose_neon_8X8(uint8_t* dst,uint8_t* src,int h,int w, int dstStrideW, int srcStrideW);
void transpose_neon_8X8(uint8_t* dst,uint8_t* src,int h,int w);
void transpose_neon_4x4(float *pDst, float *pSrc, int dstStride, int srcStride, int n, int m);
void int8scal(int len,float iscale,magicType zero,float ibias,magicType* input,magicType * output);
void dot_add(int len,float iscale1,magicType zero1,float iscale2,magicType zero2,float outscale,magicType outzero,magicType* input1,magicType* input2,magicType * output);
void fast_dot_add(int len,float iscale1,magicType zero1,float iscale2,magicType zero2,float outscale,magicType outzero,magicType* input1,magicType* input2,magicType * output);
void im2col(magicType* img, magicType* col);
void multiply(int16_t* vec, int16_t* mat, int32_t* ret, int n, int m, int k);
#ifdef USE_TFACC
void FastMemcpy(tfblas::MmapBuf<uint8_t>* dst,tfblas::MmapBuf<uint8_t>* src,uint32_t src_offset,uint32_t dst_offset,uint32_t len);
#endif
#endif
int MultiplyByQuantizedMultiplier(int x, int quantized_multiplier, int shift);
#endif //PROJECT_UTILS_H
