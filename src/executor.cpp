//
// Created by huangyuyang on 6/13/23.
//

#include "utils.h"

#include "executor.h"

#include "devices/cpu/cpudevice.h"

#ifdef USE_CUDA
#include "devices/cuda/cudadevice.h"
#endif

#ifdef USE_TFACC40T
#include "devices/tfacc/tfaccdevice.h"
#endif

namespace fastllm {
    Executor::Executor() {
        this->devices.clear();
#ifdef USE_CUDA
        this->devices.push_back((BaseDevice*) new CudaDevice());
#endif
#ifdef USE_TFACC40T
        this->devices.push_back((BaseDevice*) new TfaccDevice());
#endif
        this->devices.push_back((BaseDevice*) new CpuDevice());
    }

    Executor::~Executor() {
        for (int i = 0; i < devices.size(); i++) {
            delete devices[i];
        }
    }

    void Executor::ClearDevices() {
        this->devices.clear();
    }

    void Executor::AddDevice(fastllm::BaseDevice *device) {
        this->devices.push_back(device);
    }

    void Executor::Run(const std::string &opType, const fastllm::DataDict &datas, const fastllm::FloatDict &floatParams,
                       const fastllm::IntDict &intParams) {
//auto st = std::chrono::system_clock::now();
        bool lockInCPU = false;
        for (auto &it : datas) {
            lockInCPU |= it.second->lockInCPU;
        }
        for (auto device : devices) {
            if (lockInCPU && device->deviceType != "cpu") {
                continue;
            }
            if (device->CanRun(opType, datas, floatParams, intParams)) {
                for (auto &it : datas) {
                    it.second->ToDevice((void*)device);
                }
                device->Reshape(opType, datas, floatParams, intParams);
                device->Run(opType, datas, floatParams, intParams);
                break;
            }
        }
//printf("%s spend %f s.\n", opType.c_str(), GetSpan(st, std::chrono::system_clock::now()));
    }

    void Executor::Profile(bool silent) {
        for (auto device : devices) {
            device->Profile(silent);
        }
    }
}