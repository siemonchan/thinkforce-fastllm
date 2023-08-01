#include "Layer.h"

namespace tfdl {
    template<typename T>
    class TriluLayer : public Layer<T> {
    public:
        TriluLayer(const json11::Json &config, string dataType) : Layer<T>(config, dataType) {
            auto param = config["param"];
            this->upper = param["upper"].int_value();
            this->k = param["k"].int_value();
        }

        void Forward();

        void ForwardTF();

        json11::Json ToJson();

        string ToJsonParams();

    private:
        int upper = 0;
        int k = 0;
    };
}