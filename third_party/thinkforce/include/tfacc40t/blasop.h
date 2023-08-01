//
// Created by huangyuyang on 10/14/21.
//

#ifndef TFACC_BLASOP_H
#define TFACC_BLASOP_H

#include <vector>
#include <cstring>

#include "memory.h"

namespace tfacc40t {
    const bool INTERLEAVE_ENABLE = false;

    struct BlasopList {
    private:
        int handle; // 执行命令字的句柄
    public:
        int chipId; // 芯片ID
        int cap; // 命令字队列的最大命令字条数
        int len; // 命令字队列当前的命令字条数
        int width; // 命令字队列单条命令的字节宽度
        MEM_U8 *data; // 命令字队列中存放命令字的地址

        BlasopList (int chipId, int cap, int width = 32); // 创建最大容纳cap条命令字的命令字队列
        ~BlasopList (); // 析构

        void Clear(); // 清空命令字队列
        TF_RET Run(int device); // 在指定的device上运行命令字
        TF_RET Launch(int device); // 在指定的device上下发命令字
        TF_RET IsFinish(bool *isFinish); // 查询该命令字是否完成
        TF_RET Wait(); // 等待下发的命令字执行完
        uint8_t *GetNewBlasopAddress(); // 加入一条空命令字，并返回该命令字的首地址
    };
    uint8_t *GetNewBlasopAddress(struct BlasopList *blasopList);
    void SplitBlasopListToVector(BlasopList *blasopList, std::vector <BlasopList*> &blasopLists);

    const int FLOAT_OP_SOFTMAX =       0x0001;
    const int FLOAT_OP_L2NORM =        0x0002;
    const int FLOAT_OP_SUM =           0x0004;
    const int FLOAT_OP_MAX_MIN =       0x0008;
    const int FLOAT_OP_DIV_COMMON =    0x0010;
    const int FLOAT_OP_SCALE =         0x0020;
    const int FLOAT_OP_INNER_PRODUCT = 0x0040;
    const int FLOAT_OP_ELTWISE_ADD =   0x0080;
    const int FLOAT_OP_ELTWISE_MUL =   0x0100;

    enum DataType {TFACC_INT8, TFACC_INT16, TFACC_INT32, TFACC_FLOAT, TFACC_FLOAT16, TFACC_BF16};
    enum EltwiseOpType {ELTWISE_COPY, ELTWISE_ADD, ELTWISE_SUB, ELTWISE_MUL};
    enum PoolingOpType {MAX_POOLING, AVG_POOLING, GLOBAL_AVG_POOLING, GLOBAL_MAX_POOLING};
    enum PoolingPaddingMethod {POOLING_NO_PADDING, POOLING_QUANTIZE_ZERO_PADDING, POOLING_ZERO_PADDING};
    enum PoolingAllToOneMode {POOLING_NORMAL, POOLING_HEIGHT_TO_ONE, POOLING_CHANNEL_TO_ONE, POOLING_WIDTH_TO_ONE};
    enum OutputMapType { MAP_NORMAL, MAP_RELU, MAP_LUT };
    enum QuantizeType { QUANT_BIT_8, QUANT_BIT_10, NO_QUANT };
    enum DeconvType {Deconv,Bilnear,Near};
    enum ResizeMode {
        Binear = 0,
        Nearest = 1
    };
    struct DataQuantInfo{
        uint8_t zero_point;
        uint8_t shift;
        int32_t multiplier;
    };

    struct Blasop;
    struct DMABlasop;
    struct TFACCRegisterBlasop;
    struct InvalidCacheBlasop;
    struct PreloadCacheBlasop;
    struct LockCacheBlasop;
    struct CFGBlasop;
    struct ConvBlasop;
    struct PoolingBlasop;
    struct EltwiseBlasop;
    struct ImgCropBlasop;
    struct ImgResizeBlasop;
    struct ScaleBlasop;
    struct SoftmaxBlasop;
    struct MinMaxBlasop;
    struct L2NormalizeBlasop;
    struct DivCommonBlasop;
    struct FltEltwiseBlasop;
    struct FltInnerProBlasop;
    struct FltSumBlasop;
    struct GemmBlasop;
    struct SyncBlasop;

    void AddDMACopyBlasop(struct BlasopList *blasopList, uint64_t targetAddress, uint64_t sourceAddress, int length);
    void AddDMABroadCastBlasop(struct BlasopList *blasopList, uint64_t targetAddress, int len, int fillPattern);
    void AddBreakSyncBlasop(struct BlasopList *blasopList); // 加入一条可分裂的sync命令，代表命令字队列可以从这个点拆开
    void AddSyncBlasop(struct BlasopList *blasopList);
    void AddSyncBlasopSkipTailWrite(struct BlasopList *blasopList);
    void AddTFACCSyncBlasop(struct BlasopList *blasopList);
    void AddCFGSyncBlasop(struct BlasopList *blasopList);
    void AddPreloadSyncBlasop(struct BlasopList *blasopList);
    void AddClearSyncBlasop(struct BlasopList *blasopList);
    void AddImgSyncBlasop(struct BlasopList *blasopList);
    void AddImgCropSyncBlasop(struct BlasopList *blasopList);
    void Addyuv2rgbSyncBlasop(struct BlasopList *blasopList);
    void AddInvalidCacheSyncBlasop(struct BlasopList *blasopList);
    void AddTFACCRegisterBlasop(struct BlasopList *blasopList, uint32_t index, uint32_t value);
    void AddTFACCRegisterBlasop2(struct BlasopList *blasopList, int cnt,uint32_t* datas);
    void AddInvalidCacheBlasop(struct BlasopList *blasopList, uint32_t type);
    void AddPreloadCacheBlasop(struct BlasopList *blasopList, int port);
    void AddPreloadCacheBlasopWithoutWritetail(struct BlasopList *blasopList, int port);
    void AddLockCacheBlasop(struct BlasopList *blasopList, uint32_t type);
    void AddCFGBlasop(struct BlasopList *blasopList, uint32_t addr, uint32_t value);
    void AddCFGBlasop2(struct BlasopList *blasopList, uint32_t addr0, uint32_t value0,
                       uint32_t addr1, uint32_t value1);
    void AddCFGBlasop3(struct BlasopList *blasopList, uint32_t addr0, uint32_t value0,
                       uint32_t addr1, uint32_t value1,
                       uint32_t addr2, uint32_t value2);

    void AddConvBlasop(struct BlasopList *blasopList);
    void AddPoolingBlasop(struct BlasopList *blasopList,
                          uint64_t inputAddress, uint64_t outputAddress, enum PoolingOpType opType,
                          enum PoolingAllToOneMode allToOneMode,
                          int channels, int inputHeight, int inputWidth, int inputSpatial, int outputSpatial,
                          int kernelSize, int stride, uint8_t inputZero,
                          int isROI, int avgBypassDiv, enum PoolingPaddingMethod paddingMethod);
    void AddSimplePoolingCFGBlasop(struct BlasopList *blasopList,
                                   int inputHeight, int inputWidth, int kernelSize, int stride);
    void AddBypassEltwiseBlasop(struct BlasopList *blasopList);
    void AddBypassEltwiseBlasopWithAddress(struct BlasopList *blasopList, uint32_t dstAddress);
    void AddScBypassEltwiseBlasop(struct BlasopList *blasopList, uint32_t dstAddress);
    void AddRqtzBypassEltwiseBlasop(struct BlasopList *blasopList, uint32_t dstAddress, int len);
    void AddEltwiseBlasop(struct BlasopList *blasopList,
                          int input0FromMau, int opType, uint64_t input0Address,
                          uint64_t input1Address,
                          uint64_t outputAddress,
                          int len, enum DataType dataType, enum EltwiseOpType eltwiseFunc, int requantize,
                          int tailWriteSkip);
    void AddEltwiseOp_eltu8(struct BlasopList *blasopList,
                            int input0FromMau, int opType, uint64_t input0Address, struct DataQuantInfo quantInfo0,
                            uint64_t input1Address, struct DataQuantInfo quantInfo1,
                            uint64_t outputAddress, struct DataQuantInfo quantInfo_result,
                            int len, enum EltwiseOpType eltwiseFunc,
                            int left_shift, enum OutputMapType output_map_type);
    void AddRequantizeOp(struct BlasopList *blasopList,
                         int input0FromMau, int opType, uint64_t input0Address,
                         uint64_t outputAddress, struct DataQuantInfo quantInfo_result,
                         int len, enum OutputMapType output_map_type);
    void AddEltwiseOp_eltu32(struct BlasopList *blasopList,
                             int input0FromMau, int opType, uint64_t input0Address, struct DataQuantInfo quantInfo0,
                             uint64_t input1Address, struct DataQuantInfo quantInfo1,
                             uint64_t outputAddress, struct DataQuantInfo quantInfo_result,
                             int len, enum EltwiseOpType eltwiseFunc,
                             int requantize, int left_shift, enum OutputMapType output_map_type);
    void AddEltwiseOp_i32(struct BlasopList *blasopList,
                          int input0FromMau, int opType, uint64_t input0Address,
                          uint64_t input1Address,
                          uint64_t outputAddress,
                          int len, enum EltwiseOpType eltwiseFunc);
    void DumpEltADDQinfo(struct DataQuantInfo* a, struct DataQuantInfo* b, struct DataQuantInfo* c,double scaleA,double scaleB,double scaleC,uint8_t zeroA,uint8_t zeroB,uint8_t zeroC);
    void DumpEltMULQinfo(struct DataQuantInfo* a, struct DataQuantInfo* b, struct DataQuantInfo* c, double scaleA,double scaleB,double scaleC,uint8_t zeroA,uint8_t zeroB,uint8_t zeroC);

    uint32_t ImgCropSize(int width, int height);
    uint32_t ImgCropStride(int srcStride, int dstStride);
    uint16_t ImgCropPaddingMethod(int method, int value);
    void AddImgCropBlasop(struct BlasopList *blasopList, uint64_t inputAddress, uint64_t outputAddress,
                          int inputHeight, int inputWidth, int outputHeight, int outputWidth, int top, int left);

    uint32_t ImgResizeSize(int stride, int height);
    void AddImgResizeBlasop(struct BlasopList *blasopList, uint64_t inputAddress, uint64_t outputAddress,
                            int inputWidth, int outputWidth, int resizeMode, int resizeType);
    void AddOpResize(struct BlasopList *blasopList,uint64_t input, uint64_t output, int ishape[4], int oshape[4],
                     int ispatial, int ospatial, enum ResizeMode mode, bool pad, int padvalue, int initx, int inity,
                     int dx, int dy);
    void AddYUV2RGBBlasop(struct BlasopList *blasopList, uint64_t targetAddress, uint64_t sourceAddress, int imgstride,uint16_t imgw,uint16_t imgh,int mode);
    void AddYUV2RGBBlasop(struct BlasopList *blasopList, uint64_t targetAddress, uint64_t sourceY, uint64_t sourceU, uint64_t sourceV, int imgstride,uint16_t imgw,uint16_t imgh,int mode);
    void AddScaleBlasop(struct BlasopList *blasopList, uint64_t inputAddress, uint64_t outputAddress,
                        uint32_t lutIndex, uint32_t paramIndex, int channel, int len);
    void AddSoftmaxBlasop(struct BlasopList *blasopList, uint64_t inputAddress, uint64_t outputAddress,
                          uint32_t lutIndex, uint32_t paramIndex, int channel, int len, int rnd);
    void AddL2NormBlasop(struct BlasopList *blasopList, uint64_t inputAddress, uint64_t outputAddress,
                         uint32_t lutIndex, uint32_t paramIndex, int channel, int len);
    void AddMinMaxBlasop(struct BlasopList *blasopList, uint64_t inputAddress, uint64_t outputAddress, int op,
                         uint32_t lutIndex, uint32_t paramIndex, int channel, int len);
    void AddDivCommonBlasop(struct BlasopList *blasopList, uint64_t inputAddress, uint64_t outputAddress,
                            uint32_t lutIndex, uint32_t paramIndex, int channel, int len);
    void AddFltEltwiseBlasop(struct BlasopList *blasopList, uint64_t inputAddress, uint64_t outputAddress,
                             uint32_t lutIndex, uint32_t paramIndex, int channel, int len, enum EltwiseOpType opType,
                             enum DataType dataType);
    void AddFltInnerProBlasop(struct BlasopList *blasopList, uint64_t inputAddress, uint64_t outputAddress,
                              uint32_t lutIndex, uint32_t paramIndex, int channel, enum DataType dataType, int int8Mode);
    void AddFltSumBlasop(struct BlasopList *blasopList, uint64_t inputAddress, uint64_t outputAddress,
                         uint32_t lutIndex, uint32_t paramIndex, int channel, int len);
    void AddGemmBlasop(struct BlasopList *blasopList, uint64_t inputAddress, uint64_t outputAddress,
                       uint32_t weightIndex, int fcMode, int relu, int ac, int len, int reduction);

    void AddOpflowBlasOpInt32(struct BlasopList *blasopList, uint8_t *targetAddress, uint8_t *sourceAddress,
                              uint32_t *paramAddress, int imgw, int imgh, int imgchannel, int src_stride,
                              int dst_stride, int src_step, int dst_step);

    void AddOpflowBlasOpInt8(struct BlasopList *blasopList, uint8_t *targetAddress, uint8_t *sourceAddress,
                             uint32_t *paramAddress, int imgw, int imgh, int imgchannel, int src_stride, int dst_stride,
                             int src_step, int dst_step, float opscale, uint8_t opzp);

    void AddOpPerspectiveWarp(struct BlasopList *blasopList, uint8_t *targetAddress, uint8_t *sourceAddress,
                              uint32_t *paramAddress, int imgw, int imgh, int imgchannel, int src_stride,
                              int dst_stride, int src_step, int dst_step, uint8_t padvalue);

    void AddOpAffineWarp(struct BlasopList *blasopList, uint8_t *targetAddress, uint8_t *sourceAddress,
                         uint32_t *paramAddress, int imgw, int imgh, int imgchannel, int src_stride, int dst_stride,
                         int src_step, int dst_step, uint8_t padvalue);

    void AnalyseCmdsFromFile(const char *fileName);

    struct Blasop {
        uint16_t blasop_op;
        union{
            uint16_t blasop_ctrl;
            struct {
                uint16_t fill_zero:       1; //0
                uint16_t winograd_mode:   1; //1
                uint16_t save_discard:    1; //2
                uint16_t restore_discard: 1; //3
                uint16_t unused_2:          1; //4
                uint16_t enable_rle:      1; //5
                uint16_t rqtz_bypass:     1; //6
                uint16_t topk_allcmdlast:        1; //7
                uint16_t lut_en:          1; //8
                uint16_t unused_3:        1; //9
                uint16_t depthwise:       1; //10
                uint16_t pointwise:       1; //11
                uint16_t group_conv_mode: 1; //12
                uint16_t fc_mode:         1; //13
                uint16_t im2col_mode:     1; //14
                uint16_t two_cmd_mode:    1; //15
            };
        };
        struct {
            uint32_t blasop_pic_step_size : 24; //Must be 32Byte aligned
            uint32_t blasop_B_zeropoint   :  8;
        } blasop_pic_info0;
        uint16_t blasop_dimension_m;
        uint16_t blasop_dimension_k; //Always set 1 for now
        uint16_t blasop_dimension_n : 3; //for 128DSP, set to 1
        uint16_t blasop_dimension_m_trim : 13; //Actual kernel dim
        uint16_t blasop_pic_kernel_config : 2; //set to 0 for stride 1
        uint16_t blasop_pic_kernel_size : 3; // up to 7x7 kernel
        uint16_t blasop_pic_feature : 11; // maximum 2048 feature (not fully supported by hardware for now unless the pic is very small)
        uint32_t blasop_result_base; //Now result is not writing back to result queue, but to the address specified here
        uint16_t blasop_matrixA_index;
        uint16_t blasop_matrixC_index;
        uint32_t blasop_matrixB_base;
        uint16_t blasop_pic_width;
        uint16_t unused;

        Blasop () {
            memset((char*)this, 0, sizeof(*this));
        }
    };

    struct DMABlasop {
        uint32_t blasopOp; // 0x60
        uint32_t gap0; // 0
        uint32_t srcBase; // 源地址 >> 2，DW 对齐
        uint32_t dstBase; // 目标地址 >> 2
        uint32_t length : 27; // byte length
        uint32_t gap1 : 5; // 0
        uint32_t fillPattern; // 如果dmaType是1,那么将目标地址都填成fillPattern
        uint32_t dmaType : 2; // 0: dma搬数, 1: dma填数
        uint32_t gap2 : 2; // 0
        uint32_t dmaChannel : 2; // 使用的dma通道, 可选0 1 2 3
        uint32_t gap3 : 26; // 0
        uint32_t gap4 : 16; // 0
        uint32_t algorithm : 3; // 7
        uint32_t tailWriteSkip : 1; // 命令完成后是否跳过写命令池tail, 为1则不写tail
        uint32_t imgFunc : 4; // 4
        uint32_t gap5 : 8; // 0

        DMABlasop () {
            memset((char*)this, 0, sizeof(*this));
        }
    };

    struct TFACCRegisterBlasop {
        uint32_t header0 : 4; // 2
        uint32_t header1 : 4; // 2
        uint32_t gap0 : 4; // 0
        uint32_t header2 : 4; // 3
        uint32_t gap1 : 8; // 0;
        uint32_t regMask : 3; // 每一位代表1个寄存器通道是否启用
        uint32_t gap2 : 5; // 0
        uint32_t gap3; // 0
        uint32_t addr0;
        uint32_t value0;
        uint32_t addr1;
        uint32_t value1;
        uint32_t addr2;
        uint32_t value2;

        TFACCRegisterBlasop () {
            memset((char*)this, 0, sizeof(*this));
        }
    };

    struct InvalidCacheBlasop {
        uint32_t header0 : 4; // 0
        uint32_t header1 : 4; // 2
        uint32_t gap0 : 20; // 0
        uint32_t header2 : 4; // 1
        uint32_t gap1; // 0
        uint32_t gap2; // 0
        uint32_t type : 4; // 0-> cleaninvalid_address, 1-> makeinvalid_address, 2-> cleanshared_address, 6-> cleaninvalid_all, 7-> makeinvalid_all, 8->cleanshared_all
        uint32_t gap3 : 28; // 0
        uint32_t gap4; // 0
        uint32_t gap5; // 0
        uint32_t gap6; // 0
        uint32_t gap7; // 0

        InvalidCacheBlasop () {
            memset((char*)this, 0, sizeof(*this));
        }
    };

    struct PreloadCacheBlasop {
        uint32_t header0 : 4; // 0
        uint32_t header1 : 4; // 2
        uint32_t gap0 : 20; // 0
        uint32_t header2 : 4; // 3
        uint32_t gap1; // 0
        uint32_t gap2; // 0
        uint32_t port : 4; // port, one-hot
        uint32_t gap3 : 28; // 0
        uint32_t gap4; // 0
        uint32_t gap5; // 0
        uint32_t gap6; // 0
        uint32_t gap7; // 0

        PreloadCacheBlasop () {
            memset((char*)this, 0, sizeof(*this));
        }
    };

    struct LockCacheBlasop {
        uint32_t header0 : 4; // 0
        uint32_t header1 : 4; // 2
        uint32_t gap0 : 20; // 0
        uint32_t header2 : 4; // 2
        uint32_t gap1; // 0
        uint32_t gap2; // 0
        uint32_t type : 2; // 1-> unlock_addr, 2-> lock_addr, 3-> unlock_all
        uint32_t gap3 : 30; // 0
        uint32_t gap4; // 0
        uint32_t gap5; // 0
        uint32_t gap6; // 0
        uint32_t gap7; // 0

        LockCacheBlasop () {
            memset((char*)this, 0, sizeof(*this));
        }
    };

    struct CFGBlasop {
        uint32_t blasopOp : 16; // 0x3002
        uint32_t interrupt : 1; // 0
        uint32_t gap0 : 8; // 0
        uint32_t slotReg : 2;
        uint32_t tailWrite : 1; // 命令完成后是否更新命令池tail
        uint32_t gap1 : 4; // 0
        uint32_t gap2; // 0
        uint32_t addr0;
        uint32_t value0;
        uint32_t addr1;
        uint32_t value1;
        uint32_t addr2;
        uint32_t value2;

        CFGBlasop () {
            memset((char*)this, 0, sizeof(*this));
        }
    };

    struct ConvolutionBlasop {
        uint16_t blasop_op;
        union{
            uint16_t blasop_ctrl;
            struct {
                uint16_t fill_zero:       1; //0
                uint16_t winograd_mode:   1; //1
                uint16_t save_discard:    1; //2
                uint16_t restore_discard: 1; //3
                uint16_t unused_2:          1; //4
                uint16_t enable_rle:      1; //5
                uint16_t rqtz_bypass:     1; //6
                uint16_t unused_1:        1; //7
                uint16_t lut_en:          1; //8
                uint16_t unused_3:        1; //9
                uint16_t depthwise:       1; //10
                uint16_t pointwise:       1; //11
                uint16_t group_conv_mode: 1; //12
                uint16_t fc_mode:         1; //13
                uint16_t im2col_mode:     1; //14
                uint16_t two_cmd_mode:    1; //15
            };
        };
        struct {
            uint32_t blasop_pic_step_size : 24; //Must be 32Byte aligned
            uint32_t blasop_B_zeropoint   :  8;
        } blasop_pic_info0;
        uint16_t blasop_dimension_m;
        uint16_t blasop_dimension_k; //Always set 1 for now
        uint16_t blasop_dimension_n : 3; //for 128DSP, set to 1
        uint16_t blasop_dimension_m_trim : 13; //Actual kernel dim
        uint16_t blasop_pic_kernel_config : 2; //set to 0 for stride 1
        uint16_t blasop_pic_kernel_size : 3; // up to 7x7 kernel
        uint16_t blasop_pic_feature : 11; // maximum 2048 feature (not fully supported by hardware for now unless the pic is very small)
        uint32_t blasop_result_base; //Now result is not writing back to result queue, but to the address specified here
        uint16_t blasop_matrixA_index;
        uint16_t blasop_matrixC_index;
        uint32_t blasop_matrixB_base;
        uint16_t blasop_pic_width;
        uint16_t unused;

        ConvolutionBlasop () {
            memset((char*)this, 0, sizeof(*this));
        }
    };

    struct PoolingBlasop {
        uint32_t blasopOp; // 0x60
        uint32_t gap0; // 0
        uint32_t srcBase; // 输入数据地址 >> 2
        uint32_t srcStep; // 输入数据每通道字节数 >> 2
        uint32_t dstBase; // 输出数据地址 >> 2
        uint32_t dstStep; // 输出数据每通道字节数 >> 2
        uint32_t channels : 11; // 输入通道数 - 1
        uint32_t gap1 : 1; // 0
        uint32_t isROI : 1; // 是否是ROI Pooling
        uint32_t allToOneMode : 2; // all to one 模式
        uint32_t avgBypassDiv : 1; // 输出 avg 除法前的 int32
        uint32_t width : 12; // 输入数据的宽
        uint32_t heightLow : 4; // 输入数据的高 & 15
        uint32_t height : 8; // 输入数据的高 >> 4
        uint32_t poolingSize : 3; // Pooling核的尺寸
        uint32_t gap2 : 1; // 0
        uint32_t poolingStride : 3; // Pooling的步长
        uint32_t gap3 : 1; // 0
        uint32_t poolingAlgorithm : 3; // Pooling的算法
        uint32_t tailWriteSkip : 1; // 命令完成后是否更新命令池tail
        uint32_t gap4 : 2; // 0
        uint32_t resultPaddingMethod : 2; // result填充算法
        uint32_t zeroPoint : 8; // 输入数据量化值的0点 (zeroPoint)

        PoolingBlasop () {
            memset((char*)this, 0, sizeof(*this));
        }
    };

    struct EltwiseBlasop {
        uint32_t blasopOp;
        uint32_t gap0; //zero
        uint32_t src0Base;
        uint32_t src1Base;
        uint32_t dstBase;
        uint32_t len : 27;
        uint32_t gap1 : 5;
        uint32_t opType : 4;
        uint32_t calcFunc : 16;
        uint32_t outputfunc : 1;
        uint32_t op0Source : 2;
        uint32_t rqtzParamSel : 1;
        uint32_t gap2 : 8;
        uint32_t gap3 : 10;
        uint32_t cmdPartialWriteAsFull : 1;
        uint32_t gap4 : 5;
        uint32_t decodePoolingAlgorithm: 3;
        uint32_t tailWriteSkip : 1; // 命令完成后是否更新命令池tail
        uint32_t subFunc: 4;
        uint32_t gap5 : 8;

        EltwiseBlasop () {
            memset((char*)this, 0, sizeof(*this));
        }
    };
    struct YUV2RGBBlasOp{
        uint32_t blasopOp; // 0x60
        uint32_t gap0; // 0
        uint32_t y_srcddr; // y channel src addr
        uint32_t u_srcddr; // u channel src addr
        uint32_t v_srcddr; // v channel src addr
        uint32_t dst_ddr; // dst addr
        uint32_t mode : 1;// 0: TO RGB; 1 : TO YUV444
        uint32_t gap1 : 31;
        uint32_t gap2 : 16;
        uint32_t decodeAlgorithm : 3; // default 7
        uint32_t skipWriteTail : 1; //mean 0: write tail to mem; 1 : do not write tail to mem
        uint32_t decodeFunction : 4; // default 3
        uint32_t gap3 : 8;// 0

        YUV2RGBBlasOp () {
            memset((char*)this, 0, sizeof(*this));
        }
    };

    struct ImgTransformBlasOp{
        uint32_t blasopOp; // 0x60
        uint32_t gap0; // 0
        uint32_t img_ddr; // img addr
        uint32_t dst_ddr; // dst addr
        uint32_t param_ddr; // opflow or hmat addr
        uint32_t src_w : 16;
        uint32_t src_h : 16;
        uint32_t dst_w : 16;
        uint32_t dst_h : 16;
        uint32_t gap1 : 16;// 0
        uint32_t decodeAlgorithm : 3; // default 7
        uint32_t skipWriteTail : 1; //mean 0: write tail to mem; 1 : do not write tail to mem
        uint32_t decodeFunction : 4; // default 3
        uint32_t gap2 : 8;// 0

        ImgTransformBlasOp () {
            memset((char*)this, 0, sizeof(*this));
        }
    };


    struct ImgCropBlasop {
        uint32_t blasopOp; // 0x60
        uint32_t gap0; // 0
        uint32_t srcBase;
        uint32_t srcStep;
        uint32_t dstBase;
        uint32_t dstStep;
        uint16_t topLeftX;
        uint16_t topLeftY;
        uint16_t gap1;
        uint32_t poolingAlgorithm : 3; // Pooling的算法
        uint32_t tailWriteSkip : 1; // 命令完成后是否更新命令池tail
        uint32_t decodeImgFunction : 4;
        uint16_t gap2 : 8;

        ImgCropBlasop () {
            memset((char*)this, 0, sizeof(*this));
        }
    };

    struct ImgResizeBlasop {
        uint32_t blasopOp; // 0x60
        uint32_t gap0; // 0
        uint32_t srcBase;
        uint32_t dstBase;
        uint16_t srcWidth;
        uint16_t dstWidth;
        uint32_t resizeMode : 1; // 0 for bilinear, 1 for nearest
        uint32_t resizeType : 1; // 0 for int8, 1 for int32
        uint32_t gap1 : 30;
        uint32_t gap2;
        uint16_t gap3;
        uint32_t poolingAlgorithm : 3; // Pooling的算法
        uint32_t tailWriteSkip : 1; // 命令完成后是否更新命令池tail
        uint32_t decodeImgFunction : 4;
        uint16_t gap4 : 8;

        ImgResizeBlasop () {
            memset((char*)this, 0, sizeof(*this));
        }
    };

    struct ScaleBlasop {
        uint16_t blasop; // 0x4002
        uint16_t op; // 0b100000;
        uint16_t widx; // lut
        uint16_t pidx; // a, b
        uint32_t srcBase;
        uint32_t dstBase;
        uint16_t cfg; // 0b0000000010000000
        uint16_t srcStep;
        uint16_t maxFeature;
        uint16_t gap1; // 0
        uint32_t scaleFromCmd;
        uint32_t gap2; // 0

        ScaleBlasop () {
            memset((char*)this, 0, sizeof(*this));
        }
    };

    struct SoftmaxBlasop {
        uint16_t blasop; // 0x4002
        uint16_t op; // 1
        uint16_t widx; // lut
        uint16_t pidx; // scale
        uint32_t srcBase;
        uint32_t dstBase;
        uint16_t cfg; // 0b0000001000001100
        uint16_t srcStep;
        uint16_t maxFeature;
        uint16_t gap1; // 0
        uint32_t scaleFromCmd; //32'h3f80_0000
        uint32_t gap2; // 0

        SoftmaxBlasop () {
            memset((char*)this, 0, sizeof(*this));
        }
    };

    struct MinMaxBlasop {
        uint16_t blasop;
        uint16_t op; // 0b10;
        uint16_t widx; // lut
        uint16_t pidx; // 0
        uint32_t srcBase;
        uint32_t dstBase;
        uint16_t cfg; // 0b0000000010000100
        uint16_t srcStep;
        uint16_t maxFeature;
        uint16_t gap1; // 0
        uint32_t scaleFromCmd; //0
        uint32_t gap2; // 0

        MinMaxBlasop () {
            memset((char*)this, 0, sizeof(*this));
        }
    };

    struct L2NormalizeBlasop {
        uint16_t blasop;
        uint16_t op; // 0b10;
        uint16_t widx; // lut
        uint16_t pidx; // 0
        uint32_t srcBase;
        uint32_t dstBase;
        uint16_t cfg; // 0b0000000010000100
        uint16_t srcStep;
        uint16_t maxFeature;
        uint16_t gap1; // 0
        uint32_t scaleFromCmd; //0
        uint32_t gap2; // 0

        L2NormalizeBlasop () {
            memset((char*)this, 0, sizeof(*this));
        }
    };

    struct DivCommonBlasop {
        uint16_t blasop;
        uint16_t op; // 0b10000;
        uint16_t widx; // lut
        uint16_t pidx; // 0
        uint32_t srcBase;
        uint32_t dstBase;
        uint16_t cfg; // 0
        uint16_t srcStep;
        uint16_t maxFeature;
        uint16_t gap1; // 0
        uint32_t scaleFromCmd; //0
        uint32_t gap2; // 0

        DivCommonBlasop () {
            memset((char*)this, 0, sizeof(*this));
        }
    };

    struct FltEltwiseBlasop {
        uint16_t blasop;
        uint16_t op;
        uint16_t widx; // weight
        uint16_t pidx; // 0
        uint32_t srcBase;
        uint32_t dstBase;
        uint16_t cfg : 14;
        uint16_t bf16Mode : 1;
        uint16_t float16Mode : 1;
        uint16_t srcStep;
        uint16_t maxFeature;
        uint16_t gap1; // 0
        uint32_t scaleFromCmd; // 0 when mul, 0x3f800000 when add
        uint32_t gap2; // 0

        FltEltwiseBlasop () {
            memset((char*)this, 0, sizeof(*this));
        }
    };

    struct FltInnerProBlasop {
        uint16_t blasop;
        uint16_t op;
        uint16_t widx; // weight
        uint16_t pidx; // 0
        uint32_t srcBase;
        uint32_t dstBase;
        uint16_t cfg : 14;
        uint16_t bf16Mode : 1;
        uint16_t float16Mode : 1;
        uint16_t srcStep; // 256
        uint16_t maxFeature;
        uint16_t gap1; // 0
        uint32_t scaleFromCmd; //0
        uint32_t gap2; // 0

        FltInnerProBlasop () {
            memset((char*)this, 0, sizeof(*this));
        }
    };

    struct FltSumBlasop {
        uint16_t blasop;
        uint16_t op;
        uint16_t widx; // 0
        uint16_t pidx; // 0
        uint32_t srcBase;
        uint32_t dstBase;
        uint16_t cfg;
        uint16_t srcStep;
        uint16_t maxFeature;
        uint16_t gap1; // 0
        uint32_t scaleFromCmd; //0
        uint32_t gap2; // 0

        FltSumBlasop () {
            memset((char*)this, 0, sizeof(*this));
        }
    };

    struct PermuteBlasop {
        uint32_t blasop:8;//0x20
        uint32_t gap0:12;//0
        uint32_t src_batch:4;//0-base
        uint32_t pading_en:1;
        uint32_t gap1:3;//0
        uint32_t op:4;//0x4;
        uint32_t gap2;//0
        uint32_t src_addr;
        uint32_t src_spatial;
        uint32_t src_channel:16;
        uint32_t channel_zp:8;
        uint32_t spatial_zp:8;
        uint32_t dst_addr;
        uint32_t src_stride;
        uint32_t dst_stride;

        PermuteBlasop () {
            memset((char*)this, 0, sizeof(*this));
        }
    };
    struct ConcatBlasop0 {
        uint32_t blasop;//0x50000020
        uint32_t gap0;//0
        uint32_t operand_num:2;
        uint32_t axis:2;
        uint32_t gap1:2;//0
        uint32_t result_zp:8;//0
        uint32_t src_addr;
        uint32_t src0_axis_dim:16;
        uint32_t src1_axis_dim:16;
        uint32_t src2_axis_dim:16;
        uint32_t src3_axis_dim:16;
        uint32_t src0_stride;
        uint32_t src1_stride;

        ConcatBlasop0 () {
            memset((char*)this, 0, sizeof(*this));
        }
    };
    struct ConcatBlasop1 {
        uint32_t blasop;//0x50000020
        uint32_t gap0;//0
        uint32_t src2_stride;
        uint32_t src3_stride;
        uint32_t dst_addr;
        uint32_t dst_stride;
        uint32_t comon_width:16;
        uint32_t comon_height:16;
        uint32_t comon_channel:16;
        uint32_t comon_batch:16;

        ConcatBlasop1 () {
            memset((char*)this, 0, sizeof(*this));
        }
    };
    struct GemmBlasop {
        uint16_t blasop;
        uint16_t op;
        uint16_t widx;
        uint8_t tapselFc1;
        uint8_t tapselFc2;
        uint32_t srcBase;
        uint32_t dstBase;
        uint32_t fc1Cfg;
        uint32_t fc2Cfg;
        uint16_t weightLen;
        uint16_t globalPoolCfg;
        uint32_t totalDWLen;

        GemmBlasop () {
            memset((char*)this, 0, sizeof(*this));
        }
    };
    struct InlinexBlasop{
        uint32_t blasopOp;//0x60
        uint32_t gap0; //zero
        uint32_t internaddress;
        uint32_t internstride;
        uint32_t dstaddress;
        uint32_t dststride;
        uint64_t alltail;
        //uint32_t gap1 : 2;
        //uint32_t picwidth : 12;
        //uint32_t picheight : 12;
        //uint32_t gap2 : 2;
        /*uint32_t mausize : 2;
        uint32_t result_padmethod : 2;
        uint32_t decodePoolingAlgorithm : 3;
        uint32_t tailWriteSkip : 1; // 命令完成后是否更新命令池tail
        uint32_t subFunc : 4;
        uint32_t zeropoint : 8;*/

        InlinexBlasop () {
            memset((char*)this, 0, sizeof(*this));
        }
    };

    struct ROIMAXPOOLBlasop{
        uint32_t blasopOp;//0x60
        uint32_t gap0; //zero
        uint32_t srcAddr;
        uint32_t srcStep;
        uint32_t dstAddr;
        uint32_t dstStep;
        uint64_t alltail;

        ROIMAXPOOLBlasop () {
            memset((char*)this, 0, sizeof(*this));
        }
    };

    struct ROIPARAMBlasop{
        uint32_t blasopOp;//0x60
        uint32_t gap0; //zero
        uint32_t srcAddr;
        uint32_t gap1;
        uint32_t gap2;
        uint32_t gap3;
        uint32_t gap4;
        uint32_t deocoder_algo;

        ROIPARAMBlasop () {
            memset((char*)this, 0, sizeof(*this));
        }
    };

    struct SyncBlasop {
        int cmdHeader; // 0x3004
        int funcMask; // 每一位描述一种function是否要sync
        int unused[6];

        SyncBlasop () {
            memset((char*)this, 0, sizeof(*this));
        }
    };
}

#endif //TFACC_BLASOP_H
