//
// Created by yuyang.huang on 17-11-7.
//

#ifndef TFDL_COMMON_H
#define TFDL_COMMON_H

#include "json11.hpp"
#include "INT8Config.h"
#include "JsonConfigEditor.h"
#ifdef USE_TFACC
#include <tfblas_api.hpp>
#endif
#include <string>
#include <math.h>
#include <string.h>
#include <iostream>
#include <chrono>
#include <cassert>
#include <cstdlib>
#include <fstream>
#include <pthread.h>
#ifdef ARM
#include <arm_neon.h>
#endif
#include<set>
#ifdef MULTI_THREAD
#include <pthread.h>
#define THREAD_NUM 4
#define TFDL_THRESHOLD 160000
#endif
#include <exception>
#include <time.h>
#define INSTANTIATE_CLASS(classname) \
  char gInstantiationGuard##classname; \
  template class classname<float>; \
  template class classname<uint8_t>


#define TFCHECK_EQ(a, b) if((a)!=(b)){\
    string err = __FILE__ ;\
    string line = to_string(__LINE__);\
    err+=":";\
    err+=line;\
    err+=" "+ to_string(a) + "!=" + to_string(b);\
    throw_exception(err);\
}
#define TFCHECK_GT(a, b) if((a)<=(b)){\
    string err = __FILE__ ;\
    string line = to_string(__LINE__);\
    err+=":";\
    err+=line;\
    err+= " " + to_string(a) + "<=" + to_string(b);\
    throw_exception(err);\
}
#define TFCHECK_LT(a, b) if((a)>=(b)){\
    string err = __FILE__ ;\
    string line = to_string(__LINE__);\
    err+=":";\
    err+=line;\
    err+= " " + to_string(a) + ">=" + to_string(b);\
    throw_exception(err);\
};
#define TFCHECK_NE(a, b) if((a)==(b)){\
    string err = __FILE__ ;\
    string line = to_string(__LINE__);\
    err+=":";\
    err+=line;\
    err+= " " + to_string(a) + "!=" + to_string(b);\
    throw_exception(err);\
}
#define TFCHECK_LE(a, b) if((a)>(b)){\
    string err = __FILE__ ;\
    string line = to_string(__LINE__);\
    err+=":";\
    err+=line;\
    err+= " " + to_string(a) + ">" + to_string(b);\
    throw_exception(err);\
}
#define TFCHECK_GE(a, b) if((a)<(b)){\
    string err = __FILE__ ;\
    string line = to_string(__LINE__);\
    err+=":";\
    err+=line;\
    err+= " " + to_string(a) + "<" + to_string(b);\
    throw_exception(err);\
}

static std::set<string> TfaccLayer = {"Convolution","Pooling","Deconvolution"};
struct PostCmds{
    void* arg;
    PostCmds():arg(nullptr){}
    PostCmds(void *func):arg(func){};
};
class TFDLInitException : public exception{
public:
    const char* what() const throw(){
        return "CreatNet Error";
    }
};
class TFDLRunException : public exception{
public:
    const char* what() const throw(){
        return "RunNet Error";
    }
};
class TFDLIoException : public exception{
public:
    const char* what() const throw(){
        return "SET/GET Data from tfdl Error";
    }
};
class TLOG{
public:
    TLOG(){};
    virtual void TFLOG(std::string err){
        struct tm *ptr;
        time_t lt;
        lt =time(NULL);
        ptr=gmtime(&lt);
        string tim = asctime(ptr);
        tim[tim.size()-1] = ' ';
        printf("[%s]: %s\n",tim.c_str(),err.c_str());
    }
};
namespace tfdl {
    static bool highAccuracyDoubleMultiplier;

    static void HighAccuracyDoubleMultiplier() {
        highAccuracyDoubleMultiplier = true;
    }

    static void NormalDoubleMultiplier() {
        highAccuracyDoubleMultiplier = false;
    }

    static TLOG tlogInstance;
    static TLOG *tlog = &tlogInstance;

    struct singleInputProfile{
        vector<float> scale;
        vector<float> mean_value;
        vector<int> input_shape;
        bool force_grey = false;
        bool reverse_input = false;

        int input_size() {
            int size = 1;
            for (int s : input_shape) {
                size *= s;
            }
            return size;
        }
    };

    static const vector<string> need_trim_vec{
            "Reshape",
            "PriorBox",
            "ChannelWise",
            "Mean",
            "Permute",
            "Upsample",
            "Flatten",
            "InnerProduct",
            "Python",
            "Custom",
            "Deconvolution",
            "ExpandDims",
            "SliceLike",
            "Tile",
            "Repeat",
            "Arange",
            "BoxNMS",
            "BroadcastOp",
            "Unary",
            "BinaryOp",
            "SliceAxis",
            "ReduceArgs",
            "ReflectionPadding"
    };

    static bool need_trim(const string &type) {
        for (auto &t : need_trim_vec) {
            if (type == t) {
                return true;
            }
        }
        return false;
    }
}
static void throw_exception(std::string err,exception* exception1) {
    tfdl::tlog->TFLOG(err);
    throw exception1;
};
static void throw_exception(std::string err) {
    tfdl::tlog->TFLOG(err);
};

#ifdef XM
template <typename T>
static string to_string(T a) {
    return "";
}
#endif

static double GetSpan(std::chrono::system_clock::time_point time1, std::chrono::system_clock::time_point time2) {
    auto duration = std::chrono::duration_cast<std::chrono::microseconds> (time2 - time1);
    return double(duration.count()) * std::chrono::microseconds::period::num / std::chrono::microseconds::period::den;
};

namespace tfdl {
    struct LoggerTable {
        string type, size, device;
        float time, ops, speed;

        LoggerTable (const string &type, const string &size, float time, float ops, float speed, const string &device) :
                type(type), size(size), time(time), ops(ops), speed(speed), device(device) {}
    };

    struct Logger {
        bool writeLog = false;
        bool writeToScreen = false;
        string cache = "";
        vector <LoggerTable> table;

        void WriteLog(const string &log);
        void WriteTable(const string &type, const string &size, float time, float ops, const string &device);
    };
}




#endif //CAFFE_COMMON_H
