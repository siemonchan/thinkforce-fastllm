//
// Created by huangyuyang on 6/13/23.
//

#ifndef FASTLLM_EXECUTOR_H
#define FASTLLM_EXECUTOR_H

#include "device.h"

namespace fastllm {
    class Executor {
    private:
        std::vector <BaseDevice*> devices;
        std::map <std::string, std::map<std::string, pair<float, long long int>>> profiler; // <opType, <device, <time, ops>>>

    public:
        Executor (); // 创建默认的Executor

        ~Executor(); // 析构

        void ClearDevices(); // 清空 devices

        void AddDevice(BaseDevice *device); // 增加一个device

        void SetFirstDevice(const std::string &device); // 设定优先的device

        std::vector <int> GetDeviceIds(const std::string &device); // 获取指定device的deviceIds

        // 运行一个op
        void Run(const std::string &opType, const fastllm::DataDict &datas, const fastllm::FloatDict &floatParams,
                 const fastllm::IntDict &intParams);

        void ClearProfiler();

        void PrintProfiler();
    };
}

#endif //FASTLLM_EXECUTOR_H
