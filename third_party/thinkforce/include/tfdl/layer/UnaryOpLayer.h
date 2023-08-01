//
// Created by huangyuyang on 11/22/19.
//

#ifndef PROJECT_UNARYLAYER_H
#define PROJECT_UNARYLAYER_H

#include "../Layer.h"

namespace tfdl {
    template<typename T>
    class UnaryOpLayer : public Layer<T> {
    public:
        UnaryOpLayer(const json11::Json &config, string dataType) : Layer<T>(config, dataType) {
            auto &param = config["param"];
            op = param["op"].string_value();
            if (op == "abs") {
                opFunc = &fabs;
            } else if (op == "round") {
                opFunc = &round;
            } else if (op == "arccos") {
                opFunc = &acos;
            } else if (op == "arcsin") {
                opFunc = &asin;
            } else if (op == "arctan") {
                opFunc = &atan;
            } else if (op == "ceil") {
                opFunc = &ceil;
            } else if (op == "cos") {
                opFunc = &cos;
            } else if (op == "exp") {
                opFunc = &exp;
            } else if (op == "erf") {
                opFunc = &erf;
            } else if (op == "floor") {
                opFunc = &floor;
            } else if (op == "log") {
                opFunc = &log;
            } else if (op == "negative") {
                opFunc = ([] (float x) -> float {
                    return -x;
                });
            } else if (op == "reciprocal") {
                opFunc = ([] (float x) -> float {
                    return 1.0 / x;
                });
            } else if (op == "sin") {
                opFunc = &sin;
            } else if (op == "sqrt") {
                opFunc = &sqrt;
            } else if (op == "square") {
                opFunc = ([] (float x) -> float {
                    return x * x;
                });
            } else if (op == "tan") {
                opFunc = &tan;
            } else if (op == "sqrt" ) {
                opFunc = &sqrt;
            } else if (op == "not") {
                opFunc = ([] (float x) -> float {
                    return (x > -1e-8 && x < 1e-8) ? 1 : 0;
                });
            } else if (op == "reciprocal") {
                opFunc = ([] (float x) -> float {
                    return 1 / x;
                });
            } else if (op == "neg") {
                opFunc = ([] (float x) -> float {
                    return -x;
                });
            } else if (op.size() > 4 && op.substr(0, 4) == "cast") {
                int to = atoi(op.substr(5).c_str());
                // UNDEFINED = 0;
                // FLOAT = 1;   // float
                // UINT8 = 2;   // uint8_t
                // INT8 = 3;    // int8_t
                // UINT16 = 4;  // uint16_t
                // INT16 = 5;   // int16_t
                // INT32 = 6;   // int32_t
                // INT64 = 7;   // int64_t
                // STRING = 8;  // string
                // BOOL = 9;    // bool

                if (to == 0 || to == 1) {
                    opFunc = ([] (float x) -> float {
                        return x;
                    });
                } else if (to >= 2 && to <= 7) {
                    opFunc = ([] (float x) -> float {
                        return (int)x;
                    });
                } else if (to == 9) {
                    opFunc = ([] (float x) -> float {
                        return (int)(fabs(x) > 1e-8);
                    });
                } else {
                    throw_exception("Cast: Unsupported type: " + op + ".", new TFDLRunException());
                }
            }
        }

        ~UnaryOpLayer() {
        };

        void Forward();

        void ForwardTF();

        void Reshape();

        void Prepare();

        string ToJsonParams();

        json11::Json ToJson();

        string GetPrintType();

    private:
        float (*opFunc) (float);
        bool isEqual;
        string op;
    };
}

#endif //PROJECT_UNARYLAYER_H
