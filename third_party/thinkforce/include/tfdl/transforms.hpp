//
// Created by siemon on 5/27/20.
//

#ifndef TFDL_TRANSFORMS_HPP
#define TFDL_TRANSFORMS_HPP
#pragma once
#ifdef ARM

#include "arm_neon.h"

#endif

#include <cmath>
#include <algorithm>
#include <numeric>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <iostream>
#include <random>

// functions below are used to handle image data in pre-processing

using namespace std;

namespace tfdl {
    namespace transforms {
        /*
         *  normalizes a one-channel float image with size [H, W]
         *  calculation can be described as: dst[i] = (src[i] - mean) / std
         */
        inline void normalize_1c_f(float *src, float *dst, int h, int w, float mean, float std) {
            int i = 0;
#ifdef ARM
            float32x4_t dup_mean = vld1q_dup_f32(&mean);
            float32x4_t dup_std = vld1q_dup_f32(&std);
            for (; i < h * w; i += 4) {
                vst1q_f32(dst + i, vdivq_f32(vsubq_f32(vld1q_f32(src + i), dup_mean), dup_std));
            }
#endif
            for (; i < h * w; i++) {
                dst[i] = (src[i] - mean) / std;
            }
        }

        static void normalize_3c_f(float *src, float *dst, int h, int w, float *mean, float *std) {
            normalize_1c_f(src, dst, h, w, mean[0], std[0]);
            normalize_1c_f(src + h * w, dst + h * w, h, w, mean[1], std[1]);
            normalize_1c_f(src + 2 * h * w, dst + 2 * h * w, h, w, mean[2], std[2]);
        }

        static void normalize_nc_f(float *src, float *dst, int c, int h, int w, float *mean, float *std) {
            for (int i = 0; i < c; i++) {
                normalize_1c_f(src + i * h * w, dst + i * h * w, h, w, mean[i], std[i]);
            }
        }

        /*
         *  normalizes a one-channel float image with size [H, W] to the range of [0, 1.0]
         *  calculation can be described as: dst[i] = src[i] / max(1 << 8)
         */
        inline void normalize_img_01_f(float *src, float *dst, int size) {
            auto scale = static_cast<float>(numeric_limits<uint8_t>::max());
            int i = 0;
#ifdef ARM
            float32x4_t dup_scale = vld1q_dup_f32(&scale);
            for (; i < size; i += 4) {
                vst1q_f32(dst + i, vdivq_f32(vld1q_f32(src + i), dup_scale));
            }
#endif
            for (; i < size; i++) {
                dst[i] = src[i] / scale;
            }
        }

        static void normalize_img_01_1c_f(float *src, float *dst, int h, int w) {
            normalize_img_01_f(src, dst, h * w);
        }

        static void normalize_img_01_3c_f(float *src, float *dst, int h, int w) {
            normalize_img_01_f(src, dst, 3 * h * w);
        }

        static void normalize_img_01_nc_f(float *src, float *dst, int c, int h, int w) {
            normalize_img_01_f(src, dst, c * h * w);
        }

        inline void get_mean_variance_f(float *src, std::size_t len, float &mean, float &var) {
            int i = 0;
            float sum = 0.f;
#ifdef ARM
            float32x4_t sumx4 = vld1q_dup_f32(&sum);
            for (; i < len; i += 4) {
                vaddq_f32(sumx4, vld1q_f32(src + i));
            }
            sum += sumx4[0] + sumx4[1] + sumx4[2] + sumx4[3];
#endif
            sum += accumulate(src + i, src + len, 0);
            mean = sum / (float) len;

            i = 0;
            sum = 0.f;
#ifdef ARM
            float32x4_t meanx4 = vld1q_dup_f32(&mean);
            sumx4 = vld1q_dup_f32(&sum);
            for (; i < len; i += 4) {
                float32x4_t diffx4 = vsubq_f32(vld1q_f32(src + i), meanx4);
                vaddq_f32(sumx4, vmulq_f32(diffx4, diffx4));
            }
            sum += sumx4[0] + sumx4[1] + sumx4[2] + sumx4[3];
#endif
            for(; i < len; i++) {
                sum += pow((src[i] - mean), 2);
            }
            var = sum / (float) len;
        }

        static void instance_normalize_1c_f(float *src, float *dst, int h, int w) {
            // calculate mean and variance
            float mean, var;
            get_mean_variance_f(src, h * w, mean, var);
            normalize_1c_f(src, dst, h, w, mean, sqrt(var));
        }

        static void instance_normalize_3c_f(float *src, float *dst, int h, int w) {
            int spatial = h * w;
            instance_normalize_1c_f(src, dst, h, w);
            instance_normalize_1c_f(src + spatial, dst + spatial, h, w);
            instance_normalize_1c_f(src + 2 * spatial, dst + 2 * spatial, h, w);
        }

        static void instance_normalize_nc_f(float *src, float *dst, int h, int w, int c) {
            int spatial = h * w;
            for (int i = 0; i < c; i++) {
                instance_normalize_1c_f(src + i * spatial, dst + i * spatial, h , w);
            }
        }

        /*
         *  flip a one-channel float image with size [H, W] horizontally
         *  src and dst can point to same address
         */
        inline void flip_horizontal_1c_f(float *src, float *dst, int h, int w) {
            if (src == dst) {
                for (int i = 0; i < h; i++) {
                    float *src_walk = src + i * w;
                    int j = 0;
#ifdef ARM
                    auto dup_tmp = new float[2];
                    for (; (j + 2) <= w / 2; j += 2) {
                        vst1_f32(dup_tmp, vrev64_f32(vld1_f32(src_walk + j)));
                        vst1_f32(src_walk + j, vrev64_f32(vld1_f32(src_walk + w - 2 - j)));
                        vst1_f32(src_walk + w - 2 - j, vld1_f32(dup_tmp));
                    }
                    delete[] dup_tmp;
#endif
                    float tmp;
                    for (; j < w / 2; j++) {
                        tmp = src_walk[j];
                        src_walk[j] = src_walk[w - 1 - j];
                        src_walk[w - 1 - j] = tmp;
                    }
                }
            } else {
                for (int i = 0; i < h; i++) {
                    float *src_walk = src + i * w;
                    float *dst_walk = dst + i * w;
                    int j = 0;
#ifdef ARM
                    for (; j < w; j += 2) {
                        vst1_f32(dst_walk + j, vrev64_f32(vld1_f32(src_walk + w - 4 - j)));
                    }
#endif
                    for (; j < w; j++) {
                        dst_walk[j] = src_walk[w - 1 - j];
                    }
                }
            }
        }

        static void flip_horizontal_3c_f(float *src, float *dst, int h, int w) {
            flip_horizontal_1c_f(src, dst, h, w);
            flip_horizontal_1c_f(src + h * w, dst + h * w, h, w);
            flip_horizontal_1c_f(src + 2 * h * w, dst + 2 * h * w, h, w);
        }

        static void flip_horizontal_nc_f(float *src, float *dst, int c, int h, int w) {
            for (int i = 0; i < c; i++) {
                flip_horizontal_1c_f(src + i * h * w, dst + i * h * w, h, w);
            }
        }

        /*
         *  flip a one-channel float image with size [H, W] vertically
         *  src and dst can point to same address
         */
        inline void flip_vertical_1c_f(float *src, float *dst, int h, int w) {
            if (src == dst) {
                auto tmp = new float[w];
                for (int i = 0; i < h / 2; i++) {
                    memcpy(tmp, src + i * w, w * sizeof(float));
                    memcpy(src + i * w, src + (h - 1 - i) * w, w * sizeof(float));
                    memcpy(src + (h - 1 - i) * w, tmp, w * sizeof(float));
                }
            } else {
                for (int i = 0; i < h; i++) {
                    memcpy(dst + i * w, src + (h - 1 - i) * w, w * sizeof(float));
                }
            }
        }

        static void flip_vertical_3c_f(float *src, float *dst, int h, int w) {
            flip_vertical_1c_f(src, dst, h, w);
            flip_vertical_1c_f(src + h * w, dst + h * w, h, w);
            flip_vertical_1c_f(src + 2 * h * w, dst + 2 * h * w, h, w);
        }

        static void flip_vertical_nc_f(float *src, float *dst, int c, int h, int w) {
            for (int i = 0; i < c; i++) {
                flip_vertical_1c_f(src + i * h * w, dst + i * h * w, h, w);
            }
        }

        /*
         *  flip a one-channel uint8 image with size [H, W] horizontally
         *  src and dst can point to same address
         */
        inline void flip_horizontal_1c_u8(uint8_t *src, uint8_t *dst, int h, int w) {
            if (src == dst) {
                for (int i = 0; i < h; i++) {
                    uint8_t *src_walk = src + i * w;
                    int j = 0;
#ifdef ARM
                    auto dup_tmp = new uint8_t[8];
                    for (; (j + 8) <= w / 2; j += 8) {
                        vst1_u8(dup_tmp, vrev64_u8(vld1_u8(src_walk + j)));
                        vst1_u8(src_walk + j, vrev64_u8(vld1_u8(src + w - 8 - j)));
                        vst1_u8(src_walk + w - 1 - j, vld1_u8(dup_tmp));
                    }
                    delete[] dup_tmp;
#endif
                    uint8_t tmp;
                    for (; j < w / 2; j++) {
                        tmp = src_walk[j];
                        src_walk[j] = src_walk[w - 1 - j];
                        src_walk[w - 1 - j] = tmp;
                    }
                }
            } else {
                for (int i = 0; i < h; i++) {
                    uint8_t *src_walk = src + i * w;
                    uint8_t *dst_walk = dst + i * w;
                    int j = 0;
#ifdef ARM
                    for (; j < w; j += 8) {
                        vst1_u8(dst_walk + j, vrev64_u8(vld1_u8(src_walk + w - 8 - j)));
                    }
#endif
                    for (; j < w; j++) {
                        dst_walk[j] = src_walk[w - 1 - j];
                    }
                }
            }
        }

        static void flip_horizontal_3c_u8(uint8_t *src, uint8_t *dst, int h, int w) {
            flip_horizontal_1c_u8(src, dst, h, w);
            flip_horizontal_1c_u8(src + h * w, dst + h * w, h, w);
            flip_horizontal_1c_u8(src + 2 * h * w, dst + 2 * h * w, h, w);
        }

        static void flip_horizontal_nc_u8(uint8_t *src, uint8_t *dst, int c, int h, int w) {
            for (int i = 0; i < c; i++) {
                flip_horizontal_1c_u8(src + i * h * w, dst + i * h * w, h, w);
            }
        }

        /*
         *  flip a one-channel uint8 image with size [H, W] vertically
         *  src and dst can point to same address
         */
        inline void flip_vertical_1c_u8(uint8_t *src, uint8_t *dst, int h, int w) {
            if (src == dst) {
                auto tmp = new uint8_t[w];
                for (int i = 0; i < h / 2; i++) {
                    memcpy(tmp, src + i * w, w * sizeof(uint8_t));
                    memcpy(src + i * w, src + (h - 1 - i) * w, w * sizeof(uint8_t));
                    memcpy(src + (h - 1 - i) * w, tmp, w * sizeof(uint8_t));
                }
            } else {
                for (int i = 0; i < h; i++) {
                    memcpy(dst + i * w, src + (h - 1 - i) * w, w * sizeof(uint8_t));
                }
            }
        }

        static void flip_vertical_3c_u8(uint8_t *src, uint8_t *dst, int h, int w) {
            flip_vertical_1c_u8(src, dst, h, w);
            flip_vertical_1c_u8(src + h * w, dst + h * w, h, w);
            flip_vertical_1c_u8(src + 2 * h * w, dst + 2 * h * w, h, w);
        }

        static void flip_vertical_nc_u8(uint8_t *src, uint8_t *dst, int c, int h, int w) {
            for (int i = 0; i < c; i++) {
                flip_vertical_1c_u8(src + i * h * w, dst + i * h * w, h, w);
            }
        }

        /*
         *  crop a one-channel float image with size [h, w] to [crop_h, crop_w],
         *      top (int): Vertical component of the top left corner of the crop box.
         *      left (int): Horizontal component of the top left corner of the crop box.
         */
        inline void crop_1c_f(float *src, float *dst, int h, int w, int top, int left, int crop_h, int crop_w) {
            assert(crop_h + top <= h);
            assert(crop_w + left <= w);
            float *src_walk = src + top * w;
            for (int i = 0; i < crop_h; i++) {
                memcpy(dst + i * crop_w, src_walk + left, crop_w * sizeof(float));
                src_walk += w;
            }
        }

        static void crop_3c_f(float *src, float *dst, int h, int w, int top, int left, int crop_h, int crop_w) {
            crop_1c_f(src, dst, h, w, top, left, crop_h, crop_w);
            crop_1c_f(src + h * w, dst + crop_h * crop_w, h, w, top, left, crop_h, crop_w);
            crop_1c_f(src + 2 * h * w, dst + 2 * crop_h * crop_w, h, w, top, left, crop_h, crop_w);
        }

        static void crop_nc_f(float *src, float *dst, int c, int h, int w, int top, int left, int crop_h, int crop_w) {
            for (int i = 0; i < c; i++) {
                crop_1c_f(src + i * h * w, dst + i * crop_h * crop_w, h, w, top, left, crop_h, crop_w);
            }
        }

        /*
         *  crop a one-channel uint8 image with size [h, w] to [crop_h, crop_w],
         *      top (int): Vertical component of the top left corner of the crop box.
         *      left (int): Horizontal component of the top left corner of the crop box.
         */
        inline void crop_1c_u8(uint8_t *src, uint8_t *dst, int h, int w, int top, int left, int crop_h, int crop_w) {
            assert(crop_h + top <= h);
            assert(crop_w + left <= w);
            uint8_t *src_walk = src + top * w;
            for (int i = 0; i < crop_h; i++) {
                memcpy(dst + i * crop_w, src_walk + left, crop_w * sizeof(uint8_t));
                src_walk += w;
            }
        }

        static void crop_3c_u8(uint8_t *src, uint8_t *dst, int h, int w, int top, int left, int crop_h, int crop_w) {
            crop_1c_u8(src, dst, h, w, top, left, crop_h, crop_w);
            crop_1c_u8(src + h * w, dst + crop_h * crop_w, h, w, top, left, crop_h, crop_w);
            crop_1c_u8(src + 2 * h * w, dst + 2 * crop_h * crop_w, h, w, top, left, crop_h, crop_w);
        }

        static void crop_nc_u8(uint8_t *src, uint8_t *dst, int c, int h, int w, int top, int left, int crop_h, int crop_w) {
            for (int i = 0; i < c; i++) {
                crop_1c_u8(src + i * h * w, dst + i * crop_h * crop_w, h, w, top, left, crop_h, crop_w);
            }
        }

        static void center_crop_1c_f(float *src, float *dst, int h, int w, int crop_h, int crop_w) {
            int top = (h - crop_h) / 2;
            int left = (w - crop_w) / 2;
            assert(top > 0);
            assert(left > 0);
            crop_1c_f(src, dst, h, w, top, left, crop_h, crop_w);
        }

        static void center_crop_3c_f(float *src, float *dst, int h, int w, int crop_h, int crop_w) {
            int top = (h - crop_h) / 2;
            int left = (w - crop_w) / 2;
            assert(top > 0);
            assert(left > 0);
            crop_3c_f(src, dst, h, w, top, left, crop_h, crop_w);
        }

        static void center_crop_nc_f(float *src, float *dst, int c, int h, int w, int crop_h, int crop_w) {
            int top = (h - crop_h) / 2;
            int left = (w - crop_w) / 2;
            assert(top > 0);
            assert(left > 0);
            crop_nc_f(src, dst, c, h, w, top, left, crop_h, crop_w);
        }

        static void center_crop_1c_u8(uint8_t *src, uint8_t *dst, int h, int w, int crop_h, int crop_w) {
            int top = (h - crop_h) / 2;
            int left = (w - crop_w) / 2;
            assert(top > 0);
            assert(left > 0);
            crop_1c_u8(src, dst, h, w, top, left, crop_h, crop_w);
        }

        static void center_crop_3c_u8(uint8_t *src, uint8_t *dst, int h, int w, int crop_h, int crop_w) {
            int top = (h - crop_h) / 2;
            int left = (w - crop_w) / 2;
            assert(top > 0);
            assert(left > 0);
            crop_3c_u8(src, dst, h, w, top, left, crop_h, crop_w);
        }

        static void center_crop_nc_u8(uint8_t *src, uint8_t *dst, int c, int h, int w, int crop_h, int crop_w) {
            int top = (h - crop_h) / 2;
            int left = (w - crop_w) / 2;
            assert(top > 0);
            assert(left > 0);
            crop_nc_u8(src, dst, c, h, w, top, left, crop_h, crop_w);
        }

        static void corner_crop_1c_f(float *src, float *dst, int h, int w, int crop_h, int crop_w, int flag) {
            // int flag:
            // 0 -- top left corner crop;
            // 1 -- top right corner crop;
            // 2 -- bottom left corner crop;
            // 3 -- bottom right corner crop;
            int top, left;
            switch (flag) {
                case 0:
                    top = 0;
                    left = 0;
                    break;
                case 1:
                    top = 0;
                    left = w - crop_w;
                    break;
                case 2:
                    top = h - crop_h;
                    left = 0;
                    break;
                case 3:
                    top = h - crop_h;
                    left = w - crop_w;
                    break;
                default:
                    cout << "int flag:\n"
                            "   0 -- top left corner crop;\n"
                            "   1 -- top right corner crop;\n"
                            "   2 -- bottom left corner crop;\n"
                            "   3 -- bottom right corner crop;"
                         << endl;
            }
            crop_1c_f(src, dst, h, w, top, left, crop_h, crop_w);
        }

        static void corner_crop_3c_f(float *src, float *dst, int h, int w, int crop_h, int crop_w, int flag) {
            int src_step = h * w;
            int dst_step = crop_h * crop_w;
            corner_crop_1c_f(src, dst, h, w, crop_h, crop_w, flag);
            corner_crop_1c_f(src + src_step, dst + dst_step, h, w, crop_h, crop_w, flag);
            corner_crop_1c_f(src + 2 * src_step, dst + 2 * dst_step, h, w, crop_h, crop_w, flag);
        }

        static void corner_crop_nc_f(float *src, float *dst, int c, int h, int w, int crop_h, int crop_w, int flag) {
            int src_step = h * w;
            int dst_step = crop_h * crop_w;
            for (int i = 0; i < c; i++) {
                corner_crop_1c_f(src + i * src_step, dst + i * dst_step, h, w, crop_h, crop_w, flag);
            }
        }

        static void corner_crop_1c_u8(uint8_t *src, uint8_t *dst, int h, int w, int crop_h, int crop_w, int flag) {
            // int flag:
            // 0 -- top left corner crop;
            // 1 -- top right corner crop;
            // 2 -- bottom left corner crop;
            // 3 -- bottom right corner crop;
            int top, left;
            switch (flag) {
                case 0:
                    top = 0;
                    left = 0;
                    break;
                case 1:
                    top = 0;
                    left = w - crop_w;
                    break;
                case 2:
                    top = h - crop_h;
                    left = 0;
                    break;
                case 3:
                    top = h - crop_h;
                    left = w - crop_w;
                    break;
                default:
                    cout << "int flag:\n"
                            "   0 -- top left corner crop;\n"
                            "   1 -- top right corner crop;\n"
                            "   2 -- bottom left corner crop;\n"
                            "   3 -- bottom right corner crop;"
                         << endl;
            }
            crop_1c_u8(src, dst, h, w, top, left, crop_h, crop_w);
        }

        static void corner_crop_3c_u8(uint8_t *src, uint8_t *dst, int h, int w, int crop_h, int crop_w, int flag) {
            int src_step = h * w;
            int dst_step = crop_h * crop_w;
            corner_crop_1c_u8(src, dst, h, w, crop_h, crop_w, flag);
            corner_crop_1c_u8(src + src_step, dst + dst_step, h, w, crop_h, crop_w, flag);
            corner_crop_1c_u8(src + 2 * src_step, dst + 2 * dst_step, h, w, crop_h, crop_w, flag);
        }

        static void corner_crop_nc_u8(uint8_t *src, uint8_t *dst, int c, int h, int w, int crop_h, int crop_w, int flag) {
            int src_step = h * w;
            int dst_step = crop_h * crop_w;
            for (int i = 0; i < c; i++) {
                corner_crop_1c_u8(src + i * src_step, dst + i * dst_step, h, w, crop_h, crop_w, flag);
            }
        }

        // reload 1crop functions from center crop
        const static auto &one_crop_1c_f = center_crop_1c_f;
        const static auto &one_crop_3c_f = center_crop_3c_f;
        const static auto &one_crop_nc_f = center_crop_nc_f;
        const static auto &one_crop_1c_u8 = center_crop_1c_u8;
        const static auto &one_crop_3c_u8 = center_crop_3c_u8;
        const static auto &one_crop_nc_u8 = center_crop_nc_u8;

        static void five_crop_1c_f(float *src, float **dst, int h, int w, int crop_h, int crop_w) {
            one_crop_1c_f(src, dst[0], h, w, crop_h, crop_w); // center crop
            corner_crop_1c_f(src, dst[1], h, w, crop_h, crop_w, 0); // top left crop
            corner_crop_1c_f(src, dst[2], h, w, crop_h, crop_w, 1); // top right crop
            corner_crop_1c_f(src, dst[3], h, w, crop_h, crop_w, 2); // bottom left crop
            corner_crop_1c_f(src, dst[4], h, w, crop_h, crop_w, 3); // bottom right crop
        }

        static void five_crop_3c_f(float *src, float **dst, int h, int w, int crop_h, int crop_w) {
            one_crop_3c_f(src, dst[0], h, w, crop_h, crop_w);
            corner_crop_3c_f(src, dst[1], h, w, crop_h, crop_w, 0); // top left crop
            corner_crop_3c_f(src, dst[2], h, w, crop_h, crop_w, 1); // top right crop
            corner_crop_3c_f(src, dst[3], h, w, crop_h, crop_w, 2); // bottom left crop
            corner_crop_3c_f(src, dst[4], h, w, crop_h, crop_w, 3); // bottom right crop
        }

        static void five_crop_nc_f(float *src, float **dst, int c, int h, int w, int crop_h, int crop_w) {
            one_crop_nc_f(src, dst[0], c, h, w, crop_h, crop_w);
            corner_crop_nc_f(src, dst[1], c, h, w, crop_h, crop_w, 0); // top left crop
            corner_crop_nc_f(src, dst[2], c, h, w, crop_h, crop_w, 1); // top right crop
            corner_crop_nc_f(src, dst[3], c, h, w, crop_h, crop_w, 2); // bottom left crop
            corner_crop_nc_f(src, dst[4], c, h, w, crop_h, crop_w, 3); // bottom right crop
        }

        static void five_crop_1c_u8(uint8_t *src, uint8_t **dst, int h, int w, int crop_h, int crop_w) {
            one_crop_1c_u8(src, dst[0], h, w, crop_h, crop_w); // center crop
            corner_crop_1c_u8(src, dst[1], h, w, crop_h, crop_w, 0); // top left crop
            corner_crop_1c_u8(src, dst[2], h, w, crop_h, crop_w, 1); // top right crop
            corner_crop_1c_u8(src, dst[3], h, w, crop_h, crop_w, 2); // bottom left crop
            corner_crop_1c_u8(src, dst[4], h, w, crop_h, crop_w, 3); // bottom right crop
        }

        static void five_crop_3c_u8(uint8_t *src, uint8_t **dst, int h, int w, int crop_h, int crop_w) {
            one_crop_3c_u8(src, dst[0], h, w, crop_h, crop_w);
            corner_crop_3c_u8(src, dst[1], h, w, crop_h, crop_w, 0); // top left crop
            corner_crop_3c_u8(src, dst[2], h, w, crop_h, crop_w, 1); // top right crop
            corner_crop_3c_u8(src, dst[3], h, w, crop_h, crop_w, 2); // bottom left crop
            corner_crop_3c_u8(src, dst[4], h, w, crop_h, crop_w, 3); // bottom right crop
        }

        static void five_crop_nc_u8(uint8_t *src, uint8_t **dst, int c, int h, int w, int crop_h, int crop_w) {
            one_crop_nc_u8(src, dst[0], c, h, w, crop_h, crop_w);
            corner_crop_nc_u8(src, dst[1], c, h, w, crop_h, crop_w, 0); // top left crop
            corner_crop_nc_u8(src, dst[2], c, h, w, crop_h, crop_w, 1); // top right crop
            corner_crop_nc_u8(src, dst[3], c, h, w, crop_h, crop_w, 2); // bottom left crop
            corner_crop_nc_u8(src, dst[4], c, h, w, crop_h, crop_w, 3); // bottom right crop
        }

        static void ten_crop_1c_f(float *src, float **dst, int h, int w, int crop_h, int crop_w, bool vertical_flip = false) {
            five_crop_1c_f(src, dst, h, w, crop_h, crop_w);
            if (vertical_flip) {
                flip_vertical_1c_f(src, src, h, w);
            } else {
                flip_horizontal_1c_f(src, src, h, w);
            }
            five_crop_1c_f(src, dst + 5, h, w, crop_h, crop_w);
        }

        static void ten_crop_3c_f(float *src, float **dst, int h, int w, int crop_h, int crop_w, bool vertical_flip = false) {
            five_crop_3c_f(src, dst, h, w, crop_h, crop_w);
            if (vertical_flip) {
                flip_vertical_3c_f(src, src, h, w);
            } else {
                flip_horizontal_3c_f(src, src, h, w);
            }
            five_crop_3c_f(src, dst + 5, h, w, crop_h, crop_w);
        }

        static void ten_crop_nc_f(float *src, float **dst, int c, int h, int w, int crop_h, int crop_w, bool vertical_flip = false) {
            five_crop_nc_f(src, dst, c, h, w, crop_h, crop_w);
            if (vertical_flip) {
                flip_vertical_nc_f(src, src, c, h, w);
            } else {
                flip_horizontal_nc_f(src, src, c, h, w);
            }
            five_crop_nc_f(src, dst + 5, c, h, w, crop_h, crop_w);
        }

        static void ten_crop_1c_u8(uint8_t *src, uint8_t **dst, int h, int w, int crop_h, int crop_w, bool vertical_flip = false) {
            five_crop_1c_u8(src, dst, h, w, crop_h, crop_w);
            if (vertical_flip) {
                flip_vertical_1c_u8(src, src, h, w);
            } else {
                flip_horizontal_1c_u8(src, src, h, w);
            }
            five_crop_1c_u8(src, dst + 5, h, w, crop_h, crop_w);
        }

        static void ten_crop_3c_u8(uint8_t *src, uint8_t **dst, int h, int w, int crop_h, int crop_w, bool vertical_flip = false) {
            five_crop_3c_u8(src, dst, h, w, crop_h, crop_w);
            if (vertical_flip) {
                flip_vertical_3c_u8(src, src, h, w);
            } else {
                flip_horizontal_3c_u8(src, src, h, w);
            }
            five_crop_3c_u8(src, dst + 5, h, w, crop_h, crop_w);
        }

        static void ten_crop_nc_u8(uint8_t *src, uint8_t **dst, int c, int h, int w, int crop_h, int crop_w, bool vertical_flip = false) {
            five_crop_nc_u8(src, dst, c, h, w, crop_h, crop_w);
            if (vertical_flip) {
                flip_vertical_nc_u8(src, src, c, h, w);
            } else {
                flip_horizontal_nc_u8(src, src, c, h, w);
            }
            five_crop_nc_u8(src, dst + 5, c, h, w, crop_h, crop_w);
        }

        /*
         *  scales a one-channel float image with size [H, W]
         *  calculation can be described as: dst[i] = src[i] * scale + bias
         */
        inline void scale_1c_f(float *src, float *dst, int h, int w, float scale, float bias) {
            int i = 0;
#ifdef ARM
            float32x4_t dup_scale = vld1q_dup_f32(&scale);
            float32x4_t dup_bias = vld1q_dup_f32(&bias);
            for (; i < h * w; i += 4) {
                vst1q_f32(dst + i, vaddq_f32(vmulq_f32(vld1q_f32(src + i), dup_scale), dup_bias));
            }
#endif
            for (; i < h * w; i++) {
                dst[i] = src[i] * scale + bias;
            }
        }

        static void scale_3c_f(float *src, float *dst, int h, int w, float *scale, float *bias) {
            scale_1c_f(src, dst, h, w, scale[0], bias[0]);
            scale_1c_f(src + h * w, dst + h * w, h, w, scale[1], bias[1]);
            scale_1c_f(src + 2 * h * w, dst + 2 * h * w, h, w, scale[2], bias[2]);
        }

        static void scale_nc_f(float *src, float *dst, int c, int h, int w, float *scale, float *bias) {
            for (int i = 0; i < c; i++) {
                scale_1c_f(src + i * h * w, dst + i * h * w, h, w, scale[i], bias[i]);
            }
        }

        inline void reverse_sequence_f(float *src, float *dst, size_t l) {
            if (src == dst) {
                float *head = src;
                float *tail = src + l;
                for (; head < tail;) {
                    tail--;
                    float tmp = *head;
                    *head = *tail;
                    *tail = tmp;
                    head++;
                }
            } else {
                for (int i = 0; i < l; i++) {
                    dst[i] = src[l - 1 - i];
                }
            }
        }

        inline void reverse_sequence_u8(uint8_t *src, uint8_t *dst, size_t l) {
            if (src == dst) {
                uint8_t *head = src;
                uint8_t *tail = src + l;
#ifdef ARM
                for (; head + 8 <= tail - 8;) {
                    tail -= 8;
                    uint8x8_t head_dup = vld1_u8(head);
                    uint8x8_t tail_dup = vld1_u8(tail);
                    uint8x8_t rev_head = vrev64_u8(head_dup);
                    uint8x8_t rev_tail = vrev64_u8(tail_dup);
                    vst1_u8(head, rev_tail);
                    vst1_u8(tail, rev_head);
                    head += 8;
                }
#endif
                for (; head < tail;) {
                    tail--;
                    uint8_t tmp = *head;
                    *head = *tail;
                    *tail = tmp;
                    head++;
                }
            } else {
                int i = 0;
#ifdef ARM
                for (; i < l; i += 8) {
                    vst1_u8(dst + i, vrev64_u8(vld1_u8(src + l - 8 - i)));
                }
#endif
                for (; i < l; i++) {
                    dst[i] = src[l - 1 - i];
                }
            }
        }

        static void reverse_channel_3c_f(float *src, float *dst, int h, int w) {
            int spatial = h * w;
            if (src == dst) {
                auto tmp = new float[spatial];
                memcpy(tmp, src, spatial * sizeof(float));
                memcpy(src, src + 2 * spatial, spatial * sizeof(float));
                memcpy(src + 2 * spatial, tmp, spatial * sizeof(float));
                delete[] tmp;
            } else {
                memcpy(dst, src + 2 * spatial, spatial * sizeof(float));
                memcpy(dst + spatial, src + spatial, spatial * sizeof(float));
                memcpy(dst + 2 * spatial, src, spatial * sizeof(float));
            }
        }

        static void reverse_channel_3c_u8(uint8_t *src, uint8_t *dst, int h, int w) {
            int spatial = h * w;
            if (src == dst) {
                auto tmp = new uint8_t[spatial];
                memcpy(tmp, src, spatial * sizeof(uint8_t));
                memcpy(src, src + 2 * spatial, spatial * sizeof(uint8_t));
                memcpy(src + 2 * spatial, tmp, spatial * sizeof(uint8_t));
                delete[] tmp;
            } else {
                memcpy(dst, src + 2 * spatial, spatial * sizeof(uint8_t));
                memcpy(dst + spatial, src + spatial, spatial * sizeof(uint8_t));
                memcpy(dst + 2 * spatial, src, spatial * sizeof(uint8_t));
            }
        }

        inline void expand_channel_f(float *src, float *dst, int step, int ic, int oc) {
            TFCHECK_GE(ic, 1)
            TFCHECK_GE(oc, ic)
            TFCHECK_GE(step, 1)

            float *src_walk = src;
            float *dst_walk = dst;
            if (oc % ic == 0) {
                for (int i = 0; i < ic; i++) {
                    for (int j = 0; j < oc / ic; j++) {
                        memcpy(dst_walk, src_walk, step * sizeof(float));
                        dst_walk += step;
                    }
                    src_walk += step;
                }
            } else {
                int complete = (oc - oc % ic);
                int last = oc % ic;
                for (int i = 0; i < ic - 1; i++) {
                    for (int j = 0; j < complete / (ic - 1); j++) {
                        memcpy(dst_walk, src_walk, step * sizeof(float));
                        dst_walk += step;
                    }
                    src_walk += step;
                }
                for (int i = 0; i < last; i++) {
                    memcpy(dst_walk, src_walk, step * sizeof(float));
                    dst_walk += step;
                }
            }
        }

        inline void expand_channel_u8(uint8_t *src, uint8_t *dst, int step, int ic, int oc) {
            TFCHECK_GE(ic, 1)
            TFCHECK_GE(oc, ic)
            TFCHECK_GE(step, 1)

            uint8_t *src_walk = src;
            uint8_t *dst_walk = dst;
            if (oc % ic == 0) {
                for (int i = 0; i < ic; i++) {
                    for (int j = 0; j < oc / ic; j++) {
                        memcpy(dst_walk, src_walk, step * sizeof(uint8_t));
                        dst_walk += step;
                    }
                    src_walk += step;
                }
            } else {
                int complete = (oc - oc % ic);
                int last = oc % ic;
                for (int i = 0; i < ic - 1; i++) {
                    for (int j = 0; j < complete / (ic - 1); j++) {
                        memcpy(dst_walk, src_walk, step * sizeof(uint8_t));
                        dst_walk += step;
                    }
                    src_walk += step;
                }
                for (int i = 0; i < last; i++) {
                    memcpy(dst_walk, src_walk, step * sizeof(uint8_t));
                    dst_walk += step;
                }
            }
        }

        inline void const_pad_1c_f(float *src, float *dst, int h, int w, int pad_t, int pad_l, int pad_b, int pad_r,
                            const float v) {
            assert(pad_t >= 0);
            assert(pad_l >= 0);
            assert(pad_b >= 0);
            assert(pad_r >= 0);

            int out_spatial = (h + pad_t + pad_b) * (w + pad_l + pad_r);
            std::fill(dst, dst + out_spatial, v);

            float *src_walk = src;
            float *dst_walk = dst + pad_t * (w + pad_l + pad_r) + pad_l;
            for (int i = 0; i < h; i++) {
                memcpy(dst_walk, src_walk, w * sizeof(float));
                src_walk += w;
                dst_walk += (w + pad_l + pad_r);
            }
        }

        static void const_pad_3c_f(float *src, float *dst, int h, int w, int pad_t, int pad_l, int pad_b, int pad_r,
                            const float v) {
            int in_spatial = h * w;
            int out_spatial = (h + pad_t + pad_b) * (w + pad_l + pad_r);
            const_pad_1c_f(src, dst, h, w, pad_t, pad_l, pad_b, pad_r, v);
            const_pad_1c_f(src + in_spatial, dst + out_spatial, h, w, pad_t, pad_l, pad_b, pad_r, v);
            const_pad_1c_f(src + 2 * in_spatial, dst + 2 * out_spatial, h, w, pad_t, pad_l, pad_b, pad_r, v);
        }

        static void const_pad_nc_f(float *src, float *dst, int h, int w, int c, int pad_t, int pad_l, int pad_b, int pad_r,
                            const float v) {
            int in_spatial = h * w;
            int out_spatial = (h + pad_t + pad_b) * (w + pad_l + pad_r);
            for (int i = 0; i < c; i++) {
                const_pad_1c_f(src + i * in_spatial, dst + i * out_spatial, h, w, pad_t, pad_l, pad_b, pad_r, v);
            }
        }

        inline void const_pad_1c_u8(uint8_t *src, uint8_t *dst, int h, int w, int pad_t, int pad_l, int pad_b, int pad_r,
                             const uint8_t v) {
            assert(pad_t >= 0);
            assert(pad_l >= 0);
            assert(pad_b >= 0);
            assert(pad_r >= 0);

            int out_spatial = (h + pad_t + pad_b) * (w + pad_l + pad_r);
            memset(dst, v, out_spatial);

            uint8_t *src_walk = src;
            uint8_t *dst_walk = dst + pad_t * (w + pad_l + pad_r) + pad_l;
            for (int i = 0; i < h; i++) {
                memcpy(dst_walk, src_walk, w * sizeof(uint8_t));
                src_walk += w;
                dst_walk += (w + pad_l + pad_r);
            }
        }

        static void const_pad_3c_u8(uint8_t *src, uint8_t *dst, int h, int w, int pad_t, int pad_l, int pad_b, int pad_r,
                             const uint8_t v) {
            int in_spatial = h * w;
            int out_spatial = (h + pad_t + pad_b) * (w + pad_l + pad_r);
            const_pad_1c_u8(src, dst, h, w, pad_t, pad_l, pad_b, pad_r, v);
            const_pad_1c_u8(src + in_spatial, dst + out_spatial, h, w, pad_t, pad_l, pad_b, pad_r, v);
            const_pad_1c_u8(src + 2 * in_spatial, dst + 2 * out_spatial, h, w, pad_t, pad_l, pad_b, pad_r, v);
        }

        static void const_pad_nc_u8(uint8_t *src, uint8_t *dst, int h, int w, int c, int pad_t, int pad_l, int pad_b, int pad_r,
                        const uint8_t v) {
            int in_spatial = h * w;
            int out_spatial = (h + pad_t + pad_b) * (w + pad_l + pad_r);
            for (int i = 0; i < c; i++) {
                const_pad_1c_u8(src + i * in_spatial, dst + i * out_spatial, h, w, pad_t, pad_l, pad_b, pad_r, v);
            }
        }

        inline void reflect_pad_1c_f(float *src, float *dst, int h, int w, int pad_t, int pad_l, int pad_b, int pad_r) {
            int i_start_x = std::max(0, -pad_l);
            int i_start_y = std::max(0, -pad_t);
            int o_start_x = std::max(0, pad_l);
            int o_start_y = std::max(0, pad_t);

            // fill center of output with input data
            float *src_walk = src;
            float *dst_walk = dst + pad_t * (w + pad_l + pad_r);
            for (int index = 0; index < h; index++) {
                memcpy(dst_walk + pad_l, src_walk, w * sizeof(float));
                src_walk += w;
                dst_walk += (w + pad_l + pad_r);
            }
            // fill padding
            int ip_x, ip_y;
            // top part
            int input_h = h;
            int input_w = w;
            int output_h = h + pad_t + pad_b;
            int output_w = w + pad_l + pad_r;
            for (int i = 0; i < pad_t; i++) {
                for (int j = 0; j < output_w; j++) {
                    if (j < pad_l) {
                        ip_x = pad_l * 2 - j;
                    } else if (j >= pad_l && j < input_w + pad_l) {
                        ip_x = j;
                    } else {
                        ip_x = (input_w + pad_l - 1) * 2 - j;
                    }
                    ip_x = ip_x - o_start_x + i_start_x;

                    if (i < pad_t) {
                        ip_y = pad_t * 2 - i;
                    } else if (i >= pad_t && i < input_h + pad_t) {
                        ip_y = i;
                    } else {
                        ip_y = (input_h + pad_t - 1) * 2 - i;
                    }
                    ip_y = ip_y - o_start_y + i_start_y;

                    float *dest_p = dst + i * output_w + j;
                    float *src_p = src + ip_y * input_w + ip_x;
                    *dest_p = *src_p;
                }
            }
            // middle part
            for (int i = pad_t; i < input_h + pad_t; i++) {
                for (int j = 0; j < output_w; j++) {
                    if (j < pad_l) {
                        ip_x = pad_l * 2 - j;
                    } else if (j >= pad_l && j < input_w + pad_l) {
                        // skip where output copy form input
                        j += (input_w - 1);
                        continue;
                    } else {
                        ip_x = (input_w + pad_l - 1) * 2 - j;
                    }
                    ip_x = ip_x - o_start_x + i_start_x;

                    if (i < pad_t) {
                        ip_y = pad_t * 2 - i;
                    } else if (i >= pad_t && i < input_h + pad_t) {
                        ip_y = i;
                    } else {
                        ip_y = (input_h + pad_t - 1) * 2 - i;
                    }
                    ip_y = ip_y - o_start_y + i_start_y;

                    float *dest_p = dst + i * output_w + j;
                    float *src_p = src + ip_y * input_w + ip_x;
                    *dest_p = *src_p;
                }
            }
            // bottom part
            for (int i = input_h + pad_t; i < output_h; i++) {
                for (int j = 0; j < output_w; j++) {
                    if (j < pad_l) {
                        ip_x = pad_l * 2 - j;
                    } else if (j >= pad_l && j < input_w + pad_l) {
                        ip_x = j;
                    } else {
                        ip_x = (input_w + pad_l - 1) * 2 - j;
                    }
                    ip_x = ip_x - o_start_x + i_start_x;

                    if (i < pad_t) {
                        ip_y = pad_t * 2 - i;
                    } else if (i >= pad_t && i < input_h + pad_t) {
                        ip_y = i;
                    } else {
                        ip_y = (input_h + pad_t - 1) * 2 - i;
                    }
                    ip_y = ip_y - o_start_y + i_start_y;

                    float *dest_p = dst + i * output_w + j;
                    float *src_p = src + ip_y * input_w + ip_x;
                    *dest_p = *src_p;
                }
            }
        }

        static void reflect_pad_3c_f(float *src, float *dst, int h, int w, int pad_t, int pad_l, int pad_b, int pad_r) {
            int in_spatial = h * w;
            int out_spatial = (h + pad_t + pad_b) * (w + pad_l + pad_r);
            reflect_pad_1c_f(src, dst, h, w, pad_t, pad_l, pad_b, pad_r);
            reflect_pad_1c_f(src + in_spatial, dst + out_spatial, h, w, pad_t, pad_l, pad_b, pad_r);
            reflect_pad_1c_f(src + 2 * in_spatial, dst + 2 * out_spatial, h, w, pad_t, pad_l, pad_b, pad_r);
        }

        static void reflect_pad_nc_f(float *src, float *dst, int h, int w, int c, int pad_t, int pad_l, int pad_b, int pad_r) {
            int in_spatial = h * w;
            int out_spatial = (h + pad_t + pad_b) * (w + pad_l + pad_r);
            for (int i = 0; i < c; i++) {
                reflect_pad_1c_f(src + i * in_spatial, dst + i * out_spatial, h, w, pad_t, pad_l, pad_b, pad_r);
            }
        }

        inline void reflect_pad_1c_u8(uint8_t *src, uint8_t *dst, int h, int w, int pad_t, int pad_l, int pad_b, int pad_r) {
            int i_start_x = std::max(0, -pad_l);
            int i_start_y = std::max(0, -pad_t);
            int o_start_x = std::max(0, pad_l);
            int o_start_y = std::max(0, pad_t);

            // fill center of output with input data
            uint8_t *src_walk = src;
            uint8_t *dst_walk = dst + pad_t * (w + pad_l + pad_r);
            for (int index = 0; index < h; index++) {
                memcpy(dst_walk + pad_l, src_walk, w * sizeof(uint8_t));
                src_walk += w;
                dst_walk += (w + pad_l + pad_r);
            }
            // fill padding
            int ip_x, ip_y;
            // top part
            int input_h = h;
            int input_w = w;
            int output_h = h + pad_t + pad_b;
            int output_w = w + pad_l + pad_r;
            for (int i = 0; i < pad_t; i++) {
                for (int j = 0; j < output_w; j++) {
                    if (j < pad_l) {
                        ip_x = pad_l * 2 - j;
                    } else if (j >= pad_l && j < input_w + pad_l) {
                        ip_x = j;
                    } else {
                        ip_x = (input_w + pad_l - 1) * 2 - j;
                    }
                    ip_x = ip_x - o_start_x + i_start_x;

                    if (i < pad_t) {
                        ip_y = pad_t * 2 - i;
                    } else if (i >= pad_t && i < input_h + pad_t) {
                        ip_y = i;
                    } else {
                        ip_y = (input_h + pad_t - 1) * 2 - i;
                    }
                    ip_y = ip_y - o_start_y + i_start_y;

                    uint8_t *dest_p = dst + i * output_w + j;
                    uint8_t *src_p = src + ip_y * input_w + ip_x;
                    *dest_p = *src_p;
                }
            }
            // middle part
            for (int i = pad_t; i < input_h + pad_t; i++) {
                for (int j = 0; j < output_w; j++) {
                    if (j < pad_l) {
                        ip_x = pad_l * 2 - j;
                    } else if (j >= pad_l && j < input_w + pad_l) {
                        // skip where output copy form input
                        j += (input_w - 1);
                        continue;
                    } else {
                        ip_x = (input_w + pad_l - 1) * 2 - j;
                    }
                    ip_x = ip_x - o_start_x + i_start_x;

                    if (i < pad_t) {
                        ip_y = pad_t * 2 - i;
                    } else if (i >= pad_t && i < input_h + pad_t) {
                        ip_y = i;
                    } else {
                        ip_y = (input_h + pad_t - 1) * 2 - i;
                    }
                    ip_y = ip_y - o_start_y + i_start_y;

                    uint8_t *dest_p = dst + i * output_w + j;
                    uint8_t *src_p = src + ip_y * input_w + ip_x;
                    *dest_p = *src_p;
                }
            }
            // bottom part
            for (int i = input_h + pad_t; i < output_h; i++) {
                for (int j = 0; j < output_w; j++) {
                    if (j < pad_l) {
                        ip_x = pad_l * 2 - j;
                    } else if (j >= pad_l && j < input_w + pad_l) {
                        ip_x = j;
                    } else {
                        ip_x = (input_w + pad_l - 1) * 2 - j;
                    }
                    ip_x = ip_x - o_start_x + i_start_x;

                    if (i < pad_t) {
                        ip_y = pad_t * 2 - i;
                    } else if (i >= pad_t && i < input_h + pad_t) {
                        ip_y = i;
                    } else {
                        ip_y = (input_h + pad_t - 1) * 2 - i;
                    }
                    ip_y = ip_y - o_start_y + i_start_y;

                    uint8_t *dest_p = dst + i * output_w + j;
                    uint8_t *src_p = src + ip_y * input_w + ip_x;
                    *dest_p = *src_p;
                }
            }
        }

        static void reflect_pad_3c_u8(uint8_t *src, uint8_t *dst, int h, int w, int pad_t, int pad_l, int pad_b, int pad_r) {
            int in_spatial = h * w;
            int out_spatial = (h + pad_t + pad_b) * (w + pad_l + pad_r);
            reflect_pad_1c_u8(src, dst, h, w, pad_t, pad_l, pad_b, pad_r);
            reflect_pad_1c_u8(src + in_spatial, dst + out_spatial, h, w, pad_t, pad_l, pad_b, pad_r);
            reflect_pad_1c_u8(src + 2 * in_spatial, dst + 2 * out_spatial, h, w, pad_t, pad_l, pad_b, pad_r);
        }

        static void reflect_pad_nc_u8(uint8_t *src, uint8_t *dst, int h, int w, int c, int pad_t, int pad_l, int pad_b, int pad_r) {
            int in_spatial = h * w;
            int out_spatial = (h + pad_t + pad_b) * (w + pad_l + pad_r);
            for (int i = 0; i < c; i++) {
                reflect_pad_1c_u8(src + i * in_spatial, dst + i * out_spatial, h, w, pad_t, pad_l, pad_b, pad_r);
            }
        }

        inline void replicate_pad_1c_f(float *src, float *dst, int h, int w, int pad_t, int pad_l, int pad_b, int pad_r) {
            int iStartX = std::max(0, -pad_l);
            int iStartY = std::max(0, -pad_t);
            int oStartX = std::max(0, pad_l);
            int oStartY = std::max(0, pad_t);

            // fill center of output with input data
            float *src_walk = src;
            float *dst_walk = dst + pad_t * (w + pad_l + pad_r);
            for (int index = 0; index < h; index++) {
                memcpy(dst_walk + pad_l, src_walk, w * sizeof(float));
                src_walk += w;
                dst_walk += (w + pad_l + pad_r);
            }
            // fill padding
            int ip_x, ip_y;
            // top part
            int input_h = h;
            int input_w = w;
            int output_h = h + pad_t + pad_b;
            int output_w = w + pad_l + pad_r;
            for (int i = 0; i < pad_t; i++) {
                for (int j = 0; j < output_w; j++) {
                    if (j < pad_l) {
                        ip_x = pad_l;
                    } else if (j >= pad_l && j < input_w + pad_l) {
                        ip_x = j;
                    } else {
                        ip_x = input_w + pad_l - 1;
                    }
                    ip_x = ip_x - oStartX + iStartX;

                    if (i < pad_t) {
                        ip_y = pad_t;
                    } else if (i >= pad_t && i < input_h + pad_t) {
                        ip_y = i;
                    } else {
                        ip_y = input_h + pad_t - 1;
                    }
                    ip_y = ip_y - oStartY + iStartY;

                    float *dest_p = dst + i * output_w + j;
                    float *src_p = src + ip_y * input_w + ip_x;
                    *dest_p = *src_p;
                }
            }
            // middle part
            for (int i = pad_t; i < input_h + pad_t; i++) {
                for (int j = 0; j < output_w; j++) {
                    if (j < pad_l) {
                        ip_x = pad_l;
                    } else if (j >= pad_l && j < input_w + pad_l) {
                        // skip where output copy form input
                        j += (input_w - 1);
                        continue;
                    } else {
                        ip_x = input_w + pad_l - 1;
                    }
                    ip_x = ip_x - oStartX + iStartX;

                    if (i < pad_t) {
                        ip_y = pad_t;
                    } else if (i >= pad_t && i < input_h + pad_t) {
                        ip_y = i;
                    } else {
                        ip_y = input_h + pad_t - 1;
                    }
                    ip_y = ip_y - oStartY + iStartY;

                    float *dest_p = dst + i * output_w + j;
                    float *src_p = src + ip_y * input_w + ip_x;
                    *dest_p = *src_p;
                }
            }
            // bottom part
            for (int i = input_h + pad_t; i < output_h; i++) {
                for (int j = 0; j < output_w; j++) {
                    if (j < pad_l) {
                        ip_x = pad_l;
                    } else if (j >= pad_l && j < input_w + pad_l) {
                        ip_x = j;
                    } else {
                        ip_x = input_w + pad_l - 1;
                    }
                    ip_x = ip_x - oStartX + iStartX;

                    if (i < pad_t) {
                        ip_y = pad_t;
                    } else if (i >= pad_t && i < input_h + pad_t) {
                        ip_y = i;
                    } else {
                        ip_y = input_h + pad_t - 1;
                    }
                    ip_y = ip_y - oStartY + iStartY;

                    float *dest_p = dst + i * output_w + j;
                    float *src_p = src + ip_y * input_w + ip_x;
                    *dest_p = *src_p;
                }
            }
        }

        static void replicate_pad_3c_f(float *src, float *dst, int h, int w, int pad_t, int pad_l, int pad_b, int pad_r) {
            int in_spatial = h * w;
            int out_spatial = (h + pad_t + pad_b) * (w + pad_l + pad_r);
            replicate_pad_1c_f(src, dst, h, w, pad_t, pad_l, pad_b, pad_r);
            replicate_pad_1c_f(src + in_spatial, dst + out_spatial, h, w, pad_t, pad_l, pad_b, pad_r);
            replicate_pad_1c_f(src + 2 * in_spatial, dst + 2 * out_spatial, h, w, pad_t, pad_l, pad_b, pad_r);
        }

        static void replicate_pad_nc_f(float *src, float *dst, int h, int w, int c, int pad_t, int pad_l, int pad_b, int pad_r) {
            int in_spatial = h * w;
            int out_spatial = (h + pad_t + pad_b) * (w + pad_l + pad_r);
            for (int i = 0; i < c; i++) {
                replicate_pad_1c_f(src + i * in_spatial, dst + i * out_spatial, h, w, pad_t, pad_l, pad_b, pad_r);
            }
        }

        inline void replicate_pad_1c_u8(uint8_t *src, uint8_t *dst, int h, int w, int pad_t, int pad_l, int pad_b, int pad_r) {
            int iStartX = std::max(0, -pad_l);
            int iStartY = std::max(0, -pad_t);
            int oStartX = std::max(0, pad_l);
            int oStartY = std::max(0, pad_t);

            // fill center of output with input data
            uint8_t *src_walk = src;
            uint8_t *dst_walk = dst + pad_t * (w + pad_l + pad_r);
            for (int index = 0; index < h; index++) {
                memcpy(dst_walk + pad_l, src_walk, w * sizeof(uint8_t));
                src_walk += w;
                dst_walk += (w + pad_l + pad_r);
            }
            // fill padding
            int ip_x, ip_y;
            // top part
            int input_h = h;
            int input_w = w;
            int output_h = h + pad_t + pad_b;
            int output_w = w + pad_l + pad_r;
            for (int i = 0; i < pad_t; i++) {
                for (int j = 0; j < output_w; j++) {
                    if (j < pad_l) {
                        ip_x = pad_l;
                    } else if (j >= pad_l && j < input_w + pad_l) {
                        ip_x = j;
                    } else {
                        ip_x = input_w + pad_l - 1;
                    }
                    ip_x = ip_x - oStartX + iStartX;

                    if (i < pad_t) {
                        ip_y = pad_t;
                    } else if (i >= pad_t && i < input_h + pad_t) {
                        ip_y = i;
                    } else {
                        ip_y = input_h + pad_t - 1;
                    }
                    ip_y = ip_y - oStartY + iStartY;

                    uint8_t *dest_p = dst + i * output_w + j;
                    uint8_t *src_p = src + ip_y * input_w + ip_x;
                    *dest_p = *src_p;
                }
            }
            // middle part
            for (int i = pad_t; i < input_h + pad_t; i++) {
                for (int j = 0; j < output_w; j++) {
                    if (j < pad_l) {
                        ip_x = pad_l;
                    } else if (j >= pad_l && j < input_w + pad_l) {
                        // skip where output copy form input
                        j += (input_w - 1);
                        continue;
                    } else {
                        ip_x = input_w + pad_l - 1;
                    }
                    ip_x = ip_x - oStartX + iStartX;

                    if (i < pad_t) {
                        ip_y = pad_t;
                    } else if (i >= pad_t && i < input_h + pad_t) {
                        ip_y = i;
                    } else {
                        ip_y = input_h + pad_t - 1;
                    }
                    ip_y = ip_y - oStartY + iStartY;

                    uint8_t *dest_p = dst + i * output_w + j;
                    uint8_t *src_p = src + ip_y * input_w + ip_x;
                    *dest_p = *src_p;
                }
            }
            // bottom part
            for (int i = input_h + pad_t; i < output_h; i++) {
                for (int j = 0; j < output_w; j++) {
                    if (j < pad_l) {
                        ip_x = pad_l;
                    } else if (j >= pad_l && j < input_w + pad_l) {
                        ip_x = j;
                    } else {
                        ip_x = input_w + pad_l - 1;
                    }
                    ip_x = ip_x - oStartX + iStartX;

                    if (i < pad_t) {
                        ip_y = pad_t;
                    } else if (i >= pad_t && i < input_h + pad_t) {
                        ip_y = i;
                    } else {
                        ip_y = input_h + pad_t - 1;
                    }
                    ip_y = ip_y - oStartY + iStartY;

                    uint8_t *dest_p = dst + i * output_w + j;
                    uint8_t *src_p = src + ip_y * input_w + ip_x;
                    *dest_p = *src_p;
                }
            }
        }

        static void replicate_pad_3c_u8(uint8_t *src, uint8_t *dst, int h, int w, int pad_t, int pad_l, int pad_b, int pad_r) {
            int in_spatial = h * w;
            int out_spatial = (h + pad_t + pad_b) * (w + pad_l + pad_r);
            replicate_pad_1c_u8(src, dst, h, w, pad_t, pad_l, pad_b, pad_r);
            replicate_pad_1c_u8(src + in_spatial, dst + out_spatial, h, w, pad_t, pad_l, pad_b, pad_r);
            replicate_pad_1c_u8(src + 2 * in_spatial, dst + 2 * out_spatial, h, w, pad_t, pad_l, pad_b, pad_r);
        }

        static void replicate_pad_nc_u8(uint8_t *src, uint8_t *dst, int h, int w, int c, int pad_t, int pad_l, int pad_b, int pad_r) {
            int in_spatial = h * w;
            int out_spatial = (h + pad_t + pad_b) * (w + pad_l + pad_r);
            for (int i = 0; i < c; i++) {
                replicate_pad_1c_u8(src + i * in_spatial, dst + i * out_spatial, h, w, pad_t, pad_l, pad_b, pad_r);
            }
        }

        inline void fill_zero_f(float *dst, std::size_t l) {
            memset(dst, 0, l * sizeof(float));
        }

        inline void fill_zero_u8(uint8_t *dst, std::size_t l) {
            memset(dst, 0, l * sizeof(uint8_t));
        }

        inline void fill_ones_f(float *dst, std::size_t l) {
            std::fill(dst, dst + l, 1.f);
        }

        inline void fill_constant_u8(uint8_t *dst, std::size_t l, const uint8_t var) {
            memset(dst, var, l);
        }

        inline void fill_random_f(float *dst, std::size_t l, float min = 0.f, float max = 1.f) {
            if (min == 0.f && max == 1.f) {
                for (int i = 0; i < l; i++) {
                    dst[i] = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
                }
                return;
            }
            for (int i = 0; i < l; i++) {
                dst[i] = min + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / (max - min)));
            }
        }

        inline void fill_random_u8(uint8_t *dst, std::size_t l) {
            for (int i = 0; i < l; i++) {
                dst[i] = static_cast<uint8_t>(rand() % numeric_limits<uint8_t>::max());
            }
        }

        inline void fill_randn_f(float *dst, std::size_t l, float mean = 0.f, float var = 1.f) {
            std::default_random_engine e;
            std::normal_distribution<float> n(mean, var);
            for (std::size_t i = 0; i < l; i++) {
                dst[i] = n(e);
            }
        }

        inline void fill_eyes_2d_f(float *dst, std::size_t size) {
            for (int i = 0; i < size; i++) {
                for (int j = 0; j < size; j++) {
                    dst[i * size + j] = i == j ? 1.f : 0.f;
                }
            }
        }

        inline void fill_eyes_nd_f(float *dst, int n, std::size_t size) {
            fill_zero_f(dst, pow(size, n));

            std::size_t loc = (pow(size, n) - 1) / (size - 1);
            for (int i = 0; i < size; i++) {
                dst[i * loc] = 1.f;
            }
        }
    }
}

#endif //TFDL_TRANSFORMS_HPP
