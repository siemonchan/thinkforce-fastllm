//
// Created by huangyuyang on 10/14/21.
//

#ifndef TFACC_CONVLAYER_H
#define TFACC_CONVLAYER_H

#include <vector>

#include "blasop.h"

namespace tfacc40t {
    enum InlineType {
        EltInline = 0,
        MaxPool2X2 = 1,
        AVEPool2X2 = 2,
        GlobalMax = 3,
        GlobalAve = 4,
        Upsample2 = 5
    };

    struct ConvolutionLayer {
        // 临时测试用
        int st;

        // io
        MEM_U8 *input;
        MEM_U8 *output;

        MEM_U8 *weight;
        MEM_U8 *bias;
        MEM_U32 *lutIndex;

        MEM_U32 *inputAddrs; // 当batch > 1时使用, 存储每个batch的起始地址

        // 当batch > 1时使用,存储每个batch使用的权重和偏置的起始地址
        // 默认为NULL, 那么每个batch都使用weight和bias作为起始地址
        MEM_U32 *weightAddrs;
        MEM_U32 *biasAddrs;

        //size param
        int batch;
        int inputChannels;
        int inputHeight;
        int inputWidth;
        int outputChannels;
        int kernelH;
        int kernelW;
        int padU, padD, padL, padR;
        int kernelStride;
        int group;
        int crossGroup = 1; // 交错的Group，比如crossGroup = 2时，通道分组为1 2 1 2 1 2 ... 这种模式
        int inputSpatial; // 输入spatial
        int outputSpatial; // 输出spatial
        int inputPerBatch; // 输入中每个batch的大小
        int outputPerBatch; // 输出中每个batch的大小
        int relu;

        //cale param
        int quantizedMultiplier;
        int rightShift, outputZero, weightZero, inputZero;

        //im2col时padding的zero，一般情况下就是inputZero，但也可能不是
        bool usePaddingZero = false;
        int paddingZero;

        //helper
        bool bankSwitch;
        bool preLoadMode;
        int split;
        int single;
        int biasCnt;
        int dsp;
        bool weightLock;
        bool winograd;
        bool depthwise;
        bool pointwise;
        bool pwise;
        bool autoFill;
        bool bothBatch;
        bool deconv;
        int maus;
        int crossSpatial;

        // Inline操作
        bool isInline;
        MEM_U8 *eltwiseInput; // Eltwise的另一个操作data的起始地址,形状必须和这一层的输出完全一致; or linebuf address.
        MEM_U8 *inlineoOutput; // InlineLayer out data的起始地址
        MEM_U8 *upsamplemem;
        int inlineStride;
        bool eltwiseRelu;
        enum InlineType inlineType;
        // TODO: Eltwise层参数

        // 辅助内存空间，超大卷积做int32累加时要用
        MEM_U8 *tempBuffer;
        // 辅助bias
        MEM_U8 *tempBias;
        // 强制设置single。拆分时使用，例如1024 * 3 * 3的卷积，拆分成两个512 * 3 * 3的卷积，但这时single还是应该用1024 * 3 * 3
        int forceSingle;

        // 强制使用split
        int forceSplit = -1;

        // widthStride,拆分时使用,如果为-1代表使用width则直接使用widthStride的值
        int widthStride;
        // 左右两边是否需要补zero
        bool needLSB, needRSB;

        bool outputInt32;
        bool activation;
        MEM_U8 *mapTable;

        ConvolutionLayer (); // 构造函数

        ~ConvolutionLayer(); // 析构函数

        void AddConvolutionBlasop(BlasopList *blasopList); // 将该层信息加入一个命令字队列

        // 将该层信息加入一个命令字队列列表
        // 如果这层很大，可能会拆分成多个命令字队列
        void AddConvolutionBlasop(std::vector <BlasopList*> &blasopLists);

        // 将该层拆分到多个 device 上，每个 device 对应一组 blasopList
        // 当前假设拆分后的层，不需要再拆到多个命令字队列
        // deviceCnt: 拆分到几个设备
        // direction: 拆分方向
        // 1 : 按通道拆
        // 2 : 按高度拆
        void AddConvolutionBlasopMultiDevice(std::vector <BlasopList*> &blasopLists,
                                             int deviceCnt,
                                             int direction);
    };
}

#endif //TFACC_CONVLAYER_H
