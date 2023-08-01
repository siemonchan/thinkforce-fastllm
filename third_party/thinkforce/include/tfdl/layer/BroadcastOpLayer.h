//
// Created by huangyuyang on 11/24/19.
//

#ifndef PROJECT_BROADCASTOPLAYER_H
#define PROJECT_BROADCASTOPLAYER_H

#include "../Layer.h"

namespace tfdl {
    template<typename T>
    class BroadcastOpLayer : public Layer<T> {
    public:
        BroadcastOpLayer(const json11::Json &config, string dataType) : Layer<T>(config, dataType) {
            auto &param = config["param"];
            op = param["op"].string_value();
            if (op == "broadcast_add") {
                opFunc = ([] (float x, float y) -> float {
                    return x + y;
                });
            } else if (op == "broadcast_sub") {
                opFunc = ([] (float x, float y) -> float {
                    return x - y;
                });
            } else if (op == "broadcast_mul") {
                opFunc = ([] (float x, float y) -> float {
                    return x * y;
                });
            } else if (op == "broadcast_div") {
                opFunc = ([] (float x, float y) -> float {
                    return x / y;
                });
            } else if (op == "broadcast_max") {
                opFunc = ([] (float x, float y) -> float {
                    return std::max(x, y);
                });
            } else if (op == "broadcast_min") {
                opFunc = ([] (float x, float y) -> float {
                    return std::min(x, y);
                });
            } else if (op == "broadcast_equal") {
                opFunc = ([] (float x, float y) -> float {
                    return fabs(x - y) < 1e-15;
                });
            } else if (op == "broadcast_and") {
                opFunc = ([] (float x, float y) -> float {
                    return (fabs(x) > 1e-8) && (fabs(y) > 1e-8);
                });
            } else if (op == "broadcast_or") {
                opFunc = ([] (float x, float y) -> float {
                    return (fabs(x) > 1e-8) || (fabs(y) > 1e-8);
                });
            } else if (op == "broadcast_less") {
                opFunc = ([] (float x, float y) -> float {
                    return x < y;
                });
            } else if (op == "broadcast_greater") {
                opFunc = ([] (float x, float y) -> float {
                    return x > y ;
                });
            }

            //  type == "Equal" || type == "And" || type == "Or" || type == "Less"

            if (!param["shape0"].is_null()) {
                for (const auto &id : param["shape0"].array_items()) {
                    shape0.push_back(id.int_value());
                }
            }

            if (!param["shape1"].is_null()) {
                for (const auto &id : param["shape1"].array_items()) {
                    shape1.push_back(id.int_value());
                }
            }
        }

        ~BroadcastOpLayer() {

        };

        void Calc(float *input0, float *input1, float *output, int axis);

        void Forward();

        void ForwardTF();

        void Reshape();

        string arrayString(vector <int> &array);

        string ToJsonParams();

        json11::Json ToJson();

        string GetPrintType();

#ifdef USE_TFACC40T
        bool TFACCSupport();
        void TFACCInit();
        void TFACCRelease();
        tfacc40t::MEM_U8 *tfaccWeight = nullptr;
#endif
    private:
        string op;
        float (*opFunc) (float, float);
        int dims;
        vector <int> shape0, shape1;
        vector <int> input0Dims, input1Dims, outputDims;
        vector <int> input0Spatial, input1Spatial, outputSpatial;

        bool isSimpleBroadcast();
    };
}

#endif //PROJECT_BROADCASTOPLAYER_H
