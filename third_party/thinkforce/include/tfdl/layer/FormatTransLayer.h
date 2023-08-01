#ifndef TFDL_FORMATTRANSLAYER_H
#define TFDL_FORMATTRANSLAYER_H

#include "Layer.h"

namespace tfdl {
    template<typename T>
    class FormatTransLayer : public Layer<T> {
    public:
        FormatTransLayer(const json11::Json &config, string dataType) : Layer<T>(config, dataType) {
            auto &param = config["param"];

            inputFormat = param["inputFormat"].string_value();
            outputFormat = param["outputFormat"].string_value();

            if (!supportFormat(inputFormat)) throw_exception("unsupported input format", new TFDLInitException());
            if (!supportFormat(outputFormat)) throw_exception("unsupported output format", new TFDLInitException());
            if (!supportConversion(inputFormat, outputFormat)) throw_exception("unsupported conversion", new TFDLInitException());

            inputWidth = param["inputWidth"].int_value();
            inputHeight = param["inputHeight"].int_value();
            inputStride = param["inputStride"].int_value();

            if (inputWidth <= 0) throw_exception("inputWidth should be greater than 0", new TFDLInitException());
            if (inputHeight <= 0) throw_exception("inputHeight should be greater than 0", new TFDLInitException());
            if (inputStride <= 0) throw_exception("inputStride should be greater than 0", new TFDLInitException());

            // TODO: add crop & resize support
        }

        ~FormatTransLayer() {
        }

        void ResetInputFormat(const std::string& newInputFormat, int stride) {
            if (!supportFormat(newInputFormat)) throw_exception("unsupported input format", new TFDLInitException());
            if (!supportConversion(newInputFormat, outputFormat)) throw_exception("unsupported conversion", new TFDLInitException());
            if (stride <= 0) throw_exception("inputStride should be greater than 0", new TFDLInitException());

            inputFormat = newInputFormat;
            inputStride = stride;
            ReshapeInput();
        }

        // string ToJsonParams();
        json11::Json ToJson();
        void Reshape();
        void ReshapeInput();
        void Forward();
        void ForwardTF() {}

        bool supportFormat(const std::string& format);
        bool supportConversion(const std::string& srcFormat, const std::string& dstFormat);

    private:
        std::string inputFormat;
        std::string outputFormat;

        int inputWidth;
        int inputHeight;
        int inputStride;
    };


}


#endif
