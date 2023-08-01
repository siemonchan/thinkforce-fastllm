//
// Created by huangyuyang on 11/23/21.
//

#ifndef PROJECT_API_H
#define PROJECT_API_H

#include <string>
#include "Data.h"

#include "tfacc40t.h"
#include "tfdl.check.h"

using namespace std;

#define TFDL_MAJOR_VERSION 2
#define TFDL_MINOR_VERSION 3
#define TFDL_PATCH_LEVEL 2
#define TFDL_BUILD_NUMBER 0
#define TFDL_VERSION (TFDL_MAJOR_VERSION * 1000000 + TFDL_MINOR_VERSION * 1000 + TFDL_PATCH_LEVEL)

namespace tfdl {
    /// 描述模型IO类型的枚举变量
    /// U8 :    无符号8位整数
    /// FLOAT : 单精度浮点数
    /// DIRECTU8 : 无符号8位整数，Forward时不进行预处理、量化
    enum IODataType {
        U8 = 0,
        FLOAT = 1,
        DIRECTU8 = 2
    };

    struct ModelAnalysor {
        double recentForwardSpan;
    };

    /// 描述模型句柄的结构体
    /// dwSize :             dwSize, 需要在创建时赋值为 sizeof(ModelHandle), 防止内部对齐出错
    /// handleId :           句柄id
    /// inputSizePerBatch :  每个输入batch的元素个数
    /// outputSizePerBatch : [弃用] 请使用outputLength
    /// maxBatchNum :        最大batch数量
    /// inputDataType :      输入类型
    /// outputDataType :     输出类型
    /// inputs :             输入buffer，在调用创建模型函数中会分配，释放模型函数中会释放，inputs.size() = 输入数量
    ///                      需要在forward之前填入数据，写入数据时需要根据inputDataType将指针转换成相应类型
    /// inputNames :         每个输入块的名字，inputNames.size() = 输入数量
    /// inputLengths :       每个输入块的长度，inputLengths.size() = 输入数量
    /// inputChannelses :    每个输入块的通道数，inputChannelses.size() = 输入数量
    /// inputChannelSteps :  每个输入块的步长，inputChannelSteps.size() = 输入数量
    /// inputHeights :       每个输入块的高，inputHeights.size() = 输入数量
    /// inputWidths :        每个输入块的宽，inputWidths.size() = 输入数量
    /// inputBuffer :        输入buffer，等同于inputs[0]
    /// outputs :            输出buffer，在调用创建模型函数中会分配，释放模型函数中会释放，outputs.size() = 输出数量
    ///                      读取其中的数据时，需要根据outputDataType将指针转换成相应类型
    /// outputNames :        每个输出块的名字，outputNames.size() = 输出数量
    /// outputLengths :      每个输出块的长度，outputLengths.size() = 输出数量
    /// outputDims :         每个输出块的形状，outputDims.size() = 输出数量
    /// outputBuffer :       [弃用] 请使用outputs
    /// inputChannels :      输入通道数，等同于inputChannelses[0]
    /// inputChannelStep :   输入通道的步长，等同于inputChannelSteps[0]
    /// inputHeight :        输入画幅的高, 等同于inputHeights[0]
    /// inputWidth :         输入画幅的宽, 等同于inputWidths[0]
    /// outputLength :       [弃用] 请使用outputLength
    /// analysor :           分析器
    struct ModelHandle {
        int dwSize = sizeof(tfdl::ModelHandle);

        int handleId;
        int inputSizePerBatch;
        int outputSizePerBatch; // Deprecated
        int maxBatchNum = 1;
        IODataType inputDataType = IODataType::U8;
        IODataType outputDataType = IODataType::FLOAT;

        std::vector <void*> inputs;
        std::vector <std::string> inputNames;
        vector <int> inputLengths;
        vector <int> inputChannelses;
        vector <int> inputChannelSteps;
        vector <int> inputHeights;
        vector <int> inputWidths;

        char *inputBuffer;

        std::vector <void*> outputs;
        std::vector <std::string> outputNames;
        std::vector <int> outputLengths;
        std::vector <std::vector <int> > outputDims;

        char *outputBuffer; // Deprecated

        int inputChannels;
        int inputChannelStep;
        int inputHeight;
        int inputWidth;
        int outputLength; // Deprecated

        int threadsForOneTFACC = 1;
        ModelAnalysor analysor;
    };

    /// 描述异步流的结构体
    /// dwSize :             dwSize, 需要在创建时赋值为 sizeof(Stream), 防止内部对齐出错
    /// streamId :           流id
    struct Stream {
        int dwSize = sizeof(tfdl::Stream);

        int streamId;
    };

    /// 异步流中的事件
    /// dwSize :             dwSize, 需要在创建时赋值为 sizeof(Event), 防止内部对齐出错
    /// eventId :            事件id
    struct Event {
        int dwSize = sizeof(tfdl::Event);

        int eventId;
    };

    /// 其他框架模型转换到TFDL模型时使用的转换配置
    /// dwSize :             dwSize, 需要在创建时赋值为 sizeof(ConverterConfig), 防止内部对齐出错
    /// meanValues :         输入数据将的均值，预处理阶段将先将数据减去均值，如果不设置则使用默认均值
    ///                      可以为多个通道设置不同的均值
    /// scales :             输入数据将的缩放系数，如果不设置则使用默认系数
    ///                      可以为多个通道设置不同的scale
    /// quantizationSet :    量化数据集, 每一个vector <float> 中存放一个输入的数据
    ///                      输入数据需要紧凑排布，图像数据按NCHW格式排布
    ///                      如果不设置则使用默认图片进行量化
    struct ConverterConfig {
        int dwSize = sizeof(tfdl::ConverterConfig);

        vector <float> meanValues;
        vector <float> scales;

        vector <vector <float> > quantizationSet;
    };

    /// 获取SDK版本号
    /// \return SDK版本号
    int GetSDKVersion();

    /// 获取SDK版本字符串
    /// \return SDK版本字符串
    string GetSDKVersionString();

    /// 获取所有可用的设备ID
    /// \return : 所有可用的设备ID
    vector <int> GetAllDevices();

    /// 设置使用的TFACCID，设置之后会影响到以后创建的modelHandle
    /// \param deviceNum :   使用的设备数量
    /// \param deviceIds :   使用的设备id列表，长度至少为deviceNum，可用的设备ID可以通过GetAllDevices获取
    /// \return              一个TF_RET值代表运行成功，或失败的类型
    TF_RET SetDevicesGlobal(int deviceNum, int *deviceIds);

    /// 设置modelHandle使用的TFACC设备集合
    /// \param deviceNum :   使用的设备数量
    /// \param deviceIds :   使用的设备id列表，长度至少为deviceNum，可用的设备ID可以通过GetAllDevices获取
    /// \param modelHandle : 检索实例的句柄
    /// \return              一个TF_RET值代表运行成功，或失败的类型
    TF_RET SetDevices(int deviceNum, int *deviceIds, ModelHandle *modelHandle);

    /// 设置modelHandle中是否使用快速softmax
    /// \param openFastSoftmax
    /// \param modelHandle
    /// \return
    TF_RET SetFastSoftmax(bool openFastSoftmax, ModelHandle *modelHandle);

    /// 获取TFACC可用空间大小
    /// \param chipId : 待查询的芯片编号
    /// \return :       返回TFACC可用空间大小（字节数）
    uint64_t GetFreeSpaceSize(int chipId);

    /// 从onnx文件中读取并创建模型
    /// \param onnxFile :       输入参数，onnx模型文件路径
    /// \param modelHandle :    输出参数，创建的模型句柄
    /// \return :               一个TF_RET值代表运行成功，或失败的类型
    TF_RET CreateModelFromOnnx(const string &onnxFile, ModelHandle *modelHandle);

    /// 从onnx文件中读取并创建模型
    /// \param onnxFile :       输入参数，onnx模型文件路径
    /// \param modelHandle :    输出参数，创建的模型句柄
    /// \param ignoreLayers :       一个层类型的列表，读取模型时忽略这些类型的层
    /// \param converterConfig :    转换配置，传入 nullptr 则使用默认配置
    /// \return :                   一个TF_RET值代表运行成功，或失败的类型
    TF_RET CreateModelFromOnnxEx(const string &onnxFile,
                                  const vector <string> &ignoreLayers, ConverterConfig *converterConfig,
                                  ModelHandle *modelHandle);

    /// 从caffe文件中读取输入尺寸
    /// \param prototxtFile :   输入参数，caffe网络文件路径
    /// \param channels :       输出参数，caffe网络输入通道数
    /// \param height :         输出参数，caffe网络输入的高
    /// \param width :          输出参数，caffe网络输入的宽
    /// \return :               一个TF_RET值代表运行成功，或失败的类型
    TF_RET GetCaffeInputShape(const string &prototxtFile, int *channels, int *height, int *width);

    /// 从caffe文件中读取并创建模型
    /// \param prototxtFile :   输入参数，caffe网络文件路径
    /// \param caffemodelFile : 输入参数，caffe模型文件路径
    /// \param modelHandle :    输出参数，创建的模型句柄
    /// \return :               一个TF_RET值代表运行成功，或失败的类型
    TF_RET CreateModelFromCaffe(const string &prototxtFile, const string &caffemodelFile, ModelHandle *modelHandle);

    /// 从caffe文件中读取并创建模型
    /// \param prototxtFile :       输入参数，caffe网络文件路径
    /// \param caffemodelFile :     输入参数，caffe模型文件路径
    /// \param modelHandle :        输出参数，创建的模型句柄
    /// \param ignoreLayers :       一个层类型的列表，读取模型时忽略这些类型的层
    /// \param converterConfig :    转换配置，传入 nullptr 则使用默认配置
    /// \return :                   一个TF_RET值代表运行成功，或失败的类型
    TF_RET CreateModelFromCaffeEx(const string &prototxtFile, const string &caffemodelFile,
                                  const vector <string> &ignoreLayers, ConverterConfig *converterConfig,
                                  ModelHandle *modelHandle);
    /// 从caffe文件中读取并创建模型
    /// \param prototxtFile :   输入参数，caffe网络文件路径
    /// \param caffemodelFile : 输入参数，caffe模型文件路径
    /// \param modelHandle :    输出参数，创建的模型句柄
    /// \return :               一个TF_RET值代表运行成功，或失败的类型
    TF_RET CreateModelFromCaffe(const string &prototxtFile, const string &caffemodelFile, ModelHandle *modelHandle);

    /// 从caffe文件中读取并创建模型
    /// \param prototxtFile :       输入参数，caffe网络文件路径
    /// \param caffemodelFile :     输入参数，caffe模型文件路径
    /// \param modelHandle :        输出参数，创建的模型句柄
    /// \param ignoreLayers :       一个层类型的列表，读取模型时忽略这些类型的层
    /// \param converterConfig :    转换配置，传入 nullptr 则使用默认配置
    /// \return :                   一个TF_RET值代表运行成功，或失败的类型
    TF_RET CreateModelFromCaffeEx(const string &prototxtFile, const string &caffemodelFile,
                                  const vector <string> &ignoreLayers, ConverterConfig *converterConfig,
                                  ModelHandle *modelHandle);

    /// 从文件中读取并创建模型
    /// \param netFileName :   输入参数，网络文件路径
    /// \param modelFileName : 输入参数，模型文件路径
    /// \param modelHandle :   输出参数，创建的模型句柄
    /// \return :              一个TF_RET值代表运行成功，或失败的类型
    TF_RET CreateModelFromFile(const string &netFileName, const string &modelFileName, ModelHandle *modelHandle);

    /// 从文件中读取并创建模型
    /// \param netConfig :     输入参数，网络结构json序列化后的字符串 (既json文件中的内容)
    /// \param buffer :        输入参数，模型权重的首地址 (其中存储的按二进制读取bin文件之后的所有内容)
    /// \param len :           输入参数，buffer的长度
    /// \param modelHandle :   输出参数，创建的模型句柄
    /// \return :              一个TF_RET值代表运行成功，或失败的类型
    TF_RET CreateModelFromMemory(const string &netConfig, char* buffer, int len, ModelHandle *modelHandle);

    /// 将模型导出到文件
    /// \param netFileName :   输入参数，网络文件路径
    /// \param modelFileName : 输入参数，模型文件路径
    /// \param modelHandle :   输入参数，待导出的模型句柄
    /// \return :              一个TF_RET值代表运行成功，或失败的类型
    TF_RET ExportModel(const string &netFileName, const string &modelFileName, ModelHandle *modelHandle);

    /// 修改模型的输入尺寸
    /// \param modelHandle :   输入参数，执行推理的模型句柄
    /// \param inputShape :    输入参数，代表模型输入的尺寸，不包含batch维度，对于图像输入为CHW格式
    /// \return :              一个TF_RET值代表运行成功，或失败的类型
    TF_RET ReshapeModel(ModelHandle *modelHandle, const std::vector <int> &inputShape);

    /// 模型推理
    /// \param modelHandle :   输入参数，执行推理的模型句柄
    /// \param batch :         输入batch数量
    /// \return :              一个TF_RET值代表运行成功，或失败的类型
    TF_RET Forward(ModelHandle *modelHandle, int batch);

    /// 释放一个模型
    /// \param modelHandle : 输入参数，待释放的模型句柄
    /// \return :            一个TF_RET值代表运行成功，或失败的类型
    TF_RET FreeModel(ModelHandle *modelHandle);

    /// 设置是否打印分析信息
    /// \param profileMode : 输入参数，是否打印分析信息。如果设置为true，且Forward时batch = 1，则会输出性能信息
    /// \return :            一个TF_RET值代表运行成功，或失败的类型
    TF_RET SetProfileMode(bool profileMode);

    /// 设置是否按线程加锁
    /// \param isLockByDevice : 输入参数，是否按线程加锁
    /// \return :               一个TF_RET值代表运行成功，或失败的类型
    TF_RET SetLockByDevice(bool isLockByDevice);

    /// 设置是否打印shape信息
    /// \param shapeDebugMode : 输入参数，是否打印分shape信息。如果设置为true，则创建网络时会输出Shape信息
    /// \return :               一个TF_RET值代表运行成功，或失败的类型
    TF_RET SetShapeDebugMode(bool shapeDebugMode);

    /// 创建一个异步流
    /// \param stream :         输出参数，流句柄
    /// \return :               一个TF_RET值代表运行成功，或失败的类型
    TF_RET CreateStream(Stream *stream);

    /// 销毁一个异步流
    /// \param stream :         输入参数，流句柄
    /// \return :               一个TF_RET值代表运行成功，或失败的类型
    TF_RET DestroyStream(Stream *stream);

    /// 查询异步流状态
    /// \param stream :         输入参数，流句柄
    /// \return :               一个bool值代表异步流是否执行完毕
    bool StreamQuery(Stream *stream);

    /// 等待异步流执行完成
    /// \param stream :         输入参数，流句柄
    /// \return :               一个TF_RET值代表运行成功，或失败的类型
    TF_RET StreamSynchronize(Stream *stream);

    /// 异步模型推理
    /// \param modelHandle :   输入参数，执行推理的模型句柄
    /// \param batch :         输入batch数量
    /// \param stream :        输入参数，用于接受异步任务的流
    /// \return :              一个TF_RET值代表运行成功，或失败的类型
    TF_RET ForwardAsync(ModelHandle *modelHandle, int batch, Stream *stream);

    /// 创建一个事件
    /// \param event :          输出参数，事件句柄
    /// \return :               一个TF_RET值代表运行成功，或失败的类型
    TF_RET CreateEvent(Event *event);

    /// 销毁一个事件
    /// \param event :          输入参数，事件句柄
    /// \return :               一个TF_RET值代表运行成功，或失败的类型
    TF_RET DestroyEvent(Event *event);

    /// 在指定的流中插入一个事件
    /// \param event :          输入参数，事件句柄
    /// \param stream :         输入参数，流句柄
    /// \return :               一个TF_RET值代表运行成功，或失败的类型
    TF_RET EventRecord(Event *event, Stream *stream);

    /// 查询事件状态
    /// \param event :          输入参数，事件句柄
    /// \return :               一个bool值代表事件是否执行完毕
    bool EventQuery(Event *event);

    /// 等待事件执行完成
    /// \param event :          输入参数，事件句柄
    /// \return :               一个TF_RET值代表运行成功，或失败的类型
    TF_RET EventSynchronize(Event *event);

    /// 测量两个事件完成时间的间隔
    /// \param ms :             输出参数，两个事件的完成时间间隔（以毫秒为单位），若有事件未完成则此值无意义
    /// \param start :          输入参数，开始事件句柄
    /// \param end :            输入参数，结束事件句柄
    /// \return :               一个TF_RET值代表运行成功，或失败的类型
    TF_RET EventElapsedTime(float *ms, Event *start, Event *end);
}

namespace tfacc {
    /// 点对点加法类
    class Add {
    public:
        /// 构造函数，构造一个加法类
        /// \param input0Min : input0的范围最小值
        /// \param input0Max : input0的范围最大值
        /// \param input1Min : input1的范围最小值
        /// \param input1Max : input1的范围最大值
        /// \param outputMin : output的范围最小值
        /// \param outputMax : output的范围最大值
        /// \param len : 输入长度
        Add (float input0Min, float input0Max,
             float input1Min, float input1Max,
             float outputMin, float outputMax,
             int len);

        /// 析构函数
        ~Add();

        /// 执行加法操作
        /// \param input0 : 输入0
        /// \param input1 : 输入1
        /// \param output : 输出
        void Apply(uint8_t *input0, uint8_t *input1, uint8_t *output);

    private:
        void *handle;
    };

    /// 映射类
    /// 需要提供一个uint8映射表MapTable[256], 做映射output[i] = MapTable[input[i]]
    class Mapping {
    public:
        /// 构造函数，构造一个映射类
        /// \param MapTable : 长度为256的映射表
        /// \param len : 输入长度
        Mapping (uint8_t *mapTable, int len);

        /// 析构函数
        ~Mapping();

        /// 执行映射操作
        /// \param input : 输入
        /// \param output : 输出
        void Apply(uint8_t *input, uint8_t *output);

    private:
        void *handle;
    };
}

#endif //PROJECT_API_H




