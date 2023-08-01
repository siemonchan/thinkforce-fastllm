//
// Created by test on 18-3-12.
//

#ifndef PROJECT_CUSTOMLAYER_H
#define PROJECT_CUSTOMLAYER_H
#include "../Layer.h"

namespace tfdl{
    template<typename T>
    class CustomLayer: public Layer<T>{
    public:
        CustomLayer(const json11::Json& config,string datatype):Layer<T>(config,datatype){
            parse_param(config);
        };
        ~CustomLayer(){
        };
        //TODO: you need implement this function in the project to read json param to your custom layer
        //@param:JSON config : json struct
        //
        void parse_param(const json11::Json& config);
      
        //TODO: you need implement this function , Prepare for your net to Forward
        void Forward();

        void ForwardTF();
        
        //TODO: you need implement this function, Prepare for your net to Reshape
        void Reshape();

        //TODO: our optimize need rebuild json , and it need know which param will  be saved in new json
        string ToJsonParams();

        //TODO: decide if add padding after this layer, this function help our optimizer to know if we can optimize this layer
        bool ifoptimize(int op);
    };
}


#endif //PROJECT_CUSTOMLAYER_H
