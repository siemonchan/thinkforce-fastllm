//
// Created by huangyuyang on 10/13/21.
//

#ifndef TFACC_TFNN_H
#define TFACC_TFNN_H

#include <cstdio>
#include <iostream>
#include <cstdint>

enum TF_RET {
    TF_RET_SUCC = 0, // 成功
    TF_RET_FAIL = 1, // 失败
    TF_RET_INVALID_DEVICE = 2, // 设备不合法
    TF_RET_MALLOC_REGISTER_FAILED = 3, // 分配寄存器失败
    TF_RET_MALLOC_MEMORY_FAILED = 4, // 分配内存失败
    TF_RET_INVALID_HANDLE = 5, // 无效句柄
    TF_RET_DEVICE_BUSY = 6, // 设备忙
    TF_RET_INVISIBLE_DEVICE = 7, // 当前不可见设备
};

// 描述TFNN内存的结构体
struct TFNN_Memory {
    uint64_t size; // 字节数
    volatile void *userAddr; // 虚拟地址
    uint64_t phyAddr; // 物理地址

    uint32_t LowAddr() {
        return phyAddr & 0x00000000ffffffffUL;
    }

    uint32_t HighAddr() {
        return (phyAddr & 0xffffffff00000000UL) >> 32;
    }
};

/// 检查设备Id是否可用
/// \param deviceId : 设备id
/// \return : 一个TF_RET值，TF_RET_SUCC代表成功，TF_RET_INVALID_DEVICE代表设备不合法
TF_RET TF_TFNN_CheckDeviceId(int deviceId);

/// 检查设备Id位于哪个芯片
/// \param deviceId : 设备id
/// \return : 一个int值代表芯片ID，-1代表查询失败
int TF_TFNN_GetChipId(int deviceId);

/// 获取设备Id的类型
/// \param deviceId : 设备id
/// \return : 一个int值代表类型，0代表lite，1代表full
int TF_TFNN_GetChipType(int deviceId);

/// 获得芯片数量
/// \return 芯片数量
int TF_TFNN_GetChipNum();

/// TFACC硬重启
/// \return : 一个TF_RET值，TF_RET_SUCC代表成功
TF_RET TF_TFNN_HARDWARE_RESET();

/// 锁住TFACC
/// \return : 一个TF_RET值，TF_RET_SUCC代表成功
TF_RET TF_TFNN_LockDevice_SEM();

/// 信号量解锁TFACC
/// \return : 一个TF_RET值，TF_RET_SUCC代表成功
TF_RET TF_TFNN_UnlockDevice_SEM();

/// 锁住TFACC
/// \param tfaccId : tfacc号
/// \return : 一个TF_RET值，TF_RET_SUCC代表成功
TF_RET TF_TFNN_LockDevice_SEM(int tfaccId);

/// 信号量解锁TFACC
/// \param tfaccId : tfacc号
/// \return : 一个TF_RET值，TF_RET_SUCC代表成功
TF_RET TF_TFNN_UnlockDevice_SEM(int tfaccId);

/// 锁定所有device
/// \return : 一个TF_RET值，TF_RET_SUCC代表成功
TF_RET TF_TFNN_LockAllDevice();

/// 锁定所有device
/// \return : 一个TF_RET值，TF_RET_SUCC代表成功，TF_RET_INVALID_DEVICE代表设备不合法
TF_RET TF_TFNN_UnlockAllDevice();

/// 锁定一个device
/// \param deviceId : 待锁定的deviceId
/// \return : 一个TF_RET值，TF_RET_SUCC代表成功，TF_RET_INVALID_DEVICE代表设备不合法
TF_RET TF_TFNN_LockDevice(int deviceId);

/// 解锁一个device
/// \param deviceId : 待解锁的deviceId
/// \return : 一个TF_RET值，TF_RET_SUCC代表成功，TF_RET_INVALID_DEVICE代表设备不合法
TF_RET TF_TFNN_UnlockDevice(int deviceId);

/// 获取一个空闲的deviceId并锁定这个device
/// \param deviceId : 输出变量，获取到的deviceId
/// \return : 一个TF_RET值，TF_RET_SUCC代表成功，TF_RET_INVALID_DEVICE代表设备不合法
TF_RET TF_TFNN_GetAndLockFreeDevice(int *deviceId);

/// 创建一片连续内存
/// \param chipId : 芯片ID
/// \param bytes : 待创建的字节数
/// \param mem : 待创建的内存
/// \return : 一个TF_RET值代表创建成功，或失败的类型
TF_RET TF_TFNN_Malloc(int chipId, uint64_t bytes, TFNN_Memory *mem);

/// 释放一片连续内存
/// \param chipId : 芯片ID
/// \param mem : 待释放的内存
/// \return : 一个TF_RET值代表创建成功，或失败的类型
TF_RET TF_TFNN_Free(int chipId, TFNN_Memory *mem);

///
/// \param chipId : 芯片ID
/// \param userAddr : 查询的虚拟地址
/// \param phyAddr : 返回的物理地址，若失败则返回0x0
/// \return : 一个TF_RET值代表创建成功，或失败的类型
TF_RET TF_TFNN_GetPhyAddr(int chipId, uint64_t userAddr, uint64_t *phyAddr);

/// 获取当前剩余的reserveDDR空间大小
/// \param chipId : 芯片ID
/// \param result : 剩余的reserveDDR空间大小
/// \return : 一个TF_RET值代表创建成功，或失败的类型
TF_RET TF_TFNN_GetFreeSpaceSize(int chipId, uint64_t *result);

/// 运行一个命令字队列
/// \param device : 运行命令字的设备编号
/// \param mem : 命令字存放的地址
/// \param len : 命令字条数
/// \return 一个TF_RET值代表运行成功，或失败的类型
TF_RET TF_TFNN_Run(int device, TFNN_Memory *mem, int len);

/// 异步执行一个命令队列
/// \param device : 运行命令字的设备编号
/// \param mem : 命令字存放的地址
/// \param len : 命令字条数
/// \param handle : 返回的执行句柄
/// \return 一个TF_RET值代表运行成功，或失败的类型
TF_RET TF_TFNN_Launch(int device, TFNN_Memory *mem, int len, int *handle);

/// 等待一个命令字队列执行完成
/// \param handle : 执行句柄
/// \param isFinish : 返回是否执行完成
/// \return 一个TF_RET值代表运行成功，或失败的类型
TF_RET TF_TFNN_IsFinish(int *handle, bool *isFinish);

/// 等待一个命令字队列执行完成
/// \param handle : 执行句柄
/// \return 一个TF_RET值代表运行成功，或失败的类型
TF_RET TF_TFNN_Wait(int *handle);

/// 读取寄存器
/// \param device : 设备编号
/// \param pos : 需要读取的寄存器位置
/// \param result : 返回的寄存器值
/// \return 一个TF_RET值代表运行成功，或失败的类型
TF_RET TF_TFNN_ReadReg(int device, int pos, int *result);

/// 写入寄存器
/// \param device : 设备编号
/// \param pos : 需要写入的寄存器位置
/// \param value : 写入的寄存器值
/// \return 一个TF_RET值代表运行成功，或失败的类型
TF_RET TF_TFNN_WriteReg(int device, int pos, int value);

/// Cache操作
/// \param device : 设备编号
/// \param addr : 起始地址
/// \param len : 长度
/// \param type : 类型
/// \return 一个TF_RET值代表运行成功，或失败的类型
TF_RET TF_TFNN_InvalidCache(int device, uint32_t addr, uint32_t len, int type);

#endif //TFACC_TFNN_H
