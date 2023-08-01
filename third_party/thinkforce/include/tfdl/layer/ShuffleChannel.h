//
// Created by test on 18-9-12.
//

#ifndef PROJECT_SHUFFLECHANNEL_H
#define PROJECT_SHUFFLECHANNEL_H
//#include <Net.h>
#include <Layer.h>

namespace tfdl{

    template<typename T>
    class ShuffleChannel : public Layer<T> {

    public:
        ShuffleChannel(const json11::Json& config,string dataType):Layer<T>(config,dataType){
            auto param = config["param"];
            assert(param["group"].is_null() == false);
            group = param["group"].int_value();

        }

        void Forward();
        void ForwardTF();
        string ToJsonParams();

        json11::Json ToJson();

    private:
        int group;
    };



}


#endif //PROJECT_SHUFFLECHANNEL_H
