//
// Created by huangyuyang on 11/03/22.
//

#ifndef PROJECT_ONNX_H
#define PROJECT_ONNX_H

#include <string>
#include "json11.hpp"
#include "caffe.h"
#include "tfdl.api.h"

namespace tfdl {
    struct StringStringEntry {
        std::string key, value;

        void Parse(uint8_t *start, uint8_t *end);
    };

    struct OperatorSetId {
        std::string domain;
        int64_t version = 0;

        void Parse(uint8_t *start, uint8_t *end);
    };

    struct Dimension {
        int64_t dim_value = 0;
        std::string dim_param, denotation;

        void Parse(uint8_t *start, uint8_t *end);
    };

    struct TensorShape {
        std::vector <Dimension> dim;

        void Parse(uint8_t *start, uint8_t *end);
    };

    struct OnnxTypeTensor {
        int elem_type = 0;
        TensorShape shape;

        void Parse(uint8_t *start, uint8_t *end);
    };

    struct OnnxType {
        OnnxTypeTensor tensor_type;
        std::string denotation;

        void Parse(uint8_t *start, uint8_t *end);
    };

    struct ValueInfo {
        std::string name, doc_string;
        OnnxType type;

        void Parse(uint8_t *start, uint8_t *end);
    };

    struct TensorAnnotation {
        std::string tensor_name;
        std::vector <StringStringEntry> quant_parameter_tensor_names;

        void Parse(uint8_t *start, uint8_t *end);
    };

    struct OnnxTensorSegment {
        int64_t begin = 0, end = 0;

        void Parse(uint8_t *start, uint8_t *end);
    };

    struct OnnxTensor {
        enum DataType {
            UNDEFINED = 0, // Basic types.
            FLOAT = 1,   // float
            UINT8 = 2,   // uint8_t
            INT8 = 3,    // int8_t
            UINT16 = 4,  // uint16_t
            INT16 = 5,   // int16_t
            INT32 = 6,   // int32_t
            INT64 = 7,   // int64_t
            STRING = 8,  // string
            BOOL = 9,    // bool

            // IEEE754 half-precision floating-point format (16 bits wide).
            // This format has 1 sign bit, 5 exponent bits, and 10 mantissa bits.
            FLOAT16 = 10,

            DOUBLE = 11,
            UINT32 = 12,
            UINT64 = 13,
            COMPLEX64 = 14,     // complex with float32 real and imaginary components
            COMPLEX128 = 15,    // complex with float64 real and imaginary components

            // Non-IEEE floating-point format based on IEEE754 single-precision
            // floating-point number truncated to 16 bits.
            // This format has 1 sign bit, 8 exponent bits, and 7 mantissa bits.
            BFLOAT16 = 16,

            // Future extensions go here.
        };

        std::vector <int64_t> dims;
        int32_t data_type = 0;
        OnnxTensorSegment segment;
        std::vector <float> float_data;
        std::vector <int32_t> int32_data;
        std::vector <std::string> string_data;
        std::vector <int64_t> int64_data;
        std::string name, doc_string, raw_data;
        std::vector <StringStringEntry> external_data;
        int data_location = 0;
        std::vector <double> double_data;
        std::vector <uint64_t> uint64_data;

        void Parse(uint8_t *start, uint8_t *end);
    };

    struct OnnxAttribute;

    struct OnnxNode {
        std::vector <std::string> input, output;
        std::string name, op_type, domain, doc_string;
        std::vector <OnnxAttribute> attribute;

        void Parse(uint8_t *start, uint8_t *end);
    };

    struct OnnxGraph {
        std::vector <OnnxNode> node;
        std::vector <OnnxTensor> initializer;
        std::string name, doc_string;
        std::vector <ValueInfo> input, output, value_info;
        std::vector <TensorAnnotation> quantization_annotation;

        void Parse(uint8_t *start, uint8_t *end);
    };

    struct OnnxAttribute {
        enum AttributeType {
            UNDEFINED = 0,
            FLOAT = 1,
            INT = 2,
            STRING = 3,
            TENSOR = 4,
            GRAPH = 5,

            FLOATS = 6,
            INTS = 7,
            STRINGS = 8,
            TENSORS = 9,
            GRAPHS = 10,
        };

        std::string name, ref_attr_name, doc_string;
        int type = 0;
        float f = 0.0f;
        int64_t i = 0;
        std::string s;
        OnnxTensor t;
        OnnxGraph g;

        std::vector <float> floats;
        std::vector <int64_t> ints;
        std::vector <std::string> strings;
        std::vector <OnnxTensor> tensors;
        std::vector <OnnxGraph> graphs;

        void Parse(uint8_t *start, uint8_t *end);
    };

    struct OnnxModel {
        int64_t ir_version = 0, model_version = 0;
        std::vector <OperatorSetId> opset_import;
        std::string producer_name, producer_version, domain, doc_string;
        OnnxGraph graph;
        std::vector <StringStringEntry> metadata_props;

        void Parse(uint8_t *start, uint8_t *end);
    };

    void ReadOnnxModel(const std::string &fileName, json11::Json &net, CaffeModel &tfdlModel,
                       const std::vector <std::string> &ignoreLayers, ConverterConfig *converterConfig);
}

#endif //PROJECT_ONNX_H
