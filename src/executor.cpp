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
        auto st = std::chrono::system_clock::now();
        bool lockInCPU = false;
        for (auto &it: datas) {
            if (intParams.find(it.first + "___batch") != intParams.end()) {
                int batch = intParams.find(it.first + "___batch")->second;
                for (int i = 0; i < batch; i++) {
                    lockInCPU |= ((Data**)it.second)[i]->lockInCPU;
                }
            } else {
                lockInCPU |= it.second->lockInCPU;
            }
        }
        for (auto device: devices) {
            if (lockInCPU && device->deviceType != "cpu") {
                continue;
            }
            if (device->CanRun(opType, datas, floatParams, intParams)) {
                for (auto &it: datas) {
                    if (intParams.find(it.first + "___batch") != intParams.end()) {
                        int batch = intParams.find(it.first + "___batch")->second;
                        for (int i = 0; i < batch; i++) {
                            ((Data**)it.second)[i]->ToDevice((void *) device);
                        }
                    } else {
                        it.second->ToDevice((void *) device);
                    }
                }
                device->Reshape(opType, datas, floatParams, intParams);
                device->Run(opType, datas, floatParams, intParams);
                break;
            }
        }
        float spend = GetSpan(st, std::chrono::system_clock::now());
        profiler[opType] += spend;
    }

    void Executor::ClearProfiler() {
        profiler.clear();
    }

    void Executor::PrintProfiler() {
        float sum = 0.0;
        for (auto &it : profiler) {
            printf("%s spend %f\n", it.first.c_str(), it.second);
            sum += it.second;
        }
        printf("total spend %f\n", sum);
    }
}