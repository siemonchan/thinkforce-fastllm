//
// Created by yuyang.huang on 17-11-14.
//

#ifndef TFDL_DEPTHTOSPACELAYER_H
#define TFDL_DEPTHTOSPACELAYER_H

#include "BasePermuteLayer.h"

namespace tfdl {
    template<typename T>
    class DepthToSpaceLayer : public BasePermuteLayer<T> {
    public:
        DepthToSpaceLayer(const json11::Json &config, string dataType) : BasePermuteLayer<T>(config, dataType) {
            auto &param = config["param"];

            blockSize = param["blocksize"].int_value();
            if (param["mode"].is_null() || param["mode"].string_value() == "DCR") {
                mode = DCR;
            } else if (param["mode"].string_value() == "CRD") {
                mode = CRD;
            } else {
                mode = Invalid;
            }

            if (blockSize <= 0) throw_exception("blocksize must be > 0",new TFDLInitException());
            if (mode == Invalid) throw_exception("unknown mode",new TFDLInitException());
        }

        ~DepthToSpaceLayer() {
        }

        // string ToJsonParams();
        json11::Json ToJson();
        void Reshape();

    private:
        enum {
            Invalid,
            DCR,
            CRD
        } mode;
        int blockSize;
    };


}


#endif
