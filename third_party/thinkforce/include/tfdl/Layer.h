//
// Created by yuyang.huang on 17-11-7.
//

#ifndef TFDL_LAYER_H
#define TFDL_LAYER_H

#include <mutex>

#include "common.h"
#include "Data.h"
#include "INT8Config.h"
#ifdef USE_TFACC
#include <tfblas_api.hpp>
#endif

using namespace std;
#ifdef USE_TFACC
using namespace tfblas;
#endif

namespace tfdl {
    struct LayerAttrs {
        mutex locker;
        map<string, Data<uint8_t> *> Uint8AttrData;
        map<string, Data<float> *> FloatAttrData;
        map<string, Data<int> *> Int32AttrData;

        LayerAttrs() {
            this->Uint8AttrData = {};
            this->FloatAttrData = {};
            this->Int32AttrData = {};
        }

        bool FindUint8AttrData(const string &attrName) {
            return this->Uint8AttrData.find(attrName) != this->Uint8AttrData.end();
        }

        bool FindFloatAttrData(const string &attrName) {
            return this->FloatAttrData.find(attrName) != this->FloatAttrData.end();
        }

        bool FindInt32AttrData(const string &attrName) {
            return this->Int32AttrData.find(attrName) != this->Int32AttrData.end();
        }

        void AddUint8AttrData(const string &attrName, Data<uint8_t> *data) {
            this->locker.lock();
            this->Uint8AttrData[attrName] = data;
            this->locker.unlock();
        }

        void AddFloatAttrData(const string &attrName, Data<float> *data) {
            this->locker.lock();
            this->FloatAttrData[attrName] = data;
            this->locker.unlock();
        }

        void AddInt32AttrData(const string &attrName, Data<int> *data) {
            this->locker.lock();
            this->Int32AttrData[attrName] = data;
            this->locker.unlock();
        }

        Data<uint8_t> *GetUint8AttrData(const string &attrName) {
            if (FindUint8AttrData(attrName)) {
                return this->Uint8AttrData[attrName];
            } else {
                return nullptr;
            }
        }

        Data<float> *GetFloatAttrData(const string &attrName) {
            if (FindFloatAttrData(attrName)) {
                return this->FloatAttrData[attrName];
            } else {
                return nullptr;
            }
        }

        Data<int> *GetInt32AttrData(const string &attrName) {
            if (FindInt32AttrData(attrName)) {
                return this->Int32AttrData[attrName];
            } else {
                return nullptr;
            }
        }

        void Clear() {
            this->locker.lock();
            for (auto data : Uint8AttrData) {
                delete data.second;
            }
            for (auto data : FloatAttrData) {
                delete data.second;
            }
            for (auto data : Int32AttrData) {
                delete data.second;
            }
            Uint8AttrData.clear();
            FloatAttrData.clear();
            Int32AttrData.clear();
            this->locker.unlock();
        }
    };

    template <typename T>
    class Layer {
    public:
        Layer(const json11::Json &config, string dataType) : layerDataType(dataType) {
            layerName = config["layerName"].string_value();
            layerType = config["layerType"].string_value();

            inputDataType = config["inputDataType"].is_null() ? dataType : config["inputDataType"].string_value();
            outputDataType = config["outputDataType"].is_null() ? dataType : config["outputDataType"].string_value();
            if (!config["outputDataType"].is_null()) {
                forceOutputDataType = true;
            }

            weightDataType = config["weightDataType"].is_null() ? dataType : config["weightDataType"].string_value();

            forceMin = config["forceMin"].is_null() ? unlockMin : float(config["forceMin"].number_value());
            if(forceMin > 0) {
                forceMin = unlockMin;
            }
            forceMax = config["forceMax"].is_null() ? unlockMax : float(config["forceMax"].number_value());
            if(forceMax < 0) {
                forceMax = unlockMax;
            }
        }

        Layer(string layerName, string layerType) :
                layerName(layerName), layerType(layerType) {}

        virtual ~Layer() {
            if(floatOutput != nullptr)delete floatOutput;
            if(ShareAttr == false) {
                for (auto uintdata : Uint8AttrDatas) {
                    delete uintdata.second;
                }
                for (auto floatdata : FloatAttrDatas) {
                    delete floatdata.second;
                }
                for (auto int32data : Int32AttrDatas) {
                    delete int32data.second;
                }
            }

            if (this->layerDataType == "int8" && this->weightDataType == "int8") {
                if (!this->ShareAttr) {
                    for (int i = 0; i < floatWeights.size(); i++) {
                        delete floatWeights[i];
                    }
                }
            }
        }

        // 对该层进行推理
        virtual void Forward() = 0;

        // 开始推理
        virtual void Launch();

        // 是否推理完
        virtual bool IsFinish();

        // 等待推理结束
        virtual void Wait();

        // 获取该层的所有输入数据块
        vector <Data<T>* >& GetInputs();

        // 获取该层的所有输出数据块
        vector <Data<T>* >& GetOutputs();

        // 获取该层的所有权重数据块
        vector <Data<T>* >& GetWeights();

        // 返回该层的第index个输出的float值
        float* GetFloatOutput(int index = 0);

        // 获取网络第index个输出到指定内存位置
        void GetFloatOutputTo(int index = 0, float* output = nullptr);

        // 返回该层的层名
        string GetName();

        // 返回该层的类型
        string GetType();

        virtual string GetPrintType();

        // 给定device, version, 计算该层预计耗时
        virtual double GetExceptedTime(std::string device, int version);

        virtual long long GetOperationTimes();
        virtual long long GetInputDataAmount();
        virtual long long GetOutputDataAmount();
        virtual long long GetWeightDataAmount();

        virtual void ResetInt8Config();
        virtual void Reshape();
        virtual void Prepare();
        virtual int GetWeightChannels();

        string ToJsonString();
        string ToJsonInputs();
        string ToJsonOutputs();
        virtual string ToJsonParams();
        virtual json11::Json ToJson();

        virtual string GetParams();
        vector <Data<float>*>& GetFloatWeights();

        void ClearInput();
        void ClearOutput();
        void ReplaceInput(string originalName, Data<T> *newData);
        void ReplaceOutput(string originalName, Data<T> *newData);
        void AddInput(Data<T> *input);
        void AddOutput(Data<T> *output);
        void AddWeight(Data<T> *weight);
        void AddUint8Attr(const string &name, Data<uint8_t> *attr);
        void AddFloatAttr(const string &name, Data<float> *attr);
        void AddInt32Attr(const string &name, Data<int> *attr);
        Data<uint8_t> *GetUint8Attr(const string &name);
        Data<float_t> *GetFloatAttr(const string &name);
        Data<int32_t> *GetInt32Attr(const string &name);
        int GetId();
        int GetTFACCId();
        int SetTFACCId(int tfaccId);

        //share data between input and output
        virtual void ForwardTF() = 0;
        bool GetShare();
        void SetShare(bool share);
        string GetOutputDataType();
        string GetWeightDataType();
        void SetLayerDataType(string type);
        void SetOutputDataType(string type);
        void SetForceOutputDataType(bool force);
        void SetWeightDataType(string type);

        void PrepareOutputs(vector<float *> &outputs);
        void PrepareInputsAndOutputs(vector<float *> &inputs, vector<float *> &outputs);
        void PrepareWeights();
        void PrepareConfig();
        void SyncDataType();
        void FinishOutputs(vector<float *> &outputs);
        void FinishInputsAndOutputs(vector<float *> &inputs, vector<float *> &outputs);
        void FinishWeights();
        void SetInt8Config(INT8Config *config);
        bool ForceOutputMin(float minValue);
        bool ForceOutputMax(float maxValue);
        virtual void SaveWeights(std::ofstream &outputFile);
        virtual void SaveExWeightsInfo(std::ofstream &outputFile);
        virtual void SaveTFLiteWeightsInfo(std::ofstream &outputFile);

        // 将这层的第index个weight转成浮点导出到floatWeight中
        virtual void FillFloatWeight(int index, vector <float> &floatWeight);

        //对于Conv, FC, Deconv层，需要满足inputConfig->GetScale() * weightConfig->GetScale() <= outputConfig->GetScale()，如果不满足这个条件那么需要重设outputConfig
        bool AdjustInt8Config();
        bool AdjustInt8ConfigKeepCenter(bool fixInput);

        void SetParallelCmd(PostCmds postCmds1);
        void DisParallelCmd();

        map <string,Data<uint8_t >* >& GetUint8AttrDatas();
        map <string,Data<float >* >& GetFloatAttrDatas();
        map <string,Data<int >* >& GetInt32AttrDatas();
        // Set attribute value without asking for new mem
        void ShareAttrData(Layer<T>* layer);
        void SetShareAttr(bool shareAttr);

        void PreForwardWithFixedFloatData();

        int batchpad = -1;
//#ifdef USE_TFACC
        virtual bool TFACCSupport();
        virtual void TFACCInit();
        virtual void TFACCReleaseDDR();
        virtual void TFACCMark();
        virtual void TFACCReshape();
        virtual void GetBlasops(vector <pair <void*, int> > &blasops);
        virtual void TFACCRelease();
        virtual void SetDeviceId(int deviceId);
        void JustSetDeviceId(int deviceId);

        virtual void SetMultiDeviceId(set <int> deviceIds); // 这个层需要执行在多个设备上
        virtual void SetRunOnMultiDevice(bool runOnMultiDevice); // 设置runOnMultiDevice，若为true，那么之后forward时这个层会被执行在多个设备上
        virtual bool CanRunOnMultiDevice(); // 是否可以运行在多个设备上

        virtual bool IsActivationLayer();
        virtual void GetMapTable(vector <uint8_t> &mapTable);
//#endif
        // yield_nop mode
        int yield_num = -1;
        int nop_num =0;
        bool isyieldnum = false;
        bool isnopnum = false;
        bool isnopscale = false;
        int nop_scale = 100;
        int forwardStatus = 0; // 异步执行时的状态号

        virtual void SaveMiniNet(std::ofstream &outputFile);
    protected:
        INT8Config* int8Config;
        int layerId;
        int TFACCId = -1;
        int deviceId = 0;
        vector <void*> blasopList;

        vector <vector <void*> > splitBlasopLists; // BS1 加速时，用于记录每个设备上不同的命令字列表
        vector <int> splitDeviceIds; // BS1 加速时，记录一组 blasopList 分别跑在哪些 device 上

        bool runOnMultiDevice = false; // 是否在多设备上运行

        string layerName;
        string layerType;

        vector <Data<T>* > inputs;
        vector <Data<T>* > outputs;
        vector <Data<T>* > weights;
        map <string,Data<uint8_t >* > Uint8AttrDatas;
        map <string,Data<float >* > FloatAttrDatas;
        map <string,Data<int >* > Int32AttrDatas;
        vector <Data<float>*> floatWeights;
        float* floatOutput= nullptr;

        bool forceOutputDataType = false;
        string layerDataType;
        string inputDataType;
        string outputDataType;
        string weightDataType;

        const float unlockMin = 1;
        const float unlockMax = -1;
        float forceMin = unlockMin;
        float forceMax = unlockMax;

        bool firstReshape = true;
        bool share = false;
        bool ShareAttr = false;
        // Parallel forward interface
        bool IsPost = false;
        PostCmds postCmds;

    };
}


#endif //CAFFE_LAYER_H
