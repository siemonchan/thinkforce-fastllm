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
    public:
        Executor (); // 创建默认的Executor

        ~Executor(); // 析构

        void ClearDevices(); // 清空 devices

        void AddDevice(BaseDevice *device); // 增加一个device

        // 运行一个op
        void Run(const std::string &opType, const fastllm::DataDict &datas, const fastllm::FloatDict &floatParams,
                 const fastllm::IntDict &intParams);

        // 按照optype打印运行细节
        void Profile(bool silent = false);
    };
}

#endif //FASTLLM_EXECUTOR_H
