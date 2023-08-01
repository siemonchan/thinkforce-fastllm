//
// Created by yuyang.huang on 17-11-10.
//

#ifndef TFDL_NET_H
#define TFDL_NET_H

#include <typeinfo>
#include <iostream>
#include <fstream>
#include <thread>
#include <mutex>
#include <unistd.h>
#include "common.h"
#include "Layer.h"
#include "Model.h"
#include "schema/tfdl_generated.h"
#include "layer/IPlugin.h"
#include "caffe.h"

#if defined(USE_TFACC) || defined(USE_TFCore)
#include <tfblas_api.hpp>
#endif

#ifdef USE_OPENCV
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#endif


namespace tfdl {
    struct DisjointSet {
        vector<int> father;

        void resize(int size) {
            father.clear();
            for (int i = 0; i < size; i++) {
                father.push_back(i);
            }
        }

        int find(int i) {
            return father[i] == i ? i : find(father[i]);
        }

        void merge(int a, int b) {
            a = find(a), b = find(b);
            father[a] = b;
        }
    };

    template<typename T>
    class Net {
    public:
        Net(const json11::Json &config, string dataType, void *sourceNet = nullptr) : dataType(dataType) {
#ifndef _WINDOWS
            Init(config, dataType, sourceNet);
#endif // !_WINDOWS

        }

        ~Net();


        Net(string dataType) : dataType(dataType){
            Init(dataType);
        }

        // 初始化一个空的网络对象
        void Init(string dataType);
#ifdef TFLITE
        // 向空的网络添加一个层
        void AddLayer(const json11::Json &layerConfig, std::vector<std::pair<int, const void *>> weights,
					  std::vector<std::vector<tfdl::QuantizationConfig>> weightQuantization,
                      std::vector<std::vector<tfdl::QuantizationConfig>> outputQuantization);

        // 确定网络的输入,输出
		void IdentifyInputOutput(string dataName, std::vector<int> shape,
								 tfdl::QuantizationConfig inputQuantization,
								 float Scale, long Mean, bool reverseInputData);

#endif //TFLITE

		// 将网络推演完后需要获取的data块标记出来，避免其内容在内存节省模式下被复写
		void IdentifyOutputData(const vector<string> &names, int memId = 0);

        // 获得输入数据块的名称
        string &GetInputDataName(int i = 0);

        // 获得第o个输出数据块的名称
        string &GetOutputDataName(int o = 0);

        // 获得输出数据块的数量
        int GetOutCount();

        // 获得输入数据块的数量
        int GetInCount();

        // 获得输入数据块
        Data<T> *GetInputData(int i = 0);

        Data<T> *GetInputData(const string &name);

        int GetInputCount();

        // 获得输入数据块的通道数
        int GetInputChannels(int i = 0);

        int GetInputChannels(const string &name);

        // 获得输入数据块的高度
        int GetInputHeight(int i = 0);

        int GetInputHeight(const string &name);

        // 获得输入数据块的宽度
        int GetInputWidth(int i = 0);

        int GetInputWidth(const string &name);

        singleInputProfile GetInputProfile(int i = 0);

        singleInputProfile GetInputProfile(const string &name);

        // 前向推理整个网络
        void ForwardAll();

        // 前向推理网络的第layerId层
        void ForwardLayer(int layerId);

        // 将网络的batch数调整为batchNum
        void ReshapeBatchNum(int batchNum, int memId = 0);

        // 获得网络的第o个输出数据块
        Data<T> *GetOutputData(int o = 0);

        Data<T> *GetOutputData(const string &name);

        // 获得网络中名为LayerName的Layer
        Layer<T> *GetLayer(string LayerName);

        // 获得网络中名为DataName的Data
        Data<T> *GetData(string DataName);

        // 将float型数据input作为网络的输入
        void SetFloatInputData(float *input, int index = 0);

        void SetFloatInputDataByName(float *input, const string &name);

        // 将inputs中的输入依次作为网络每一个batch的输入
        void SetBatchFloatInputData(vector<float *> &inputs, int index = 0);

        void SetBatchFloatInputDataByName(vector<float *> &inputs, const string &name);

        // 将uint8型数据input作为网络的输入
        void SetUint8InputData(const uint8_t *input, int index = 0);

        void SetUint8InputDataByName(const uint8_t *input, const string &name);

        // 将inputs中的输入依次作为int8网络每一个batch的输入
        void SetBatchUint8InputData(vector<magicType *> &inputs, int index = 0);

        void SetBatchUint8InputDataByName(vector<magicType *> &inputs, const string &name);

        // 获取网络第index个输出到指定内存位置
        void GetFloatOutputDataTo(int index = 0, float* output = nullptr);

        void GetFloatOutputDataTo(const string &name, float *output = nullptr);

        // 获取第index个输出的float数据首地址
        float *GetFloatOutputData(int index = 0);

        float *GetFloatOutputDataByName(const string &name);

        // 获取第o个输出的uint8数据首地址
        uint8_t* GetInt8OutputData(int index = 0);

        uint8_t *GetInt8OutputDataByName(const string &name);

        // 获取第o个输出的所有batch的数据，并依次存入vector中
        const vector<float *> &GetBatchFloatOutputData(int o = 0);

        const vector<float *> &GetBatchFloatOutputDataByName(const string &name);

        // 设置网络的打印信息级别，0为无打印，1为打印统计信息，2为打印详细信息
        void SetPrintTimeInfo(int print);

        //创建一个相同的网络，共用权重信息
        Net<T>* Copy(int memId = 0);

        void ShareAttr(Net<T> *net);

        void SetMode(string type, int mode);

        //0 for tflite, 1 for leftshift = 20, 2 for leftshift = 6
        void SetEltwiseMode(int mode);

        //0 for normal, 1 for faster
        void SetSoftmaxMode(int mode);

        //0 for cpu, 1 for tfacc
        void SetInnerProductMode(int mode);

        //0 for tfacc0, 1 for tfacc1
        void FreeSingleTFACC(int tfaccId);

        bool GetHighAccuracyDoubleMultiplier();

        void SetHighAccuracyDoubleMultiplier();

        void Init(const json11::Json &config, string dataType, void* sourceNet = nullptr);

        string MakeNewDataName(string baseName, string suffix);

        void DeleteData(Data<T> *data);

        void ForwardOneLayer(int layerId);

        void ForwardAll(bool updateInt8Config);

        void ForwardTwice();

        void ForwardFromTo(int from, int to, bool updateInt8Config = false);

        void ClearRealSpace();

        void Reshape(bool doMalloc = true, int memId = 0);

        void LoadWeights(string fileName);

        void LoadWeights(char *weightBin, int fend);

        void LoadWeights(CaffeModel &model);

        void LoadRawWeight(char *rawData, int len);

        void LoadQuantizationWeights(string fileName);

        void LoadQuantizationWeights(char* weightBin, int fend);

        void LoadSerialWeights(char* weightBin, int fend);

        void LoadSerialWeights(string fileName);

        Int8Model* ExportInt8Model();

        void ImportInt8Model(Int8Model *model);

        string ToJsonString();

        void SaveCaffeModel(CaffeModel &model);

        void SaveJson(string fileName);

        void SaveVisualQuantizeConfig(string fileName, bool skipWeight = false);

        void SaveWeights(string fileName, bool fromTFLite = false);

        void SaveSerialWeights(string fileName);

        void GetFlatbufferBuilder(flatbuffers::FlatBufferBuilder &fbb);

        int GetOutputCount(int o = 0);

        int GetOutputCount(const string &name);

        INT8Config *GetInt8Config();

        vector <Net<T> *> SplitByLayerName(const string &layerName); // 按一个层把模型拆成两部分
        vector <Layer<T> *> &GetLayers();

        //merge some layer
        void MergeInt8Config();

        //merge some layer's scale (runtime)
        void MergeInt8Scale();

        void SetInt8Config(INT8Config *config);

        void SetInt8Config();

        void UpdateInt8Config();

        void UpdateInt8WeightConfig();

        void UpdateInt8DataConfig();

        void UpdateInt8DataConfig(Data<T> *data);

        void FixInt8DataConfig(Layer<T> *layer);

        void FixInt8DataConfig(Data<T> *data);

        void RevertInt8DataConfig();

        void RevertInt8DataConfig(Data<T> *data);

        void MergeWeight(string mainWeight, string anotherWeight,
                         int step = -1); //merge weight for two layer, and remove second layer
#ifdef USE_TFACC
        //when use int32 bias in convolution directly, change useInt32Bias to true
        void TFACCInit();
        void GetBlasops(vector <pair <void*, int> > &blasops);
#endif
#ifdef USE_TFCore
        void TFACCInit();
#endif
#ifdef USE_TFACC40T
        void TFACCInit();
        void TFACCReshape();
        void TFACCReleaseDDR();
        void SetDeviceId(int deviceId);
        void JustSetDeviceId(int deviceId);
        void SetMultiDeviceId(set <int> devicesIds);
        void SetMultiThreadNum(int threadNum);
        int deviceId = 0;
        int threadNum = 1;
        vector <vector <bool> > needInvalid; // needInvalid[i][j]代表第i层的第j个输出是否需要invalid
#endif

        void SetMean(vector<float> means, int index = 0);

        void SetFrugalMode(bool frugalMode);

#ifdef USE_OPENCV
        void resizeInputImg(int num_channels_, cv::Mat &img, cv::Mat &resultImg, int targetHeight, int targetWidth) {
            /* Convert the input image to the input image format of the network. */
            cv::Mat sample;
            if (img.channels() == 3 && num_channels_ == 1)
                cv::cvtColor(img, sample, cv::COLOR_BGR2GRAY);
            else if (img.channels() == 4 && num_channels_ == 1)
                cv::cvtColor(img, sample, cv::COLOR_BGRA2GRAY);
            else if (img.channels() == 4 && num_channels_ == 3)
                cv::cvtColor(img, sample, cv::COLOR_BGRA2BGR);
            else if (img.channels() == 1 && num_channels_ == 3)
                cv::cvtColor(img, sample, cv::COLOR_GRAY2BGR);
            else
                sample = img;
            cv::Size input_geometry_ = cv::Size(targetWidth, targetHeight);
            cv::Mat sample_resized;
            if (sample.size() != input_geometry_)
                cv::resize(sample, sample_resized, input_geometry_);
            else
                sample_resized = sample;
            if (num_channels_ == 3)
                sample_resized.convertTo(resultImg, CV_32FC3);
            else
                sample_resized.convertTo(resultImg, CV_32FC1);
        }

        void batchPreprocessInputData(int channels, int inputHeight, int inputWidth, const vector<cv::Mat> &img, bool reverse_channel = false) {
            int height = inputHeight;
            int width = inputWidth;
            int cnt = channels * height * width;
            vector<float*> input_layer;
            for (int i = 0; i < (int)img.size(); i++) {
                input_layer.push_back(new float[cnt]);
                if (channels == 3 || channels == 1) {
                    std::vector<cv::Mat> input_channels;
                    for (int j = 0; j < channels; ++j) {
                        cv::Mat channel(height, width, CV_32FC1, input_layer.back());
                        input_channels.push_back(channel);
                        input_layer.back() += width * height;
                    }

                    input_layer.back() -= cnt;
                    if (reverse_channel){
                        std::reverse(input_channels.begin(), input_channels.end());
                    }
                    cv::split(img[i], input_channels);
                }
            }

            this->SetBatchFloatInputData(input_layer);
            for (int i = 0; i < input_layer.size(); i++) {
                delete[] input_layer[i];
            }
            input_layer.clear();
        }
#endif

        bool SetInputImg(string imgFileName, bool swap_rgb = false) {
#ifdef USE_OPENCV
            if (!(access (imgFileName.c_str(), F_OK) == 0)) {
                printf("File %s not found.\n", imgFileName.c_str());
                return false;
            }

            int inputHeight = this->GetInputHeight();
            int inputWidth = this->GetInputWidth();
            cv::Mat img = cv::imread(imgFileName.c_str(), -1);
            if (swap_rgb && img.channels() == 3) {
                cv::cvtColor(img, img, cv::COLOR_BGR2RGB);
            }
            cv::Mat inputImg;
            resizeInputImg(this->GetInputChannels(), img, inputImg, inputHeight, inputWidth);
            std::vector<cv::Mat> inputs;
            for (int i = 0; i < 1; i++) {
                inputs.push_back(inputImg);
            }

            this->batchPreprocessInputData(this->GetInputChannels(), inputHeight, inputWidth, inputs);
#else
            throw_exception("If you want use function \"Net::setInputImg\", you must add macro definition \"USE_OPENCV\" in compile options. (Try use \"-DUSE_OPENCV\")",new TFDLIoException());
#endif
            return true;
        }

#ifdef USE_OPENCV
        void setInputMat(cv::Mat &img) {
            int inputHeight = this->GetInputHeight();
            int inputWidth = this->GetInputWidth();
            cv::Mat inputImg;
            resizeInputImg(this->GetInputChannels(), img, inputImg, inputHeight, inputWidth);
            std::vector<cv::Mat> inputs;
            for (int i = 0; i < 1; i++) {
                inputs.push_back(inputImg);
            }

            this->batchPreprocessInputData(this->GetInputChannels(), inputHeight, inputWidth, inputs);
        }
#endif
        void addDataName(const string &dataName, const string &dataType);

        void SetHybridMode(bool hybridMode);

        void SetNonuniformTable(string dataName, vector<int> table);

        void SaveMiniNet(std::ofstream &outputFile);

        void useReverseInput(int index);
    protected:
        void *sourceNet; //源网络

        bool needClear = true; //copy出来的Net有一部分内容不需要清除

        vector<Layer<T> *> layers;

        map<string, Layer<T> *> layersDict;
        map<string, singleInputProfile> inputProfile;
        bool autoFeedOutput = false;
        vector<Data<T> *> inputData;
        vector<Data<T> *> outputData;
        vector<string> inputDataName;
        vector<string> outputDataName;
        vector<string> inputLayerDescription;

        vector<Data<T> *> datas;

        vector<string> dataNames;
        map<string, int> dataNameToIdDict;
        map<string, string> realDataName;
        map<string, Layer<T> *> lastLayer; // (dataName -> 以这个data作为输入的最后一个layer)
        map<string, Layer<T> *> firstLayer; // (dataName -> 以这个data作为输出的第一个layer)

        INT8Config *netInt8Config = new INT8Config();
        map<string, QuantizationConfig> originalConfig;

        DisjointSet disjointSet;

        string dataType;
        map<string, float *> realOutput;
        vector<vector<float *>> batchOutputs;

        bool frugalMode = false; //节俭模式下不会预先reallocate空间，只有需要时才会分配，内存在不需要使用之后就会delete掉
        int printTimeInfo = 0;

        vector <bool> realSpaceType;
#ifndef USE_TFACC
        T** realSpace = nullptr;
#else
        tfblas::MmapBuf<magicType>** realSpace;
#endif
        int realSpaceSize = 0;

        bool hybridMode = false; //是否为混合模式

        void useScale(tfdl::Data<T> *input_layer);

        void useScale(float *data, int batchNum = 1, int index = 0);

        void useMean(tfdl::Data<T> *input_layer);

        void useMean(float *data, int batchNum = 1, int index = 0);

        bool sameScaleAs(singleInputProfile &profile, float x);

        bool sameZeroPointAs(singleInputProfile &profile, uint8_t x);

        bool hasSameData(vector<Data<T> *> x, vector<Data<T> *> y);

        tfdl_fb::DataType DataTypeStringToFlatbuffer(string dataType);

        bool inOutputData(const string &name);

        // yield_nop mode
        int yield_num = -1;
        int nop_num =0;
        int nop_scale = 100;
        bool high_acc_multiplier = false;
#ifdef debug
        void print_debug_info();

#endif
    };

    class HybridNet : public Net<uint8_t> {
    public:
        HybridNet(const json11::Json &config, void *sourceNet = nullptr);
    };
    HybridNet *CreateHybridNet(const string &netFile, const string &modelFile, bool frugalMode = true, int tfaccId = 0);

    template<typename T>
    class Optimizer : public Net<T>{
    public:
        Optimizer(const json11::Json &config, string dataType) : Net<T>(config, dataType) {
            this->do_alignment = config["doAlignment"].is_null() ?
                                 true : config["doAlignment"].bool_value();
            this->merge_conv_bn_sc_relu = config["mergeConvBnScRelu"].is_null() ?
                                          true : config["mergeConvBnScRelu"].bool_value();
            this->merge_eltwise_relu = config["mergeEltwiseRelu"].is_null() ?
                                       true : config["mergeEltwiseRelu"].bool_value();
            this->merge_swish = config["mergeSwish"].is_null() ?
                                true : config["mergeSwish"].bool_value();
            this->merge_all_activation = config["mergeAllActivation"].is_null() ?
                                         true : config["mergeAllActivation"].bool_value();
            this->replace_broadcast_eltwise = config["replaceBroadcastEltwise"].is_null() ?
                                              true : config["replaceBroadcastEltwise"].bool_value();
            this->replace_conv_fc = config["replaceConvFC"].is_null() ?
                                    true : config["replaceConvFC"].bool_value();
            this->remove_inplace_layer = config["removeInplaceLayer"].is_null() ?
                                         true : config["removeInplaceLayer"].bool_value();
            this->remove_useless_layer = config["removeUselessLayer"].is_null() ?
                                         true : config["removeUselessLayer"].bool_value();
        }

#ifdef TFLITE
        Optimizer(string dataType):Net<T>(dataType){

        }
#endif
        void Optimize(bool merge_layer = true, bool doAlignment = false, bool doBatch = false, bool mergeQLayer = false,
                      bool high_accuracy = false, bool printInfo = false);
        void AddFloatLayer(bool printInfo, bool autoMode);
        void ReplaceGlobalPoolingWithMean(bool printInfo, bool autoMode);
        void DoAlignment(bool printInfo);
        void DoBatch(bool printInfo);
        void MergeLayer(bool printInfo);
        void MergeQuantizedLayer(bool printInfo);

        // TODO : rewrite the net interface
        void ReshapeBatchNum(int batchNum, int memId = 0);

        // TODO : rewrite the net ForwardAll
        void ForwardAll();

        // TODO : Set Uint8 Input to Optimizer
        void SetBatchUint8InputData(vector<magicType *> inputs);

        //TODO : Copy the Optimzer object
        Optimizer<T>* Copy(int memid = 0);

        // TODO : Reference an imagelist and print output
        void Classification(string imglist,bool PrintInfo);

        // TODO : Just run all cmds to test speed , and debug cmd queue built by compiler
        void BenchMark(bool PrintInfo);


        /* TODO: Init optimize' cmd queue, and split cpu layer with splitconfig file
         @param WindowSize : set the slide windowsize
         @param Maxsize : set the max length of queue for slide window
         @param splitconfig : add json file for compiler to split cpu layer
         @param memid : set the optimize ddr id */
        void InitAllCmds(int WindowSize,int Maxsize,json11::Json& splitconfig,int memid);

        /* TODO: Init optimize' cmd queue, and split cpu layer with cost of time of each layer
         @param WindowSize : set the slide windowsize
         @param Maxsize : set the max length of queue for slide window
         @param memid : set the optimize ddr id
         @param threshold : set the threshold for Compiler to decide if split cpu layer when a TFACC layer not bigger than cpu layer*/
        void InitAllCmds(int WindowSize,int Maxsize,int memid,float threshold=1);

        /* TODO : Init optimize' cmd queue with the Copy
         @param: Allcmds Copied from other Optimizer
         @param: fitbatch Copied from other Optimzer
         @param: layerstime Copied from other Optimizer
         @param: memid : the ddr id*/
        void InitAllCmds(vector<pair<int,int>>& allcmds,int fitbatch,vector<float>&layerstime, int memid = 0);

        //TODO : Save one fast cmds queue for next running
        void SaveCmds(string fileName);


        ~Optimizer(){
            for(auto net : nets){
                delete net;
            }
            for(auto layer : Splitlayers){
                delete layer;
            }
        }

    private:
        vector<Layer<T> *> NextLayer(int index);
        vector<Layer<T> *> LastLayer(int index);
        bool GenCmdsQueue(int windowsize,int Maxsize,json11::Json& splitconfig); // build layer forward cmd queue with batch > 1;
        bool GenCmdsQueue(int windowsize,int Maxsize); // build layer forward cmd queue with batch > 1;
        void GenRunCmds(bool equation = true);
        void GenIOCmds();
        bool SwapConvFC();
        bool MergeAllReLU();
        bool SplitLayer(Layer<T>*layer,vector<float>splits,vector<Layer<T>*>&splitlayers);
        void HasPathFromTo(string st, string end);
        bool IsUniqueUsage(string dataName);
        bool MergeZeroPadding();
        map<string, vector<string>> layerDependency;

        vector<pair<int,int>> AllCmds;
        vector<pair<pair<int,Layer<T>*>,pair<int,Layer<T>*>>> RunCmds;
        vector<pair<int,int>>INCmds;
        vector<pair<pair<int,int>,int>>OUTCmds;
        vector<float> layerstimes;
        vector<Layer<T>*> Splitlayers;
        vector<Layer<T>*> Mergelayers;
        map<string,int> layeriddict;
        vector<Net<T>*>nets;
#ifdef USE_TFACC
        tfblas::MmapBuf<magicType >* originInput;
        vector<tfblas::MmapBuf<magicType >* > originOutputs;
#else
        T* originInput;
        vector<T* > originOutputs;
#endif
        int  WindowSize = 1;
        int Maxsize;
        float _Threshold = 0;
        int fitbatch;
        int memid = 0;
        mutex InputLock;
        mutex OutputLock;
        thread* Serverthread = nullptr;
        bool serverstart = false;
        bool serverstop = false;
        bool serverhangup = false;
        bool serverfinfish = false;
        vector<uint8_t *>inputbuff;
        int inputid = 0;
        vector<vector<vector<float >>>outputbuff;

        // optimize controller
        bool do_alignment = true;
        bool merge_conv_bn_sc_relu = true;
        bool merge_eltwise_relu = true;
        bool merge_swish = true;
        bool merge_all_activation = true;
        bool replace_broadcast_eltwise = true;
        bool replace_conv_fc = true;
        bool remove_inplace_layer = true;
        bool remove_useless_layer = true;
    };

    bool IsFileExist(const char *path);

    template<typename T>
    Net<T> *CreateNetFromContent(const string &netConfig, char* netBin, int len,
                                 bool frugalMode = true, bool optimizeNet = true, int tfaccId = 0, int threadNum = 1) {
        string netDataType;
        if (typeid(T).name() == typeid(uint8_t).name()) {
            netDataType = "int8";
            //return CreateNet<T>(netFile, modelFile, "int8", frugalMode, optimizeNet);
        } else if (typeid(T).name() == typeid(float).name()) {
            netDataType = "float";
        } else {
            throw_exception("Create net failed: unsupport type.\n",new TFDLInitException());
            return nullptr;
        }

        string error;
        auto config = json11::Json::parse(netConfig, error);
        if (!error.empty()) {
            throw_exception("Parse json file failed: "+error,new TFDLInitException());
        }
        tfdl::Optimizer<T> *ret = new tfdl::Optimizer<T>(config, netDataType);
#ifdef debug
        printf("Created net config\n");
#endif

#ifdef _WINDOWS
        ret->Init(config, netDataType);
#endif // _WINDOWS

        if (netDataType == "float") {
            ret->LoadWeights(netBin, len);
        } else {
            ret->LoadQuantizationWeights(netBin, len);
        }

#ifdef debug
        printf("Finished read weight file \n");
#endif

#if defined(USE_TFACC) || defined(USE_TFCore)
        if (netDataType == "int8") {
            tfblas::TFTryInit();
        }
#endif
        ret->SetFrugalMode(frugalMode);
#ifdef debug
        printf("start Reshape\n");
#endif

        if (optimizeNet) {
            ret->Reshape(false);
            ret->Optimize(true, true, false, true, false, false);
            ret->Reshape(true, tfaccId);
        } else {
            ret->Reshape(true, tfaccId);
        }

        while (true) {
            bool modify = false;
            for (int i = 0; i < (int) ret->GetLayers().size(); i++) {
                modify |= ret->GetLayers()[i]->AdjustInt8Config();
            }

            if (!modify) {
                break;
            }
        }

        if (ret->GetHighAccuracyDoubleMultiplier()) {
            ret->SetHighAccuracyDoubleMultiplier();
        }
#if defined(USE_TFACC) || defined(USE_TFCore) || defined(USE_TFACC40T)
        ret->JustSetDeviceId(tfaccId);
        ret->SetMultiThreadNum(threadNum);
        if (netDataType == "int8") {
            ret->TFACCInit();
        }
#endif

        return (Net<T> *) ret;
    }

    template<typename T>
    Net<T> *CreateNet(const string &netFile, const string &modelFile, const string &netDataType,
                      bool frugalMode = true, bool optimizeNet = true, int tfaccId = 0, int threadNum = 1) {
        assert(netDataType == "int8" || netDataType == "float");
        if ((sizeof(T) == 4 && netDataType == "float") || (sizeof(T) == 1 && netDataType == "int8"));
        else {
            throw_exception("the defined net type isn't same as the param \"netDataType\"",new TFDLInitException());
        }

        if (!IsFileExist(netFile.c_str())) {
            throw_exception("Net file \"" + netFile + "\" not found.\n",new TFDLInitException());
        }

        if (!IsFileExist(modelFile.c_str())) {
            throw_exception("Model file \"" + modelFile + "\" not found.\n",new TFDLInitException());
        }

        ifstream t(netFile.c_str());
        string netString((std::istreambuf_iterator<char>(t)),
                         std::istreambuf_iterator<char>());
        t.close();

        FILE *weightfile = nullptr;
        weightfile = fopen(modelFile.c_str(), "rb");
        if (weightfile == nullptr) {
            throw_exception("Can't open model file.\n",new TFDLInitException());
        }
        fseek(weightfile, 0L, SEEK_END);
        int fend = ftell(weightfile);
        fseek(weightfile, 0L, SEEK_SET);
        char *weightBin = new char[fend];
        if (fread(weightBin, sizeof(char), fend, weightfile) != fend) throw_exception("read bin file error\n",new TFDLInitException());
        fclose(weightfile);
        weightfile = nullptr;
        Net<T> * ret = CreateNetFromContent<T>(netString, weightBin, fend, frugalMode, optimizeNet, tfaccId, threadNum);
        delete[] weightBin;
        return ret;
    }

    template<typename T>
    Net<T> *
    CreateNetSerial(const string &netFile, const string &modelFile, bool frugalMode = true, bool optimizeNet = true,
                    int tfaccId = 0, int threadNum = 1) {
        string netDataType;
        if (typeid(T).name() == typeid(uint8_t).name()) {
            netDataType = "int8";
        } else if (typeid(T).name() == typeid(float).name()) {
            netDataType = "float";
        }

        ifstream t(netFile.c_str());
        string netConfig((std::istreambuf_iterator<char>(t)),
                         std::istreambuf_iterator<char>());
        t.close();
        string error;
        auto config = json11::Json::parse(netConfig, error);
        if (!error.empty()) {
            throw_exception("Parse json file failed: " + error, new TFDLInitException());
        }
        tfdl::Optimizer<T> *ret = new tfdl::Optimizer<T>(config, netDataType);
        ret->LoadSerialWeights(modelFile);
#ifdef debug
        printf("Finished read weight file \n");
#endif

#if defined(USE_TFACC) || defined(USE_TFCore)
        if (netDataType == "int8") {
            tfblas::TFTryInit();
        }
#endif
        ret->SetFrugalMode(frugalMode);
#ifdef debug
        printf("start Reshape\n");
#endif

        if (optimizeNet) {
            ret->Reshape(false);
            ret->Optimize(true, true, false, true, false, false);
            ret->Reshape(true, tfaccId);
        } else {
            ret->Reshape(true, tfaccId);
        }

        while (true) {
            bool modify = false;
            for (int i = 0; i < (int) ret->GetLayers().size(); i++) {
                modify |= ret->GetLayers()[i]->AdjustInt8Config();
            }

            if (!modify) {
                break;
            }
        }
#if defined(USE_TFACC) || defined(USE_TFCore) || defined(USE_TFACC40T)
        ret->SetMultiThreadNum(threadNum);
        if (netDataType == "int8") {
            ret->TFACCInit();
        }
#endif

        return (Net<T> *) ret;
    }

    // 用网络文件netFile和模型文件modelFile创建一个网络，frugalMode表示是否使用内存节俭模式，optimizeNet表示是否优化网络
    template <typename T>
    Net <T> *CreateNet(const string &netFile, const string &modelFile, bool frugalMode = true, bool optimizeNet = true, int tfaccId = 0, int threadNum = 1) {
        if (modelFile.substr(modelFile.size() - 2) == "fb") {
            return CreateNetSerial<T>(netFile, modelFile, frugalMode, optimizeNet, tfaccId, threadNum);
        } else if (typeid(T).name() == typeid(uint8_t).name()) {
            return CreateNet<T>(netFile, modelFile, "int8", frugalMode, optimizeNet, tfaccId, threadNum);
        } else if (typeid(T).name() == typeid(float).name()) {
            return CreateNet<T>(netFile, modelFile, "float", frugalMode, optimizeNet, tfaccId, threadNum);
        } else {
            throw_exception("Create net failed: unsupport type.\n",new TFDLInitException());
            return nullptr;
        }
    }

    // 量化参数优化
    // netInt8: 定点Net（待优化）
    // netFloat: 浮点Net
    // inputs: 输入
    // (1) inputs.size() = 输入数量（建议不超过100张）
    // (2) inputs[i].size() = 网络的输入尺寸
    // target: 期望的余弦相似度
    // hybridNetFileName: 存盘的hybridNet网络文件
    // int8ModelFileName: 存盘的int8模型文件
    void OptimizeInt8Net(HybridNet *netInt8, Net<float> *netFloat, vector <vector <float> > &inputs,
                         double target, string hybridNetFileName, string int8ModelFileName,
                         map<string, vector<pair<float, float>>> *candidates = nullptr);

    void TFACCInit(const vector<tfdl::Net<magicType> *> &nets);
}


#endif //CAFFE_NET_H
