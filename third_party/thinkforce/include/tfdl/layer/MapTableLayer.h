//
// Created by siemon on 12/28/21.
//

#ifndef TFDL_MAPTABLELAYER_H
#define TFDL_MAPTABLELAYER_H

#include "Layer.h"

namespace tfdl {
    template<typename T>
    class MapTableLayer : public Layer<T> {
    public:
        MapTableLayer(const json11::Json &config, string dataType) : Layer<T>(config, dataType) {
            // nothing
        }

        void Forward();

        void ForwardTF();

#ifdef USE_TFACC40T

        MapTableLayer(string layerName, string dataType, INT8Config *int8Config,
                      Data<T> *input, Data<T> *output, Data<uint8_t> *mapTable, int deviceID, bool shared) : Layer<T>(
                layerName, dataType) {
            this->int8Config = int8Config;
            this->inputs.clear();
            this->inputs.push_back(input);
            this->outputs.clear();
            this->outputs.push_back(output);
            uint8_t input_zero = int8Config->GetDataConfig(input->GetName())->getZeroPoint();

            this->map_table = new tfacc40t::MEM_U8(TF_TFNN_GetChipId(deviceID), mapTable->Count(0));
            memcpy((char *) this->map_table->userAddr, mapTable->GetCpuData(), mapTable->GetSize());
            this->bypass_adder = new tfacc40t::MEM_U8(TF_TFNN_GetChipId(deviceID), min(single, (int)input->Count(0)));
            for (int i = 0; i < min(single, (int)input->Count(0)); i++) {
                ((char *) this->bypass_adder->userAddr)[i] = input_zero;
            }

            this->SetDeviceId(deviceID);
            this->SetShare(shared);
            this->TFACCInit();
        }

        void TFACCInit();

        void TFACCRelease();

#endif
    private:
#ifdef USE_TFACC40T
        int single = 4096;
        tfacc40t::MEM_U8 *map_table = nullptr;
        tfacc40t::MEM_U8 *bypass_adder = nullptr;
#endif
    };
}

#endif //TFDL_MAPTABLELAYER_H
