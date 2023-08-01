//
// Created by yuyang.huang on 17-11-14.
//

#ifndef TFDL_PRIORBOXLAYER_H
#define TFDL_PRIORBOXLAYER_H

#include "../Layer.h"

namespace tfdl {
    template<typename T>
    class PriorBoxLayer : public Layer<T> {
    public:
        PriorBoxLayer(const json11::Json &config, string dataType) : Layer<T>(config, dataType) {
            auto &param = config["param"];
            minSizes.clear();
            for (const auto &it : param["minSizes"].array_items()) {
                minSizes.push_back(it.number_value());
            }

            maxSizes.clear();
            for (const auto &it : param["maxSizes"].array_items()) {
                maxSizes.push_back(it.number_value());
            }

            variance.clear();
            for (const auto &it : param["variance"].array_items()) {
                variance.push_back(it.number_value());
            }


            default_aspect_ratio=param["default_aspect_ratio"].is_null()?1.0:param["default_aspect_ratio"].number_value();
            clip = param["clip"].bool_value();
            flip=param["flip"].bool_value();
            if ((!param["step_w"].is_null()) && (!param["step_h"].is_null())) {
                step_h = param["step_h"].number_value();
                step_w = param["step_w"].number_value();
            } else if (param["step"].is_null()) {
                throw_exception("error the step must have a value",new TFDLInitException());
            } else {
                step_h = param["step"].number_value();
                step_w = param["step"].number_value();
            }
            offset = param["offset"].number_value();
            std::vector<float> aspect_ratio_;
            for (const auto &it : param["aspect_ratio"].array_items()) {
                aspect_ratio_.push_back(it.number_value());
            }
            aspect_ratio.clear();
            aspect_ratio.push_back(default_aspect_ratio);
            for (int i = 0; i < aspect_ratio_.size(); ++i) {
                float ar = aspect_ratio_[i];
                bool already_exist = false;
                for (int j = 0; j < aspect_ratio.size(); ++j) {
                    if (fabs(ar - aspect_ratio[j]) < 1e-6) {
                        already_exist = true;
                        break;
                    }
                }
                if (!already_exist) {
                    aspect_ratio.push_back(ar);
                    if (flip) {
                        aspect_ratio.push_back(1./ar);
                    }
                }
            }
            if (aspect_ratio.empty()) {
                numPriors = maxSizes.size() + minSizes.size();
            } else {
                numPriors = maxSizes.size() + minSizes.size() * aspect_ratio.size();
            }

        }

        ~PriorBoxLayer() {
        }

        string arrayString(vector <float> &array);
        string ToJsonParams();
        json11::Json ToJson();
        void Forward();
        void ForwardTF();
        void Reshape();
    private:
        vector <float> minSizes, maxSizes, variance,aspect_ratio;
        float default_aspect_ratio=1.0;
        bool clip,flip;
        float offset;
        float step_w,step_h;
        int numPriors;
    };


}


#endif //CAFFE_PRIORBOXLAYER_H
