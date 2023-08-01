//
// Created by yuyang.huang on 17-11-15.
//

#ifndef TFDL_DETECTIONOUTPUTLAYER_H
#define TFDL_DETECTIONOUTPUTLAYER_H

#include "../Layer.h"
#include "../bbox_util.h"
#include <iostream>
#ifdef ARM
#include <arm_neon.h>
#include "armFunctions.h"
#endif

namespace tfdl {
    template<typename T>
    class DetectionOutputLayer : public Layer<T> {
    public:
        DetectionOutputLayer(const json11::Json &config, string dataType) : Layer<T>(config, dataType) {
            auto &param = config["param"];
            num_classes_ = param["num_classes_"].int_value();
            share_location_ = true;
            num_loc_classes_ = share_location_ ? 1 : num_classes_;
            background_label_id_ = param["background_label_id_"].int_value();
            code_type_ = CodeType::PriorBoxParameter_CodeType_CENTER_SIZE;
            variance_encoded_in_target_ =
                    param["variance_encoded_in_target_"].int_value();
            keep_top_k_ = param["keep_top_k_"].int_value();
            confidence_threshold_ = param["confidence_threshold_"].is_null() ?
                                    -1e100:param["confidence_threshold_"].number_value() ;
            // Parameters used in nms.
            nms_threshold_ = param["nms_threshold_"].number_value();
            TFCHECK_GE(nms_threshold_, 0.);
            eta_ = 1.0;
            top_k_ = param["top_k_"].int_value();
            mbox_reverse_ = param["switch_channels"].int_value();
            clip_bbox = ((param["clip_bbox"].is_null())?false:param["clip_bbox"].int_value());
        }
        ~DetectionOutputLayer() {

        }

        string ToJsonParams();
        json11::Json ToJson();
        void Forward();
        void ForwardTF();
        void Reshape();
        int GetNumClasses();
        float * ddoutdata = nullptr;
    private:
        bool mbox_reverse_; //special for mv00, need swap(mbox[0], mbox[1]), swap(mbox[2], mbox[3])

        int num_classes_;
        bool share_location_;
        int num_loc_classes_;
        int background_label_id_;
        CodeType code_type_;
        bool variance_encoded_in_target_;
        int keep_top_k_;
        float confidence_threshold_;
        bool clip_bbox;
        int num_;
        int num_priors_;

        float nms_threshold_;
        int top_k_;
        float eta_;

        string output_name_prefix_;
        string output_format_;
        map<int, string> label_to_name_;
        map<int, string> label_to_display_name_;
        vector<string> names_;
        vector<pair<int, int> > sizes_;
        int num_test_image_;
        int name_count_;
        bool has_resize_;

        Data<float> bbox_preds_;
        Data<float> bbox_permute_;
        Data<float> conf_permute_;

        // Retrieve all prior bboxes. It is same within a batch since we assume all
        // images in a batch are of same dimension.
        vector<NormalizedBBox> prior_bboxes;
        vector<vector<float> > prior_variances;
        vector<vector<magicType> > original_prior_varicances;
        bool samePriorValue1 = false, samePriorValue2 = false;
        float priorValue1, priorValue2;
        magicType int8PriorValue2;

        float expAnswer[300][300];
        float answer[1 << 8];
        float locAnswer[1 << 8];
        float confAnswer[1 << 8];
        vector<int> idList[1 << 8]; //idList[i] will save list of id when conf[id] = i
    };


}

#endif //CAFFE_DETECTIONOUTPUTLAYER_H
