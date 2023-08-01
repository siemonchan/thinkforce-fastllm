//
// Created by huangyuyang on 1/13/22.
//

#ifndef PROJECT_TFDL_CHECK_H
#define PROJECT_TFDL_CHECK_H

#include <map>
#include <vector>
#include <string>

#include "tfdl.api.h"
#include "Net.h"

using namespace std;

namespace tfdl {
    /// 检查器的日志等级
    enum CheckerLogLevel {
        LOG = 0, WARNING = 1, ERROR = 2, MUTE = 3
    };

    /// 检查的配置
    /// inputs :             输入数据集, 每一个vector <float> 中存放一个输入的数据
    ///                      输入数据需要紧凑排布，图像数据按NCHW格式排布
    ///                      该参数为空时使用默认输入
    /// logLevel :           指定输出到屏幕的信息的最低级别
    ///                      该参数为LOG时输出所有信息，为WARNING时输出警告和报错，为ERROR时只输出报错
    struct CheckConfig {
        vector<vector<float> > inputs;
        bool resetPerLayer;

        CheckerLogLevel logLevel = CheckerLogLevel::WARNING;
    };

    /// 检查的结果数据
    /// outputScore :    输出层平均分数
    /// scores :         每一层的分数
    /// log :            日志
    /// warning :        警告内容
    /// error :          报错内容
    struct CheckResult {
        float outputScore;
        std::map<std::string, float> scores;

        std::string log;
        std::string warning;
        std::string error;
    };

    /// 使用默认配置检查Caffe模型在TFDL上是否可以使用
    /// \param prototxtFile :   输入参数，caffe网络文件路径
    /// \param caffemodelFile : 输入参数，caffe模型文件路径
    /// \return :               一个CheckResult结构体描述检查的结果
    CheckResult CheckCaffeModel(const string &prototxtFile, const string &caffemodelFile);

    /// 使用自定义配置检查Caffe模型在TFDL上是否可以使用
    /// \param prototxtFile :   输入参数，caffe网络文件路径
    /// \param caffemodelFile : 输入参数，caffe模型文件路径
    /// \param config :         输入参数，检查的配置，若为nullptr则使用默认配置
    /// \return :               一个CheckResult结构体描述检查的结果
    CheckResult CheckCaffeModelEx(const string &prototxtFile, const string &caffemodelFile, CheckConfig *config);

    /// 使用默认配置检查TFDL模型
    /// \param netFile :        输入参数，TFDL网络文件路径
    /// \param floatFile :      输入参数，TFDL浮点模型文件路径
    /// \param int8File :       输出参数，TFDL定点模型文件路径
    /// \return :               一个CheckResult结构体描述检查的结果
    CheckResult CheckTFDLModel(const string &netFile, const string &floatFile, const string &int8File);

    /// 使用默认配置检查TFDL模型
    /// \param netFile :        输入参数，TFDL网络文件路径
    /// \param floatFile :      输入参数，TFDL浮点模型文件路径
    /// \param int8File :       输出参数，TFDL定点模型文件路径
    /// \param config :         输入参数，检查的配置，若为nullptr则使用默认配置
    /// \return :               一个CheckResult结构体描述检查的结果
    CheckResult CheckTFDLModelEx(const string &netFile, const string &floatFile, const string &int8File, CheckConfig *config);

    /// 使用默认配置检查TFDL模型
    /// \param netFile :        输入参数，TFDL网络文件路径
    /// \param int8File :       输出参数，TFDL定点模型文件路径
    /// \return :               一个CheckResult结构体描述检查的结果
    CheckResult CheckTFDLModel(const string &netFile, const string &int8File);

    /// 使用默认配置检查TFDL模型
    /// \param netFile :        输入参数，TFDL网络文件路径
    /// \param int8File :       输出参数，TFDL定点模型文件路径
    /// \param config :         输入参数，检查的配置，若为nullptr则使用默认配置
    /// \return :               一个CheckResult结构体描述检查的结果
    CheckResult CheckTFDLModelEx(const string &netFile, const string &int8File, CheckConfig *config);

    tfdl::Net <uint8_t> *CreateCacheBypassInt8Model(const string &netFile, const string &int8File);

    CheckResult CheckTFDLModelMul(tfdl::Net<float> *net, tfdl::Net<uint8_t> *netInt8, CheckConfig *config);
}

#endif //PROJECT_TFDL_CHECK_H
