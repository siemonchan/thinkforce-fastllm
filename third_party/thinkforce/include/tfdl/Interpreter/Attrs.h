//
// Created by siemon on 3/26/20.
//

#ifndef PROJECT_ATTRS_H
#define PROJECT_ATTRS_H

#include <vector>
#include <string>

using namespace std;

namespace tfdl {
    namespace attr {
        typedef void *ParamType;

        enum Method {
            SUM = 0,
            SUB = 1,
            PROD = 2,
            DIV = 3,
            MAX = 4,
            MIN = 5,
            GREATER = 6,
            LESS = 7,
            GREATEREQUAL = 8,
            LESSEQUAL = 9,
            EQUAL = 10
        };

        enum Interpolation {
            DUPLICATE = 0,
            BILINEAR = 1,
            NEARSET = 2,
            BICUBIC = 3
        };

        struct BinaryOpParam {
            Method method;
            bool relu;

            explicit BinaryOpParam(Method m, bool r = false) {
                method = m;
                relu = r;
            }
        };

        struct BroadCastOpParam {
            Method method;
            bool relu;

            explicit BroadCastOpParam(Method m, bool r = false) {
                method = m;
                relu = r;
            }
        };

        struct ConcatParam {
            int axis;

            explicit ConcatParam(int a = 1) {
                axis = a;
            }
        };

        struct ConvolutionParam {
            int outputChannels;
            int kernelH;
            int kernelW;
            int strideH;
            int strideW;
            int padT;
            int padL;
            int padB;
            int padR;
            int dilationH;
            int dilationW;
            int group;
            bool relu;
            bool reverseBack;

            ConvolutionParam(int o, int k_h, int k_w, int s_h, int s_w, int p_t, int p_l, int p_b, int p_r,
                             int d_h = 1, int d_w = 1, int g = 1, bool r = false, bool rb = false) {
                outputChannels = o;
                kernelH = k_h;
                kernelW = k_w;
                strideH = s_h;
                strideW = s_w;
                padT = p_t;
                padL = p_l;
                padB = p_b;
                padR = p_r;
                dilationH = d_h;
                dilationW = d_w;
                group = g;
                relu = r;
                reverseBack = rb;
            }
        };

        struct FusedActivationParam {
            bool hard;

            explicit FusedActivationParam(bool h = false) {
                hard = h;
            }
        };

        struct FlattenParam {
            int startAxis;
            int endAxis;

            FlattenParam(int st, int ed) {
                startAxis = st;
                endAxis = ed;
            }
        };

        struct InnerProductParam {
            int outputChannels;
            bool relu;

            explicit InnerProductParam(int o, bool r = false) {
                outputChannels = o;
                relu = r;
            }
        };

        struct LayerNormParam {
            int axis;

            explicit LayerNormParam(int a = 1) {
                axis = a;
            }
        };

        struct LRNParam {
            int size;
            float alpha;
            float beta;

            LRNParam(int s, float a, float b) {
                size = s;
                alpha = a;
                beta = b;
            }
        };

        struct MatMulParam {
            bool transA;
            bool transB;
            float alpha;

            explicit MatMulParam(bool t_a = false, bool t_b = false, float alp = 1.f) {
                transA = t_a;
                transB = t_b;
                alpha = alp;
            }
        };

        struct MeanParam {
            vector<int> axis;

            explicit MeanParam(const vector<int> &a) {
                axis = a;
            }
        };

        struct PermuteParam {
            vector<int> orders;

            explicit PermuteParam(const vector<int> &o) {
                orders = o;
            }
        };

        struct PoolingParam {
            int kernelH;
            int kernelW;
            int strideH;
            int strideW;
            int padT;
            int padL;
            int padB;
            int padR;
            string poolingType;
            bool ceilMode;
            bool globalPooling;

            PoolingParam(int k_h, int k_w, int s_h, int s_w, int p_t, int p_l, int p_b, int p_r,
                         const string &type = "AVE", bool c_m = true, bool global = false) {
                kernelH = k_h;
                kernelW = k_w;
                strideH = s_h;
                strideW = s_w;
                padT = p_t;
                padL = p_l;
                padB = p_b;
                padR = p_r;
                poolingType = type;
                ceilMode = c_m;
                globalPooling = global;
            }
        };

        struct PowerParam {
            float shift;
            float scale;
            float power;

            PowerParam(float sh, float sc, float p) {
                shift = sh;
                scale = sc;
                power = p;
            }
        };

        struct ReshapeDataParam {
            vector<int> shapes;

            explicit ReshapeDataParam(const vector<int> &s) {
                shapes = s;
            }
        };

        struct SoftmaxParam {
            int axis;

            explicit SoftmaxParam(int a = 1) {
                axis = a;
            }
        };

        struct ShufflePixelParam {
            float scale;

            explicit ShufflePixelParam(float s) {
                scale = s;
            };
        };

        struct TileParam {
            vector<int> multiples;

            explicit TileParam(const vector<int> &m) {
                multiples = m;
            }
        };

        struct UpsampleParam {
            int stride;
            int outputHeight;
            int outputWidth;
            Interpolation operation;
            bool alignCorners;
            bool preserveAspectRatio;

            explicit UpsampleParam(int s, Interpolation op = DUPLICATE, bool align = false, bool pre = false) {
                stride = s;
                operation = op;
                alignCorners = align;
                preserveAspectRatio = pre;

                // useless part
                outputHeight = 0;
                outputWidth = 0;
            }

            UpsampleParam(int out_h, int out_w, Interpolation op = DUPLICATE, bool align = false, bool pre = false) {
                outputHeight = out_h;
                outputWidth = out_w;
                operation = op;
                alignCorners = align;
                preserveAspectRatio = pre;

                // useless part
                stride = 0;
            }
        };

        struct ZeroPaddingParam {
            vector<int> pad;

            explicit ZeroPaddingParam(const vector<int> &p) {
                pad = p;
            }
        };
    }
}

#endif //PROJECT_ATTRS_H
