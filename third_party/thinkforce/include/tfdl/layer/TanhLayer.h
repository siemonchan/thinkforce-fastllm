//
// Created by test on 17-12-13.
//

#ifndef PROJECT_TANHLAYER_H
#define PROJECT_TANHLAYER_H
#include "../Layer.h"
#include "MapTableLayer.h"

namespace tfdl{
    template<typename T>
    class TanhLayer:public Layer<T>{
    public:
        TanhLayer(const json11::Json &config, string dataType):Layer<T>(config,dataType){

        }
        ~TanhLayer(){

        };
        void Forward();
        void ForwardTF();

        void Prepare();

        json11::Json ToJson();
        void SaveMiniNet(std::ofstream &outputFile);

#ifdef USE_TFACC40T
        void TFACCInit();

        void TFACCMark();

        bool TFACCSupport();

        void TFACCRelease();
#endif

    private:

#ifdef USE_TFACC40T
        MapTableLayer<T> *mapTableLayer = nullptr;
#endif
    };



}

#endif //PROJECT_TANHLAYER_H
