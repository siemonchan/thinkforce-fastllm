//
// Created by huangyuyang on 10/14/21.
//

#ifndef TFACC_MEMORY_H
#define TFACC_MEMORY_H

#include <cstdint>
#include "tfnn.h"

namespace tfacc40t {

    template <typename T>
    struct Memory {
        int chipId;
        TFNN_Memory mem;
        uint64_t bytes; // 字节数
        uint64_t size; // 元素数
        volatile T *userAddr; // 虚拟地址
        uint64_t phyAddr; // 物理地址
        bool duplicate;

        Memory() {}

        // 创建可容纳size个元素的memory
        Memory (int chipId, uint64_t size);

        ~Memory();

        // 返回低位地址
        uint32_t LowAddr();

        // 返回高位地址
        uint32_t HighAddr();

        Memory *Duplicate();
    };

    typedef Memory <uint8_t> MEM_U8;
    typedef Memory <int8_t> MEM_S8;
    typedef Memory <uint16_t> MEM_U16;
    typedef Memory <int16_t> MEM_S16;
    typedef Memory <uint32_t> MEM_U32;
    typedef Memory <int32_t> MEM_S32;
    typedef Memory <uint64_t> MEM_U64;
    typedef Memory <int64_t> MEM_S64;
    typedef Memory <float> MEM_FLOAT;
    typedef Memory <double> MEM_DOUBLE;
}

#endif //TFACC_MEMORY_H
