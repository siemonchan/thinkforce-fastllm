//
// Created by test on 18-9-10.
//

#ifndef PROJECT_CROPLAYER_H
#define PROJECT_CROPLAYER_H

#include <Layer.h>
#include <common.h>


namespace tfdl{

    template<typename T>
    class CropLayer: public Layer<T>{

    public:
        CropLayer(const json11::Json &config, string dataType):Layer<T>(config,dataType) {
            auto param = config["param"];
            axis = (param["axis"].is_null()) ? 2 : param["axis"].int_value();
            //assert(param["offset"].array_items().size() == 3 || param["offset"].array_items().size() == 1);
            offset.clear();
            if (!param["offset"].is_null()) {
                for (int i = 0; i < param["offset"].array_items().size(); i++) {
                    offset.push_back(param["offset"].array_items()[i].int_value());
                }
            }
            if (offset.size() == 0) {
                offset.push_back(0);
            }
        }

        void Forward();
        void ForwardTF();
        void Reshape();
        json11::Json ToJson();
    private:
        vector<int> offset;
        int axis;
    };


}



#endif //PROJECT_CROPLAYER_H
