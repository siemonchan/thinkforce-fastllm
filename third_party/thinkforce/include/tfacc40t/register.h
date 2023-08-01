//
// Created by huangyuyang on 6/12/20.
//

#ifndef PROJECT_REGISTER_H
#define PROJECT_REGISTER_H

#include "stdint.h"

#define TFACC40T
#ifdef TFACC40T
#define CFG_SUBMAU_ON_INIT 0x263
#define CREDIT 0x271
#define CFG_MAU_B_ON_INIT 0x261
#define CFG_MAU_A_ON_INIT 0x262
#define CFG_MAU_ON 0x260
#define CFG_W_MAU_ON 0x264
#define CFG_W_VLD_ON 0x265
#define CFG_SM_CHANNEL_OFFSET 0x291
#define CFG_DUMMY_DLY0 0xf0
#define CFG_DUMMY_DLY1 0xf1

#define REG_VERSION_BASE 0x0
// queue0
#define REG_BLASOP_QUEUE_DEPTH_BASE 0x13
#define REG_RESULT_QUEUE_DEPTH_BASE 0x17
#define REG_BLASOP_QUEUE_BASE_LOW 0x10
#define REG_BLASOP_QUEUE_BASE_HIGH 0x11
#define REG_RESULT_QUEUE_BASE_LOW 0x14
#define REG_RESULT_QUEUE_BASE_HIGH 0x15
#define REG_RESULT_QUEUE_TAIL_BASE_LOW 0x18
#define REG_RESULT_QUEUE_TAIL_BASE_HIGH 0x19
#define REG_BLASOP_QUEUE_HEAD_BASE 0x13
#define REG_BLASOP_QUEUE_TAIL_BASE 0x12
#define REG_RESULT_QUEUE_HEAD_BASE 0x16
//queue1
#define REG_BLASOP_QUEUE1_DEPTH_BASE 0x33
#define REG_RESULT_QUEUE1_DEPTH_BASE 0x37
#define REG_BLASOP_QUEUE1_BASE_LOW 0x30
#define REG_BLASOP_QUEUE1_BASE_HIGH 0x31
#define REG_RESULT_QUEUE1_BASE_LOW 0x34
#define REG_RESULT_QUEUE1_BASE_HIGH 0x35
#define REG_RESULT_QUEUE1_TAIL_BASE_LOW 0x38
#define REG_RESULT_QUEUE1_TAIL_BASE_HIGH 0x39
#define REG_BLASOP_QUEUE1_HEAD_BASE 0x33
#define REG_BLASOP_QUEUE1_TAIL_BASE 0x32
#define REG_RESULT_QUEUE1_HEAD_BASE 0x36
//queue2
#define REG_BLASOP_QUEUE2_DEPTH_BASE 0x43
#define REG_RESULT_QUEUE2_DEPTH_BASE 0x47
#define REG_BLASOP_QUEUE2_BASE_LOW 0x40
#define REG_BLASOP_QUEUE2_BASE_HIGH 0x41
#define REG_RESULT_QUEUE2_BASE_LOW 0x44
#define REG_RESULT_QUEUE2_BASE_HIGH 0x45
#define REG_RESULT_QUEUE2_TAIL_BASE_LOW 0x48
#define REG_RESULT_QUEUE2_TAIL_BASE_HIGH 0x49
#define REG_BLASOP_QUEUE2_HEAD_BASE 0x43
#define REG_BLASOP_QUEUE2_TAIL_BASE 0x42
#define REG_RESULT_QUEUE2_HEAD_BASE 0x46
//queue3
#define REG_BLASOP_QUEUE3_DEPTH_BASE 0x53
#define REG_RESULT_QUEUE3_DEPTH_BASE 0x57
#define REG_BLASOP_QUEUE3_BASE_LOW 0x50
#define REG_BLASOP_QUEUE3_BASE_HIGH 0x51
#define REG_RESULT_QUEUE3_BASE_LOW 0x54
#define REG_RESULT_QUEUE3_BASE_HIGH 0x55
#define REG_RESULT_QUEUE3_TAIL_BASE_LOW 0x58
#define REG_RESULT_QUEUE3_TAIL_BASE_HIGH 0x59
#define REG_BLASOP_QUEUE3_HEAD_BASE 0x53
#define REG_BLASOP_QUEUE3_TAIL_BASE 0x52
#define REG_RESULT_QUEUE3_HEAD_BASE 0x56

#define REG_POOLING_TAIL_BASE_LOW 0x20000
#define REG_POOLING_TAIL_BASE_HIGH 0x20001
#define REG_POOLING_OFFSET 0x20
#define REG_POOLING_STRIDE 0x20800
#define REG_POOLING_ROI_HEIGHT 0x20801
#define REG_POOLING_ROI_WIDTH 0x20802
#define REG_POOLING_ROI_X 0x20803
#define REG_POOLING_ROI_Y 0x20804
#define REG_POOLING_SRC_PAD_MODE 0x20805
#define REG_POOLING_GAVG_MODE 0x20810
#define REG_POOLING_GAVG_BIAS 0x20811
#define REG_POOLING_GAVG_MULTIPLY 0x20812
#define REG_POOLING_GAVG_DIV_ROUNDING 0x20813
#define REG_POOLING_INPUT_INT32 0x20814
#define REG_MAPTABLE_INDEX0 0x100
#define REG_MAPTABLE_INDEX1 0x140
#define REG_MAPTABLE_CHANGEINDEX 0x300
#define REG_MAPTABLE_LUTSET 0x304
#define REG_MAPTABLE_INTR 0x305
#define REG_MASKTABLE_INDEX0 0x200
#define REG_MASKTABLE_INDEX1 0x210
#define REG_MASKTABLE_CHANGEINDEX 0x301
#define REG_DMACOPYDATA_SRC_LOW 0x20C00
#define REG_DMACOPYDATA_SRC_HIGH 0x20C01
#define REG_DMACOPYDATA_DST_LOW 0x20C02
#define REG_DMACOPYDATA_DST_HIGH 0x20C03
#define REG_DMACOPYDATA_LEN 0x20C04
#define REG_DMACOPYDATA_ENABLE 0x20C05
#define REG_DMACOPYDATA_FINISH 0x20C06

#define REG_CROP_MAXFEATURE 0x21000
#define REG_CROP_INPUTSIZE 0x21001
#define REG_CROP_OUTPUTSIZE 0x21002
#define REG_CROP_STRIDE 0x21003
#define REG_CROP_PADDING_METHOD 0x21004

#define REG_YUV2RGB_RGBOFFSET 0x21408
#define REG_YUV2RGB_INPUTSIZE 0x21409
#define REG_YUV2RGB_INPUTSTRIDE 0x2140a

#define REG_RESIZE_SRCSTEP 0x21800
#define REG_RESIZE_DSTSTEP 0x21801
#define REG_RESIZE_CHANNEL 0x21802
#define REG_RESIZE_SRC_HEIGHT_STRIDE 0x21803
#define REG_RESIZE_DST_HEIGHT_STRIDE 0x21804
#define REG_RESIZE_INITX 0x21805
#define REG_RESIZE_DX 0x21806
#define REG_RESIZE_INITY 0x21807
#define REG_RESIZE_DY 0x21808
#define REG_RESIZE_PADMODE 0x21809
#define REG_RESIZE_PADVALUE 0x2180a

#define REG_ELMNTWZ_RESULT_QUANT 0x21c01
#define REG_ELMNTWZ_RESULT_MUL 0x21c02
#define REG_ELMNTWZ_INPUT0_QUANT 0x21c03
#define REG_ELMNTWZ_INPUT0_MUL 0x21c04
#define REG_ELMNTWZ_INPUT1_QUANT 0x21c05
#define REG_ELMNTWZ_INPUT1_MUL 0x21c06
#define REG_ELMNTWZ_FEATURE_STRIDE 0x21c07
#define REG_ELMNTWZ_MAX_CHANNEL 0x21c08
#define REG_ELMNTWZ_ACPMASK_LOCATION 0x21c09
#define REG_ELMNTWZ_ACPMASK_CONFIG 0x21c0a
#define REG_ELMNTWZ_LEFT_SHIFT 0x21c0b
#define REG_ELMNTWZ_POOLINGGLB_CONFIG 0x21c0c
#define REG_ELMNTWZ_PIC_STRIDE 0x21c0d
#define REG_ELMNTWZ_UPSAMPLE_PAD_VALUE 0x21c0e
#define REG_ELMNTWZ_POOLING_REQ_MUL 0x21c10
#define REG_ELMNTWZ_POOLING_REQ_SHIFT_ZP 0x21c11
#define REG_ELMNTWZ_RQTZ_CONFIG 0x21c12

#define REG_POOLING_TAIL_ADDR 0x64000
#define REG_CHICKEN_BASE 0xff
#define REG_QUEUE_RESET 0xfa
#define REG_CW_CTRL2 0xfe

#define REG_MATRIX_A_BASE_BASE 0xe0
#define REG_MATRIX_C_BASE_BASE 0xe1
#define REG_IMG2COL_BASE 0xe5
#define REG_MEM_ADDR_ENTRY_BASE 0x380
#define REG_MEM_DATA_ENTRY_BASE 0x3c0
#define REG_BIAS_BOUND 0xe4

//cache invalid
#define REG_CACHE_INVALID 0x281
//XPLI
#define REG_IM2COL_PIC_TIME_STRIDE 0x282
//2-0 time_num, 3- depthwise, 4-need_lsb_bd, 5-need_msb_bd, stride 10-8 height 27-16
#define REG_IM2COL_PIC_CTRL0 0x283
//15-0 dsp_per_line //31-16 width_stride
#define REG_IM2COL_PIC_CTRL1 0x285
//7-0 dsp_num 11-8 no use 18-16 pad_num_w_lsb 21-19 pad_num_h_lsb 24-22 pad_num_w_msb 27-25 pad_num_h_msb
#define REG_IM2COL_PIC_CTRL2 0x286
#define REG_IM2COL_PIC_CTRL3 0x289
#define REG_IM2COL_PIC_CTRL4 0x28a
#define REG_IM2COL_PIC_CTRL5 0x28b
#define REG_IM2COL_PIC_CTRL6 0x28c
#define REG_DWISE_PIC_FEATURE_GROUP_LOCK_VAL 0x28D
#define REG_IM2COL_PIC_CTRL7 0x28f
#define REG_WEIGHT_LOCK_STEP 0x291
#define REG_TOPK_FILTER_MODE 0x298
#define REG_TOPK_IDX_INIT_VALUE 0x299
#define REG_TOPK_TAIL_INIT_VALUE 0x29A
#define REG_WEIGHT_LOCK 0x270
#define REG_IMG_YUV2RGB_BB 0x21400
#define REG_IMG_YUV2RGB_BG 0x21401
#define REG_IMG_YUV2RGB_BR 0x21402
#define REG_IMG_YUV2RGB_YG 0x21403
#define REG_IMG_YUV2RGB_VR 0x21404
#define REG_IMG_YUV2RGB_VG 0x21405
#define REG_IMG_YUV2RGB_UG 0x21406
#define REG_IMG_YUV2RGB_UB 0x21407
#define REG_IMG_YUV2RGB_FRAMESIZE 0x21408
#define REG_IMG_YUV2RGB_W_H 0x21409
#define REG_IMG_YUV2RGB_W_STRIDE 0x2140a
#define REG_IMG_TRANS_FEATURE 0x22000
#define REG_IMG_TRANS_SRC_STEP 0x22001
#define REG_IMG_TRANS_SRC_STRIDE 0x22002
#define REG_IMG_TRANS_DST_STEP 0x22003
#define REG_IMG_TRANS_DST_STRIDE 0x22004
// TYPE: bit0 affine, bit1 gridface, bit2 interp, bit3 skip_div, bit4 dic_rounding, bit5-6 src_padding_method(0: use board value 1: use padding value), bit8-15 padding value
#define REG_IMG_TRANS_TYPE 0x22005
// 3:0 grd_w 7:4 grd_h
#define REG_IMG_TRANS_GRID_SIZE 0x22006
//0: Affine 1: opflow
#define REG_IMG_TRANS_FUNC 0x22007
#define REG_IMG_TRANS_OPFLOW_STEP 0x22008
//0: INT32 1: INT8
#define REG_IMG_TRANS_OPFLOW_FORMAT_ZP 0x22009
#define REG_IMG_TRANS_DEQ_SCALE 0x2200a
#define REG_IMG_TRANS_OPFLOW_STRIDE 0x2200b
#define REG_IMG_TRANS_CLAP_MAX 0x2200c

#define REG_FLOAT_CHANNEL_STEP 0x21c07
#define REG_INLINE_OP_EN 0x273
#endif

#ifdef TFACC20T
#define CFG_SUBMAU_ON_INIT     0x263
#define CREDIT                 0x271
#define CFG_MAU_B_ON_INIT      0x261
#define CFG_MAU_A_ON_INIT      0x262
#define CFG_MAU_ON             0x260
#define CFG_MAU_SHRINK         0x2c0
#define CFG_W_MAU_ON           0x264
#define CFG_W_VLD_ON           0x265
#define CFG_SM_CHANNEL_OFFSET  0x291
#define CFG_DUMMY_DLY0         0xf0
#define CFG_DUMMY_DLY1         0xf1

#define REG_VERSION_BASE 0x0
// queue0
#define REG_BLASOP_QUEUE_DEPTH_BASE      0x13
#define REG_RESULT_QUEUE_DEPTH_BASE      0x17
#define REG_BLASOP_QUEUE_BASE_LOW        0x10
#define REG_BLASOP_QUEUE_BASE_HIGH       0x11
#define REG_RESULT_QUEUE_BASE_LOW        0x14
#define REG_RESULT_QUEUE_BASE_HIGH       0x15
#define REG_RESULT_QUEUE_TAIL_BASE_LOW   0x18
#define REG_RESULT_QUEUE_TAIL_BASE_HIGH  0x19
#define REG_BLASOP_QUEUE_HEAD_BASE       0x13
#define REG_BLASOP_QUEUE_TAIL_BASE       0x12
#define REG_RESULT_QUEUE_HEAD_BASE       0x16
//queue1
#define REG_BLASOP_QUEUE1_DEPTH_BASE      0x33
#define REG_RESULT_QUEUE1_DEPTH_BASE      0x37
#define REG_BLASOP_QUEUE1_BASE_LOW        0x30
#define REG_BLASOP_QUEUE1_BASE_HIGH       0x31
#define REG_RESULT_QUEUE1_BASE_LOW        0x34
#define REG_RESULT_QUEUE1_BASE_HIGH       0x35
#define REG_RESULT_QUEUE1_TAIL_BASE_LOW   0x38
#define REG_RESULT_QUEUE1_TAIL_BASE_HIGH  0x39
#define REG_BLASOP_QUEUE1_HEAD_BASE       0x33
#define REG_BLASOP_QUEUE1_TAIL_BASE       0x32
#define REG_RESULT_QUEUE1_HEAD_BASE       0x36
//queue2
#define REG_BLASOP_QUEUE2_DEPTH_BASE      0x43
#define REG_RESULT_QUEUE2_DEPTH_BASE      0x47
#define REG_BLASOP_QUEUE2_BASE_LOW        0x40
#define REG_BLASOP_QUEUE2_BASE_HIGH       0x41
#define REG_RESULT_QUEUE2_BASE_LOW        0x44
#define REG_RESULT_QUEUE2_BASE_HIGH       0x45
#define REG_RESULT_QUEUE2_TAIL_BASE_LOW   0x48
#define REG_RESULT_QUEUE2_TAIL_BASE_HIGH  0x49
#define REG_BLASOP_QUEUE2_HEAD_BASE       0x43
#define REG_BLASOP_QUEUE2_TAIL_BASE       0x42
#define REG_RESULT_QUEUE2_HEAD_BASE       0x46
//queue3
#define REG_BLASOP_QUEUE3_DEPTH_BASE      0x53
#define REG_RESULT_QUEUE3_DEPTH_BASE      0x57
#define REG_BLASOP_QUEUE3_BASE_LOW        0x50
#define REG_BLASOP_QUEUE3_BASE_HIGH       0x51
#define REG_RESULT_QUEUE3_BASE_LOW        0x54
#define REG_RESULT_QUEUE3_BASE_HIGH       0x55
#define REG_RESULT_QUEUE3_TAIL_BASE_LOW   0x58
#define REG_RESULT_QUEUE3_TAIL_BASE_HIGH  0x59
#define REG_BLASOP_QUEUE3_HEAD_BASE       0x53
#define REG_BLASOP_QUEUE3_TAIL_BASE       0x52
#define REG_RESULT_QUEUE3_HEAD_BASE       0x56

#define REG_POOLING_TAIL_BASE_LOW      0x20000
#define REG_POOLING_TAIL_BASE_HIGH     0x20001
#define REG_POOLING_OFFSET             0x20
#define REG_POOLING_STRIDE             0X20800
#define REG_POOLING_ROI_HEIGHT         0X20801
#define REG_POOLING_ROI_WIDTH          0X20802
#define REG_POOLING_ROI_X              0X20803
#define REG_POOLING_ROI_Y              0X20804
#define REG_POOLING_SRC_PAD_MODE       0X20805
#define REG_POOLING_GAVG_MODE          0X20810
#define REG_POOLING_GAVG_BIAS          0X20811
#define REG_POOLING_GAVG_MULTIPLY      0X20812
#define REG_POOLING_GAVG_DIV_ROUNDING  0X20813
#define REG_POOLING_INPUT_INT32        0X20814
#define REG_MAPTABLE_INDEX0            0x100
#define REG_MAPTABLE_INDEX1            0x140
#define REG_MAPTABLE_CHANGEINDEX       0x300
#define REG_MAPTABLE_LUTSET            0x304
#define REG_MAPTABLE_INTR              0x305
#define REG_MASKTABLE_INDEX0           0x200
#define REG_MASKTABLE_INDEX1           0x210
#define REG_MASKTABLE_CHANGEINDEX      0x301
#define REG_DMACOPYDATA_SRC_LOW        0x20C00
#define REG_DMACOPYDATA_SRC_HIGH       0x20C01
#define REG_DMACOPYDATA_DST_LOW        0x20C02
#define REG_DMACOPYDATA_DST_HIGH       0x20C03
#define REG_DMACOPYDATA_LEN            0x20C04
#define REG_DMACOPYDATA_ENABLE         0x20C05
#define REG_DMACOPYDATA_FINISH         0x20C06

#define REG_CROP_MAXFEATURE      0x21000
#define REG_CROP_INPUTSIZE       0x21001
#define REG_CROP_OUTPUTSIZE      0x21002
#define REG_CROP_STRIDE          0x21003
#define REG_CROP_PADDING_METHOD  0x21004

#define REG_YUV2RGB_RGBOFFSET    0x21408
#define REG_YUV2RGB_INPUTSIZE    0x21409
#define REG_YUV2RGB_INPUTSTRIDE  0x2140a

#define REG_RESIZE_SRCSTEP            0x21800
#define REG_RESIZE_DSTSTEP            0x21801
#define REG_RESIZE_CHANNEL            0x21802
#define REG_RESIZE_SRC_HEIGHT_STRIDE  0x21803
#define REG_RESIZE_DST_HEIGHT_STRIDE  0x21804
#define REG_RESIZE_INITX              0x21805
#define REG_RESIZE_DX                 0x21806
#define REG_RESIZE_INITY              0x21807
#define REG_RESIZE_DY                 0x21808
#define REG_RESIZE_PADMODE            0x21809
#define REG_RESIZE_PADVALUE           0x2180a

#define REG_ELMNTWZ_RESULT_QUANT            0x21c01
#define REG_ELMNTWZ_RESULT_MUL              0x21c02
#define REG_ELMNTWZ_INPUT0_QUANT            0x21c03
#define REG_ELMNTWZ_INPUT0_MUL              0x21c04
#define REG_ELMNTWZ_INPUT1_QUANT            0x21c05
#define REG_ELMNTWZ_INPUT1_MUL              0x21c06
#define REG_ELMNTWZ_FEATURE_STRIDE          0x21c07
#define REG_ELMNTWZ_MAX_CHANNEL             0x21c08
#define REG_ELMNTWZ_ACPMASK_LOCATION        0x21c09
#define REG_ELMNTWZ_ACPMASK_CONFIG          0x21c0a
#define REG_ELMNTWZ_LEFT_SHIFT              0x21c0b
#define REG_ELMNTWZ_POOLINGGLB_CONFIG       0x21c0c
#define REG_ELMNTWZ_PIC_STRIDE              0x21c0d
#define REG_ELMNTWZ_UPSAMPLE_PAD_VALUE      0x21c0e
#define REG_ELMNTWZ_DST_CHANNEL_STEP        0x21c0f
#define REG_ELMNTWZ_POOLING_REQ_MUL         0x21c10
#define REG_ELMNTWZ_POOLING_REQ_SHIFT_ZP    0x21c11
#define REG_ELMNTWZ_RQTZ_CONFIG             0x21c12
#define REG_ELMNTWZ_FEATURE1_STRIDE         0x21c13
#define REG_ELMNTWZ_RQTZ_MULTIPLIER_SEL     0X21c14
#define REG_ELMNTWZ_OVRD0                   0x21c15
#define REG_ELMNTWZ_OVRD1                   0X21c16
#define REG_ELMNTWZ_OVRD_PATTERN            0x21c17

#define REG_POOLING_TAIL_ADDR 0x64000
#define REG_CHICKEN_BASE  0xff
#define REG_QUEUE_RESET   0xfa
#define REG_CW_CTRL2      0xfe

#define REG_LUT_LE_TABLE_ENTRY  0x22800
#define REG_LUT_LO_TABLE_ENTRY  0x22841
#define REG_LUT_PARAM           0x22950
#define REG_LUT_LE_START        0x22951
#define REG_LUT_LE_END          0x22952
#define REG_LUT_LO_START        0x22953
#define REG_LUT_LO_END          0x22954
#define REG_LUT_OVER_UNDER_FLOW 0x22955
#define REG_LUT_MIN_OFFSET      0x22956
#define REG_LUT_MAX_OFFSET      0x22957
#define REG_LUT_YAXLE_BND       0x22958
#define REG_LUT_PARAM1          0x22959
#define REG_LUT_EXP_PARAM       0x22960

#define REG_MATRIX_A_BASE_BASE   0xe0
#define REG_MATRIX_C_BASE_BASE   0xe1
#define REG_IMG2COL_BASE         0xe5
#define REG_FC_BANKSWITCH        0xe6
#define REG_MEM_ADDR_ENTRY_BASE  0x380
#define REG_MEM_DATA_ENTRY_BASE  0x3c0
#define REG_BIAS_BOUND           0xe4

//cache invalid
#define REG_CACHE_INVALID                     0x281
#define REG_CACHE_UNCACHEABLE                 0X2a0
//XPLI
#define REG_IM2COL_PIC_TIME_STRIDE            0x282
//2-0 time_num, 3- depthwise, 4-need_lsb_bd, 5-need_msb_bd, stride 10-8 height 27-16
#define REG_IM2COL_PIC_CTRL0                  0x283
//15-0 dsp_per_line //31-16 width_stride
#define REG_IM2COL_PIC_CTRL1                  0x285
//7-0 dsp_num 11-8 no use 18-16 pad_num_w_lsb 21-19 pad_num_h_lsb 24-22 pad_num_w_msb 27-25 pad_num_h_msb
#define REG_IM2COL_PIC_CTRL2                  0x286
#define REG_IM2COL_PIC_CTRL3                  0x289
#define REG_IM2COL_PIC_CTRL4                  0x28a
#define REG_IM2COL_PIC_CTRL5                  0x28b
#define REG_IM2COL_PIC_CTRL6                  0x28c
#define REG_DWISE_PIC_FEATURE_GROUP_LOCK_VAL  0x28d
#define REG_IM2COL_PIC_CTRL7                  0x28f
#define REG_IM2COL_PIC_CTRL8                  0x278
#define REG_IM2COL_PIC_CTRL9                  0x279
#define REG_IM2COL_PIC_CTRL10                 0x27a
#define REG_IM2COL_PIC_CTRL11                 0x27b
#define REG_WEIGHT_LOCK_STEP                  0x291
#define REG_TOPK_FILTER_MODE                  0X298
#define REG_TOPK_IDX_INIT_VALUE               0x299
#define REG_TOPK_TAIL_INIT_VALUE              0X29a
#define REG_WEIGHT_LOCK                       0x270
#define REG_IMG_YUV2RGB_BB         0x21400
#define REG_IMG_YUV2RGB_BG         0x21401
#define REG_IMG_YUV2RGB_BR         0x21402
#define REG_IMG_YUV2RGB_YG         0x21403
#define REG_IMG_YUV2RGB_VR         0x21404
#define REG_IMG_YUV2RGB_VG         0x21405
#define REG_IMG_YUV2RGB_UG         0x21406
#define REG_IMG_YUV2RGB_UB         0x21407
#define REG_IMG_YUV2RGB_FRAMESIZE  0x21408
#define REG_IMG_YUV2RGB_W_H        0x21409
#define REG_IMG_YUV2RGB_W_STRIDE   0x2140a
#define REG_IMG_TRANS_FEATURE      0x22000
#define REG_IMG_TRANS_SRC_STEP     0x22001
#define REG_IMG_TRANS_SRC_STRIDE   0x22002
#define REG_IMG_TRANS_DST_STEP     0x22003
#define REG_IMG_TRANS_DST_STRIDE   0x22004
//     TYPE: bit0 affine, bit1 gridface, bit2 interp, bit3 skip_div, bit4 dic_rounding,
// bit5-6 src_padding_method(0: use board value 1: use padding value), bit8-15 padding value
#define REG_IMG_TRANS_TYPE         0x22005
// 3:0 grd_w 7:4 grd_h
#define REG_IMG_TRANS_GRID_SIZE    0x22006
//0: Affine 1: opflow
#define REG_IMG_TRANS_FUNC         0x22007
#define REG_IMG_TRANS_OPFLOW_STEP  0x22008
//0: INT32 1: INT8
#define REG_IMG_TRANS_OPFLOW_FORMAT_ZP  0x22009
#define REG_IMG_TRANS_DEQ_SCALE         0x2200a
#define REG_IMG_TRANS_OPFLOW_STRIDE     0x2200b
#define REG_IMG_TRANS_CLAP_MAX          0x2200c

#define REG_FLOAT_CHANNEL_STEP  0x21c07
#define REG_INLINE_OP_EN        0x273

#define REG_SGBM_QUEUE_BASE                    0xf0000000
#define REG_ID                                 0x000
#define REG_VERSION                            0x004
#define REG_CMD_START                          0x008
#define REG_SRC_IMG_SIZE                       0x00C
#define REG_SRC_IMG_LEFT_BASE                  0x010
#define REG_SRC_IMG_RIGHT_BASE                 0x014
#define REG_SRC_IMG_STRIDE                     0x018
#define REG_PREV_DISPARITY_LEFT_BASE           0x01C
#define REG_PREV_DISPARITY_RIGHT_BASE          0x020
#define REG_PREV_DISPARITY_STRIDE              0x024
#define REG_DST_DISPARITY_LEFT_BASE            0x028
#define REG_DST_DISPARITY_RIGHT_BASE           0x02C
#define REG_DST_DISPARITY_STRIDE               0x030
#define REG_RESIZE_CTRL0                       0x034
#define REG_RESIZE_CTRL1                       0x038
#define REG_SGBM_CTRL0                         0x03C
#define REG_SGBM_CTRL1                         0x040
#define REG_SGBM_CTRL2                         0x044
#define REG_SGBM_CTRL3                         0x048
#define REG_SGBM_CTRL4                         0x04C
#define REG_CANNY_THR                          0x050
#define REG_GAUSS_KERNEL0                      0x054
#define REG_GAUSS_KERNEL1                      0x058
#define REG_GAUSS_SCALE0                       0x05C
#define REG_GAUSS_SCALE1                       0x060
#define REG_GAUSS_SCALE2                       0x064
#define REG_GAUSS_CLAP_NORM                    0x068
#define REG_INTR_EN                            0x06C
#define REG_INTR_MASK                          0x070
#define REG_INTR_STAT                          0x074
#define REG_INTR_STAT_RAW                      0x078
#define REG_STATUS                             0x07C
#define REG_SRC_IMG_RESIZE_ROI_INIT_X          0x084
#define REG_SRC_IMG_RESIZE_ROI_INIT_DX         0x088
#define REG_SRC_IMG_RESIZE_ROI_INIT_Y          0x08C
#define REG_SRC_IMG_RESIZE_ROI_INIT_DY         0x090
#define REG_SRC_IMG_RESIZE_SIZE                0x094
#define REG_PREV_DISPARITY_RESIZE_ROI_INIT_X   0x098
#define REG_PREV_DISPARITY_RESIZE_ROI_INIT_DX  0x09C
#define REG_PREV_DISPARITY_RESIZE_ROI_INIT_Y   0x0A0
#define REG_PREV_DISPARITY_RESIZE_ROI_INIT_DY  0x0A4
#define REG_PREV_DISPARITY_SIZE                0x0A8
#define REG_DST_DISPARITY_RESIZE_ROI_INIT_X    0x0AC
#define REG_DST_DISPARITY_RESIZE_ROI_INIT_DX   0x0B0
#define REG_DST_DISPARITY_RESIZE_ROI_INIT_Y    0x0B4
#define REG_DST_DISPARITY_RESIZE_ROI_INIT_DY   0x0B8
#define REG_DST_DISPARITY_RESIZE_SIZE          0x0BC
#define REG_POST_PROCESS_CTRL0                 0x0C0
#define REG_POST_PROCESS_CTRL1                 0x0C4
#define REG_TEMPORAL_FILTER0                   0x0C8
#define REG_TEMPORAL_L_BASE                    0x0CC
#define REG_TEMPORAL_R_BASE                    0x0D0
#define REG_TEMPORAL_STRIDE                    0x0D4
#define REG_INVALID_DISP_REPLACE               0x0D8
#define REG_SUBPIXEL_GFUNC0                    0x0E0
#define REG_SUBPIXEL_GFUNC1                    0x0E4
#define REG_SUBPIXEL_GFUNC2                    0x0E8
#define REG_SUBPIXEL_REPLACE                   0x0EC
#define REG_CLK_CTRL                           0x0F0
#define REG_CMDQ_BASE                          0x100
#define REG_CMDQ_CTRL                          0x104
#define REG_CMDQ1_BASE                         0x108
#define REG_CMDQ1_CTRL                         0x10C
#define REG_NON_FUNCTIONAL_BUS0                0x110
#define REG_NON_FUNCTIONAL_BUS1                0x114
#define REG_NON_FUNCTIONAL_BUS2                0x118
#define REG_NON_FUNCTIONAL_BUS3                0x11C
#define REG_MISC_CTRL                          0x120
#define REG_AXCACHE_OVRERRIDE                  0x124
#define REG_AXPROT_OVRERRIDE                   0x128
#define REG_ADDR_AXADDR_H                      0x12C
#define REG_WARP_SRC_BASE                      0x130
#define REG_WARP_DST_BASE                      0x134
#define REG_WARP_HMAT_BASE                     0x138
#define REG_WARP_SRC_SIZE                      0x13C
#define REG_WARP_DST_SIZE                      0x140
#define REG_WARP_STRIDE                        0x144
#define REG_WARP_HMAT_STRIDE                   0x148
#define REG_WARP_HMAT_STEP                     0x14C
#define REG_WARP_CTRL0                         0x150
#define REG_WARP_CTRL1                         0x154
#define REG_WARP_CTRL2                         0x158
#define REG_WARP_CTRL3                         0x15C
#define REG_WARP_CTRL4                         0x164
#define REG_WARP_CTRL5                         0x168
#define REG_WARP_START                         0x160
#define REG_DBG_CNT00                          0x200
#endif

uint32_t RegRead(void *deviceHandle, uint32_t index);
void RegWrite(void *deviceHandle, uint32_t index, uint32_t value);

#endif //PROJECT_REGISTER_H
