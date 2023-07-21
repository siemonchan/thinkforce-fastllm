//
// Created by huangyuyang on 6/13/23.
//

#include "utils.h"
#include "device.h"

namespace fastllm {
    bool BaseDevice::Malloc(void **ret, Data &data) {
        return Malloc(ret, data.expansionBytes);
    }

    bool BaseDevice::CopyDataFromCPU(Data &data) {
        AssertInFastLLM(data.cpuData != nullptr, "Copy data to " + this->deviceName + " from cpu failed: cpu's data is null.\n");
        AssertInFastLLM(data.deviceData == nullptr, "Copy data to " + this->deviceName + " from cpu failed: device's data is not null.\n");
        Malloc(&data.deviceData, data.expansionBytes);
        bool ret = CopyDataFromCPU(data.cudaData, data.cpuData, data.expansionBytes);
        delete[] data.cpuData;
        data.cpuData = nullptr;
        return ret;
    }

    bool BaseDevice::CopyDataToCPU(Data &data) {
        AssertInFastLLM(data.cpuData == nullptr, "Copy data from " + this->deviceName + " to cpu failed: cpu's data is not null.\n");
        AssertInFastLLM(data.deviceData != nullptr, "Copy data from " + this->deviceName + " to cpu failed: device's data is null.\n");
        data.cpuData = new uint8_t [data.expansionBytes];
        bool ret = CopyDataToCPU(data.cpuData, data.deviceData, data.expansionBytes);
        this->Free(data.deviceData);
        data.deviceData = nullptr;
        return ret;
    }

    bool BaseDevice::CanRun(const std::string &opType, const fastllm::DataDict &datas,
                            const fastllm::FloatDict &floatParams, const fastllm::IntDict &intParams) {
        if (this->ops.find(opType) == this->ops.end()) {
            return false;
        }
        return this->ops[opType]->CanRun(opType, datas, floatParams, intParams);
    }

    void BaseDevice::Reshape(const std::string &opType, const fastllm::DataDict &datas,
                             const fastllm::FloatDict &floatParams, const fastllm::IntDict &intParams) {
        this->ops[opType]->Reshape(opType, datas, floatParams, intParams);
    }

    void BaseDevice::Run(const std::string &opType, const fastllm::DataDict &datas,
                         const fastllm::FloatDict &floatParams, const fastllm::IntDict &intParams) {
#ifdef DEBUG
        auto st = chrono::system_clock::now();
#endif
        this->ops[opType]->Run(opType, datas, floatParams, intParams);
#ifdef DEBUG
        auto end = chrono::system_clock::now();
        this->ops[opType]->profile.Write(GetSpan(st, end), this->ops[opType]->Ops(opType, datas, floatParams, intParams));
#endif
    }

    void BaseDevice:: Profile(bool silent) {
        for (auto op : ops) {
            string opType = op.first;
            BaseOperator *opObject = op.second;
            opObject->profile.Profile(opType, deviceType, silent);
        }
    }

    bool BaseOperator::CanRun(const std::string &opType, const DataDict &datas, const FloatDict &floatParams,
                              const IntDict &intParams) {
        return true;
    }

    void BaseOperator::Reshape(const std::string &opType, const DataDict &datas, const FloatDict &floatParams,
                               const IntDict &intParams) {
        if (datas.find("output") == datas.end()) {
            return;
        }
        // 默认的Reshape，把output和input变成一样的形状
        Data &input = *(datas.find("input")->second);
        Data &output = *(datas.find("output")->second);

        output.dataType = input.dataType;
        output.Resize(input.dims);
    }

    uint64_t BaseOperator::Ops(const std::string &opType, const DataDict &datas, const FloatDict &floatParams,
                               const IntDict &intParams) {
        return 0;
    }
}