//
// Created by huangyuyang on 6/13/23.
//

#include "utils.h"

#include "executor.h"

#include "devices/cpu/cpudevice.h"

#ifdef USE_CUDA
#include "devices/cuda/cudadevice.h"
#include "devices/cuda/fastllm-cuda.cuh"
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

    void Executor::SetFirstDevice(const std::string &device) {
        auto temp = this->devices;
        this->devices.clear();
        for (int i = 0; i < temp.size(); i++) {
            if (StartWith(device, temp[i]->deviceType)) {
                this->devices.push_back(temp[i]);
                this->devices.back()->deviceIds = ParseDeviceIds(device, temp[i]->deviceType);
            }
        }
        for (int i = 0; i < temp.size(); i++) {
            if (!StartWith(device, temp[i]->deviceType)) {
                this->devices.push_back(temp[i]);
            }
        }
    }

    std::vector <int> Executor::GetDeviceIds(const std::string &device) {
        for (int i = 0; i < devices.size(); i++) {
            if (StartWith(devices[i]->deviceType, device)) {
                return devices[i]->deviceIds;
            }
        }
        return {0};
    }

    void Executor::Run(const std::string &opType, const fastllm::DataDict &datas, const fastllm::FloatDict &floatParams,
                       const fastllm::IntDict &intParams) {
        auto st = std::chrono::system_clock::now();
        bool lockInCPU = false;
        for (auto &it: datas) {
            if (intParams.find(it.first + "___batch") != intParams.end()) {
                int batch = intParams.find(it.first + "___batch")->second;
                for (int i = 0; i < batch; i++) {
                    lockInCPU |= (((Data**)it.second)[i] && ((Data**)it.second)[i]->lockInCPU);
                }
            } else {
                lockInCPU |= (it.second && it.second->lockInCPU);
            }
        }
        for (auto device: devices) {
            if (lockInCPU && device->deviceType != "cpu") {
                continue;
            }
            if (device->CanRun(opType, datas, floatParams, intParams)) {
#ifdef USE_CUDA
                if (device->deviceType == "cuda" && device->deviceIds.size() > 0) {
                    FastllmCudaSetDevice(device->deviceIds[0]);
                }
#endif
                for (auto &it: datas) {
                    if (intParams.find(it.first + "___batch") != intParams.end()) {
                        int batch = intParams.find(it.first + "___batch")->second;
                        for (int i = 0; i < batch; i++) {
                            if (((Data**)it.second)[i]) {
                                ((Data**)it.second)[i]->ToDevice((void *) device);
                            }
                        }
                    } else {
                        if (it.second) {
                            it.second->ToDevice((void *) device);
                        }
                    }
                }
                device->Reshape(opType, datas, floatParams, intParams);
                device->Run(opType, datas, floatParams, intParams);
#ifdef DEBUG
                long long int ops = device->Ops(opType, datas, floatParams, intParams);
                float spend = GetSpan(st, std::chrono::system_clock::now());
                profiler[opType][device->deviceType].first += spend;
                profiler[opType][device->deviceType].second += ops;
#endif
                break;
            }
        }
    }

    void Executor::ClearProfiler() {
        profiler.clear();
    }

    void Executor::PrintProfiler() {
        if (profiler.empty()) {
            return;
        }
        float sum = 0.0;
        printf("%-23s%6s%10s%10s\n", "opType", "device", "time(s)", "Gops");
        printf("-------------------------------------------------\n");
        for (auto &op : profiler) {
            for (auto &dev : op.second) {
                const char *op_type = op.first.c_str();
                const char *device_type = dev.first.c_str();
                float time = dev.second.first;
                double gops = (double) dev.second.second / (dev.second.first * 1024 * 1024 * 1024);

                if (gops == 0) {
                    printf("%-24s%5s%10.2f%10s\n", op_type, device_type, time, "-");
                } else {
                    printf("%-24s%5s%10.2f%10.2f\n", op_type, device_type, time, gops);
                }
                
                sum += time;
            }
        }
        printf("total spend %.2fs\n", sum);
    }
}