//
// Created by huangyuyang on 6/2/23.
//
#pragma once

#ifndef FASTLLM_UTILS_H
#define FASTLLM_UTILS_H

#include <map>
#include <chrono>
#include <string>
#include <cstdio>
#include <cstdint>
#include <vector>

#if defined(_WIN32) or defined(_WIN64)
#include <Windows.h>
#else
#include <unistd.h>
#endif

#ifdef __AVX__
#include "immintrin.h"
#ifdef __GNUC__
#if __GNUC__ < 8
#define _mm256_set_m128i(/* __m128i */ hi, /* __m128i */ lo) \
    _mm256_insertf128_si256(_mm256_castsi128_si256(lo), (hi), 0x1)
#endif
#endif
#endif

#ifdef __aarch64__
#include <arm_neon.h>
#include "armMath.h"
#endif

#ifdef USE_OPENCV
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#endif

namespace fastllm {
    static void MySleep(int t) {
#if defined(_WIN32) or defined(_WIN64)
        Sleep(t);
#else
        sleep(t);
#endif
    }

    #define ErrorInFastLLM(error) \
        fastllm::ErrorInFastLLMCore(error, __func__);
    
    #define WarningInFastLLM(warning) \
        fastllm::WarningInFastLLMCore(warning, __func__);

    static void ErrorInFastLLMCore(const std::string &error, const char *f) {
        printf("FastLLM Error @ %s: %s\n", f, error.c_str());
        throw error;
    }

    static void WarningInFastLLMCore(const std::string &warning, const char *f) {
        printf("FastLLM Warning @ %s: %s\n", f, warning.c_str());
    }

    static void AssertInFastLLM(bool condition, const std::string &error) {
        if (!condition) {
            ErrorInFastLLM(error);
        }
    }

    static uint32_t as_uint(const float x) {
        return *(uint32_t*)&x;
    }
    static float as_float(const uint32_t x) {
        return *(float*)&x;
    }

    static float half_to_float(const uint16_t x) { // IEEE-754 16-bit floating-point format (without infinity): 1-5-10, exp-15, +-131008.0, +-6.1035156E-5, +-5.9604645E-8, 3.311 digits
        const uint32_t e = (x & 0x7C00) >> 10; // exponent
        const uint32_t m = (x & 0x03FF) << 13; // mantissa
        const uint32_t v = as_uint((float) m) >> 23; // evil log2 bit hack to count leading zeros in denormalized format
        return as_float((x & 0x8000) << 16 | (e != 0) * ((e + 112) << 23 | m) | ((e == 0) & (m != 0)) * ((v - 37) << 23 |
                                                                                                         ((m << (150 - v)) &
                                                                                                          0x007FE000))); // sign : normalized : denormalized
    }
    static uint16_t float_to_half(const float x) { // IEEE-754 16-bit floating-point format (without infinity): 1-5-10, exp-15, +-131008.0, +-6.1035156E-5, +-5.9604645E-8, 3.311 digits
        const uint32_t b = as_uint(x) + 0x00001000; // round-to-nearest-even: add last bit after truncated mantissa
        const uint32_t e = (b & 0x7F800000) >> 23; // exponent
        const uint32_t m = b &
                       0x007FFFFF; // mantissa; in line below: 0x007FF000 = 0x00800000-0x00001000 = decimal indicator flag - initial rounding
        return (b & 0x80000000) >> 16 | (e > 112) * ((((e - 112) << 10) & 0x7C00) | m >> 13) |
               ((e < 113) & (e > 101)) * ((((0x007FF000 + m) >> (125 - e)) + 1) >> 1) |
               (e > 143) * 0x7FFF; // sign : normalized : denormalized : saturate
    }

    static double GetSpan(std::chrono::system_clock::time_point time1, std::chrono::system_clock::time_point time2) {
        auto duration = std::chrono::duration_cast<std::chrono::microseconds> (time2 - time1);
        return double(duration.count()) * std::chrono::microseconds::period::num / std::chrono::microseconds::period::den;
    };

    static bool StartWith(const std::string &a, const std::string &b) {
        return a.size() >= b.size() && a.substr(0, b.size()) == b;
    }

    static std::vector <int> ParseDeviceIds(const std::string &s, const std::string &type) {
        int i = type.size();
        std::vector <int> ret;
        std::string cur = "";
        if (s.size() > i && s[i] == ':') {
            i++;
            while (i < s.size()) {
                if (s[i] >= '0' && s[i] <= '9') {
                    cur += s[i];
                } else {
                    if (cur != "") {
                        ret.push_back(atoi(cur.c_str()));
                        cur = "";
                    }
                }
                i++;
            }
        }
        if (cur != "") {
            ret.push_back(atoi(cur.c_str()));
        }
        return ret;
    }

    struct TimeRecord {
        std::map<std::string, float> v;
        std::chrono::system_clock::time_point t;

        void Clear() {
            v.clear();
        }

        void Record() {
            t = std::chrono::system_clock::now();
        }

        void Record(const std::string &key) {
            auto now = std::chrono::system_clock::now();
            v[key] += GetSpan(t, now);
            t = now;
        }

        void Print() {
            float s = 0;
            for (auto &it: v) {
                printf("%s: %f s.\n", it.first.c_str(), it.second);
                s += it.second;
            }
            printf("Total: %f s.\n", s);
        }
    };

#ifdef __AVX__
    static inline float Floatsum(const __m256 a) {
        __m128 res = _mm256_extractf128_ps(a, 1);
        res = _mm_add_ps(res, _mm256_castps256_ps128(a));
        res = _mm_add_ps(res, _mm_movehl_ps(res, res));
        res = _mm_add_ss(res, _mm_movehdup_ps(res));
        return _mm_cvtss_f32(res);
    }

    static inline int I32sum(const __m256i a) {
        const __m128i sum128 = _mm_add_epi32(_mm256_extractf128_si256(a, 0), _mm256_extractf128_si256(a, 1));
        const __m128i hi64 = _mm_unpackhi_epi64(sum128, sum128);
        const __m128i sum64 = _mm_add_epi32(hi64, sum128);
        const __m128i hi32  = _mm_shuffle_epi32(sum64, _MM_SHUFFLE(2, 3, 0, 1));
        return _mm_cvtsi128_si32(_mm_add_epi32(sum64, hi32));
    }

    static inline int I16Sum(const __m256i a) {
        int sum = I32sum(_mm256_madd_epi16(a, _mm256_set1_epi16(1)));
        return sum;
    }
#endif

    static void convert_half_to_float(float *dst, uint16_t *src, size_t len) {
        for (size_t i = 0; i < len; i++) {
            dst[i] = half_to_float(src[i]);
        }
    }

    static void convert_float_to_half(uint16_t *dst, float *src, size_t len) {
        for (size_t i = 0; i < len; i++) {
            dst[i] = float_to_half(src[i]);
        }
    }

    static void convert_bf16_to_float(float *dst, uint16_t *src, size_t len) {
        // todo
    }

    static void convert_float_to_bf16(uint16_t *dst, float *src, size_t len) {
        // todo
    }

#ifdef __aarch64__
    static inline void quantu8(int len, float quant_scale, uint8_t quant_zero, float *input, uint8_t *output) {
        for (int i = 0; i < len % 8; i++) {
            output[i] = (uint8_t) (std::min(255.0, std::max(input[i] / quant_scale + quant_zero + 0.5, 0.0)));
        }
        float *endp = input + len;
        float *index = input + len % 8;
        uint8_t *outindex = output + len % 8;
        float scale[4];
        float zero[4];
        std::fill(scale, scale + 4, quant_scale);
        std::fill(zero, zero + 4, (float) (quant_zero + 0.5));
        float32x4_t scales = vld1q_f32(scale);
        float32x4_t zeros = vld1q_f32(zero);
        int32x4_t maxds = vcombine_s32(vcreate_s32(0x000000ff000000ff), vcreate_s32(0x000000ff000000ff));
        int32x4_t minds = vcombine_s32(vcreate_s32(0x0000000000000000), vcreate_s32(0x0000000000000000));
        for (; index != endp;) {
            float32x4_t fin1 = vld1q_f32(index);
            index += 4;
            float32x4_t fin2 = vld1q_f32(index);
            index += 4;
            fin1 = vaddq_f32(vdivq_f32(fin1, scales), zeros);
            fin2 = vaddq_f32(vdivq_f32(fin2, scales), zeros);
            int32x4_t out1 = vcvtq_s32_f32(fin1);
            int32x4_t out2 = vcvtq_s32_f32(fin2);
            out1 = vmaxq_s32(out1, minds);
            out1 = vminq_s32(out1, maxds);
            out2 = vmaxq_s32(out2, minds);
            out2 = vminq_s32(out2, maxds);
            uint16x8_t out3 = vpaddq_u16(vreinterpretq_u16_s32(out1), vreinterpretq_u16_s32(out2));
            uint8x8_t out = vmovn_u16(out3);
            vst1_u8(outindex, out);
            outindex += 8;
        }
    }

    static inline void dequantu8(int len, float quant_scale, uint8_t quant_zeropoint, uint8_t *input, float *output) {
        float scale[4];
        uint8_t zero[8];
        std::fill(scale, scale + 4, quant_scale);
        std::fill(zero, zero + 8, quant_zeropoint);
        float *endp = output + len;
        float32x4_t scales;
        uint8x8_t zeros;
        zeros = vld1_u8(zero);
        scales = vld1q_f32(scale);
        for (int i = 0; i < len % 8; i++) {
            output[i] = quant_scale * (input[i] - quant_zeropoint);
        }
        uint8_t *pin1 = input + len % 8;
        float32_t *pout = output + len % 8;
        for (; pout != endp;) {
            uint8x8_t a = vld1_u8(pin1);
            uint16x8_t result = vsubl_u8(a, zeros);
            int16x8_t sresult = vreinterpretq_s16_u16(result);
            int16x4_t result1 = vget_low_s16(sresult);
            int16x4_t result2 = vget_high_s16(sresult);
            int32x4_t result3 = vmovl_s16(result1);
            int32x4_t result4 = vmovl_s16(result2);
            float32x4_t out1 = vmulq_f32(scales, vcvtq_f32_s32(result3));
            float32x4_t out2 = vmulq_f32(scales, vcvtq_f32_s32(result4));
            vst1q_f32((pout), out1);
            pout += 4;
            vst1q_f32((pout), out2);
            pout += 4;
            pin1 += 8;

        }
    }
#endif

#ifdef USE_OPENCV
    static void opencvLoadImage(std::string path, std::string mode, int size, float *dst) {
        auto image = cv::imread(path, -1);
        if (mode == "RGB") {
            if (image.channels() == 1) {
                cv::cvtColor(image, image, cv::COLOR_GRAY2RGB);
            } else if (image.channels() == 4) {
                cv::cvtColor(image, image, cv::COLOR_BGRA2RGB);
            } else {
                cv::cvtColor(image, image, cv::COLOR_BGR2RGB);
            }
        } else if (mode == "BGR") {
            if (image.channels() == 1) {
                cv::cvtColor(image, image, cv::COLOR_GRAY2BGR);
            } else if (image.channels() == 4) {
                cv::cvtColor(image, image, cv::COLOR_BGRA2BGR);
            }
        }/*  else if (mode == "GRAY") {
            if (image.channels() == 4) {
                cv::cvtColor(image, image, cv::COLOR_BGRA2GRAY);
            } else {
                cv::cvtColor(image, image, cv::COLOR_BGR2GRAY);
            }
        } */

        cv::resize(image, image, cv::Size(size, size), 0, 0, cv::INTER_CUBIC);
        image.convertTo(image, CV_32FC3);

        std::vector<cv::Mat> channels;
        for (int i = 0; i < 3; i++) {
            channels.push_back(cv::Mat(size, size, CV_32FC1, dst + i * size * size));
        }
        cv::split(image, channels);
    }
#endif
}

#endif //FASTLLM_UTILS_H
