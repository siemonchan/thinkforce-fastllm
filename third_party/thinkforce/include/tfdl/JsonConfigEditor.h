//
// Created by siemon on 2/12/19.
//

#ifndef PROJECT_JSONCONFIGEDITOR_H
#define PROJECT_JSONCONFIGEDITOR_H

#include "json11.hpp"
#include <iostream>
#include <sstream>
#include <float.h>

using namespace std;

namespace tfdl {
    string basicLayerConfig(string layerName,
                            string layerType,
                            vector<string> inputs,
                            vector<string> outputs,
                            string inputDataType = "",
                            string outputDataType = "",
                            string weightDataType = "");

    const json11::Json BasicLayerJson(string layerName,
                                      string layerType,
                                      vector<string> inputs,
                                      vector<string> outputs);

    const json11::Json InputLayerJson(string layerName,
                                      string outputName,
                                      vector<int> inputShape,
                                      float scale,
                                      vector<int> meanValue,
                                      bool forceGrey);

    const json11::Json BatchNormalJson(string layerName,
                                       string inputName,
                                       string outputName);

    const json11::Json ChannelWiseJson(string layerName,
                                       pair<string, string> inputName,
                                       string outputName,
                                       string Operation);

    const json11::Json ConcatJson(string layerName,
                                  vector<string> inputName,
                                  string outputName,
                                  int axis);

    const json11::Json ConvolutionJson(string layerName,
                                       string inputName,
                                       string outputName,
                                       int outputChannels,
                                       int kernelSize,
                                       int stride,
                                       int pad,
                                       int dilation = 1,
                                       int group = 1,
                                       int crossGroup = 1,
                                       bool hasBias = false,
                                       bool int32Bias = false,
                                       bool reverseBack = false,
                                       bool relu = false,
                                       float negativeSlope = 0.f,
                                       float threshold = -1,
                                       int fixZero = 0,
                                       int originalHeight = 0,
                                       int batchPadding = 0);

    const json11::Json ConvolutionJson(string layerName,
                                       string inputName,
                                       string outputName,
                                       int outputChannels,
                                       pair<int, int> kernel,
                                       pair<int, int> stride,
                                       pair<int, int> pad,
                                       int dilation = 1,
                                       int group = 1,
                                       bool hasBias = false,
                                       bool int32Bias = false,
                                       bool reverseBack = false,
                                       bool relu = false,
                                       float negativeSlope = 0.f,
                                       float threshold = -1,
                                       int fixZero = 0,
                                       int originalHeight = 0,
                                       int batchPadding = 0,
                                       std::string outputDataType = "");

    const json11::Json ConvolutionJson(string layerName,
                                       string inputName,
                                       string outputName,
                                       int outputChannels,
                                       pair <int, int> kernel,
                                       pair <int, int> stride,
                                       vector<int> pad,
                                       int dilation = 1,
                                       int group = 1,
                                       bool hasBias = false,
                                       bool int32Bias = false,
                                       bool reverseBack = false,
                                       bool relu = false,
                                       float negativeSlope = 0.f,
                                       float threshold = -1,
                                       int fixZero = 0,
                                       int originalHeight = 0,
                                       int batchPadding = 0,
                                       std::string outputDataType = "");

    const json11::Json ConvolutionJson(string layerName,
                                       string inputName,
                                       string secondInputName,
                                       string outputName,
                                       int outputChannels,
                                       pair<int, int> kernel,
                                       int stride,
                                       pair<int, int> pad,
                                       int dilation = 1,
                                       int group = 1,
                                       bool hasBias = false,
                                       bool int32Bias = false,
                                       bool reverseBack = false,
                                       bool relu = false,
                                       float negativeSlope = 0.f,
                                       float threshold = -1,
                                       int fixZero = 0,
                                       int originalHeight = 0,
                                       int batchPadding = 0,
                                       std::string outputDataType = "");

    /*
    const json11::Json ConvolutionJson(string layerName,
                                       string inputName,
                                       vector<string> outputName,
                                       int outputChannels1,
                                       int outputChannels2,
                                       pair<int, int> kernel,
                                       int stride,
                                       pair<int, int> pad,
                                       int dilation,
                                       int group,
                                       bool hasBias,
                                       bool int32Bias,
                                       bool reverseBack,
                                       bool relu,
                                       float negativeSlope,
                                       float threshold,
                                       int fixZero,
                                       int originalHeight,
                                       int batchPadding);
                                       */

    const json11::Json CropJson(string layerName,
                                string inputName,
                                string outputName,
                                int axis,
                                vector<int> offset);

    const json11::Json DeconvolutionJson(string layerName,
                                         string inputName,
                                         string outputName,
                                         int outputChannels,
                                         pair<int, int> kernel,
                                         int stride,
                                         pair<int, int> pad,
                                         pair<int, int> output_padding,
                                         int dilation = 1,
                                         int group = 1,
                                         bool hasBias = false,
                                         bool int32Bias = false,
                                         bool reverseBack = false,
                                         bool relu = false,
                                         float negativeSlope = 0.f,
                                         int fixZero = 0);

    const json11::Json DetectionOutputJson(string layerName,
                                           string inputName,
                                           string outputName,
                                           int num_classes,
                                           bool share_loaction,
                                           int background_label_id,
                                           bool variance_encoded_in_target,
                                           int keep_top_k,
                                           float confidence_threshold,
                                           float nms_threshold,
                                           int top_k,
                                           bool switch_channels,
                                           bool clip_bbox);

    const json11::Json DropoutJson(string layerName,
                                   string inputName,
                                   string outputName);

    const json11::Json EltwiseJson(string layerName,
                                   pair<string, string> inputName,
                                   string outputName,
                                   string Operation,
                                   bool relu = false, bool reluX = false, int reluMax = 0);

    const json11::Json FlattenJson(string layerName,
                                   string inputName,
                                   string outputName,
                                   int startAxis,
                                   int endAxis);

    const json11::Json FlipJson(string layerName,
                                string inputName,
                                string outputName);

    const json11::Json GruJson(string layerName,
                               string inputName,
                               string outputName,
                               int numOutput,
                               bool returnSequence,
                               string recurrentActivation);

    const json11::Json SwishJson(string layerName,
                                 string inputName,
                                 string outputName,
                                 bool hard = false);

    const json11::Json InnerProductJson(string layerName,
                                        string inputName,
                                        string outputName,
                                        int outputChannels,
                                        bool fromConvolution = false,
                                        bool hasBias = false,
                                        bool int32Bias = false,
                                        bool relu = false,
                                        int axis = 1,
                                        std::string outputDataType = "");

    const json11::Json L2_NormalizationJson(string layerName,
                                            string inputName,
                                            string outputName);

    const json11::Json LrnJson(string layerName,
                               string inputName,
                               string outputName,
                               int size,
                               float alpha,
                               float beta);

    const json11::Json LstmJson(string layerName,
                                string inputName,
                                string outputName,
                                int numOutput,
                                bool returnSequence,
                                string recurrentActivation);

    const json11::Json MergeBatchJson(string layerName,
                                      string inputName,
                                      string outputName,
                                      bool reverse,
                                      int padding,
                                      int height);

    const json11::Json NormalizeJson(string layerName,
                                     string inputName,
                                     string outputName);

    const json11::Json PaddingJson(string layerName,
                                   string inputName,
                                   string outputName,
                                   vector<int> shape);

    const json11::Json PermuteJson(string layerName,
                                   string inputName,
                                   string outputName,
                                   vector<int> orders);

    const json11::Json PoolingJson(string layerName,
                                   string inputName,
                                   string outputName,
                                   int kernelSize,
                                   int pad,
                                   int stride,
                                   string poolingType,
                                   bool floor_mode = false,
                                   bool global_pooling = false,
                                   bool reverseBack = false);

    const json11::Json PoolingJson(string layerName,
                                   string inputName,
                                   string outputName,
                                   pair<int, int> kernel,
                                   pair<int, int> pad,
                                   pair<int, int> stride,
                                   string poolingType,
                                   bool floor_mode = false,
                                   bool global_pooling = false,
                                   bool reverseBack = false);

    const json11::Json PowerJson(string layerName,
                                 string inputName,
                                 string outputName,
                                 float shift,
                                 float scale,
                                 float power);

    const json11::Json PReLUJson(string layerName,
                                 string inputName,
                                 string outputName,
                                 bool channel_shared);

    const json11::Json PriorBoxJson(string layerName,
                                    string inputName,
                                    string outputName,
                                    vector<float> minSizes,
                                    vector<float> maxSizes,
                                    vector<float> variance,
                                    vector<float> aspect_ratio,
                                    float default_aspect_ratio,
                                    float step_h,
                                    float step_w,
                                    float offset,
                                    bool clip,
                                    bool flip);

    const json11::Json ReduceArgsJson(string layerName,
                                      string inputName,
                                      string outputName,
                                      int axis,
                                      vector <int> axisArray,
                                      string operation,
                                      bool keep_dims = true);

    const json11::Json ReLUJson(string layerName,
                                string inputName,
                                string outputName,
                                float negative_slope);

    const json11::Json ReLUXJson(string layerName,
                                 string inputName,
                                 string outputName,
                                 float threshold);

    const json11::Json ReorgJson(string layerName,
                                 string inputName,
                                 string outputName,
                                 int stride,
                                 bool reverse);

    const json11::Json ExpandDimsJson(string layerName,
                                      string inputName,
                                      string outputName,
                                      int axis,
                                      vector <int> axisArray);

    const json11::Json SliceLikeJson(string layerName,
                                     vector <string> inputName,
                                     string outputName,
                                     vector<int> axis, vector<int> shape0, vector<int> shape1);

    const json11::Json TileJson(string layerName,
                                string inputName,
                                string outputName,
                                vector<int> reps, int axis, int tiles);

    const json11::Json ReshapeJson(string layerName,
                                   vector<string> inputName,
                                   string outputName,
                                   vector<int> shape);

    const json11::Json ResizeBilinearJson(string layerName,
                                          string inputName,
                                          string outputName,
                                          int height,
                                          int width);

    const json11::Json ScaleJson(string layerName,
                                 string inputName,
                                 string outputName,
                                 bool bias_term,
                                 int axis = 1,
                                 int fixZero = 0,
                                 bool relu = false);

    const json11::Json ShuffleChannelJson(string layerName,
                                          string inputName,
                                          string outputName,
                                          int group);

    const json11::Json SigmoidJson(string layerName,
                                   string inputName,
                                   string outputName,
                                   bool hard = false);

    const json11::Json SliceJson(string layerName,
                                 string inputName,
                                 vector<string> outputName,
                                 int axis,
                                 vector<int> slice_point);

    const json11::Json SoftMaxJson(string layerName,
                                   string inputName,
                                   string outputName,
                                   int axis);

    const json11::Json LogSoftMaxJson(string layerName,
                                      string inputName,
                                      string outputName,
                                      int axis);

    const json11::Json RepeatJson(string layerName,
                                  string inputName,
                                  string outputName,
                                  int axis, int repeats);

    const json11::Json ArangeJson(string layerName,
                                  vector <string> inputName,
                                  string outputName,
                                  float start, float stop, float step, int repeat);

    const json11::Json AttentionQuantizationJson(string layerName,
                                                 string inputName,
                                                 string outputName,
                                                 bool useAttention, vector<float> centers);

    const json11::Json BoxNMSJson(string layerName,
                                  string inputName,
                                  string outputName,
                                  float overlapThresh, float validThresh,
                                  int topK, int coordStart, int scoreIndex, int idIndex, int backgroundId,
                                  int forceSuppress,
                                  string inFormat, string outFormat);

    const json11::Json BroadcastOpJson(string layerName,
                                       vector <string> inputNames,
                                       string outputName,
                                       string op, vector<int> shape0, vector<int> shape1);

    const json11::Json BatchMatrixMulJson(string layerName,
                                       string inputName,
                                       string outputName,
                                       float alpha, bool transpose0, bool transpose1);

    const json11::Json LayerNormJson(string layerName,
                                          string inputName,
                                          string outputName,
                                          int axis);

    const json11::Json SoftmaxMaskJson(string layerName,
                                       string input0Name,
                                       string input1Name,
                                       string outputName,
                                       int axis);

    const json11::Json GeluJson(string layerName,
                                string inputName,
                                string outputName,
                                string approximation);

    const json11::Json UnaryOpJson(string layerName,
                                   string inputName,
                                   string outputName,
                                   string op);

    const json11::Json BinaryOpJson(string layerName,
                                    string inputName,
                                    string outputName,
                                    string op,
                                    vector<float> values);

    const json11::Json TanhJson(string layerName,
                                string inputName,
                                string outputName);

    const json11::Json SliceAxisJson(string layerName,
                                     string inputName,
                                     string outputName,
                                     int axis, int begin, int end);

    const json11::Json TrimJson(string layerName,
                                string inputName,
                                string outputName,
                                vector<int> shapes);

    const json11::Json UpsampleJson(string layerName,
                                    string inputName,
                                    string outputName,
                                    string Operation,
                                    int stride = 0,
                                    int output_height = 0,
                                    int output_width = 0,
                                    bool align_corners = false,
                                    bool pytorch_half_pixel = false,
                                    bool preserve_aspect_ratio = false);

    const json11::Json ZeroPaddingJson(string layerName,
                                       string inputName,
                                       string outputName,
                                       int pad);

    const json11::Json ZeroPaddingJson(string layerName,
                                       string inputName,
                                       string outputName,
                                       int pad_t,
                                       int pad_l,
                                       int pad_b,
                                       int pad_r);

    const json11::Json RequantizeJson(string layerName,
                                      string inputName,
                                      string outputName,
                                      float scale,
                                      int zero);

    const json11::Json QuantizeJson(string layerName,
                                    string inputName,
                                    string outputName);

    const json11::Json DequantizeJson(string layerName,
                                      string inputName,
                                      string outputName);

    const json11::Json ShufflePixelJson(string layerName,
                                        string inputName,
                                        string outputName,
                                        int upscaleFactor);

    const json11::Json MeanJson(string layerName,
                                string inputName,
                                string outputName,
                                vector<int> axis);

    const json11::Json MishJson(string layerName,
                                string inputName,
                                string outputName);

    const json11::Json EluJson(string layerName,
                               string inputName,
                               string outputName,
                               float alpha);

    const json11::Json ReflectionPaddingJson(string layerName,
                                             string inputName,
                                             string outputName,
                                             int pad_t,
                                             int pad_l,
                                             int pad_b,
                                             int pad_r);

    const json11::Json SoftplusJson(string layerName,
                                    string inputName,
                                    string outputName);

    const json11::Json SqueezeJson(string layerName,
                                   string inputName,
                                   string outputName,
                                   const vector<int> &axis);

    const json11::Json InstanceNormalizationJson(string layerName,
                                                 string inputName,
                                                 string outputName,
                                                 float eps = 1e-5f);

    const json11::Json ReplicationPaddingJson(string layerName,
                                              string inputName,
                                              string outputName,
                                              int pad_t,
                                              int pad_l,
                                              int pad_b,
                                              int pad_r);

    const json11::Json LeakyReluJson(string layerName,
                                     string inputName,
                                     string outputName,
                                     float alpha);

    const json11::Json SeluJson(string layerName,
                                string inputName,
                                string outputName,
                                float alpha,
                                float gamma);

    const json11::Json TransposeJson(string layerName,
                                     string inputName,
                                     string outputName,
                                     vector<int> outputDims);
}

#endif //PROJECT_JSONCONFIGEDITOR_H
