//
// Created by hyy on 19-5-21.
//
#ifndef PROJECT_GEMM_H
#define PROJECT_GEMM_H

#define GEMM_SUCC 1
#define GEMM_FAIL -1
#include <cstdint>
#ifdef USE_TFACC
#include <tfblas_api.hpp>
#include <common.hpp>
#endif

namespace tfdl {
#ifdef USE_TFACC

    //运算规则
    //result = ((A - [offsetA]) * (B - offsetB) * multiplier / 2147483647) >> rightShift

    //配置参数
    struct GemmConfig {
        int offsetA; //矩阵A的0值
        int offsetB; //矩阵B的0值
        int offsetOutput; //结果矩阵的0值
        int bias; //偏置
        int multiplier; //缩放系数
        uint8_t rightShift; //位移系数

        bool operator == (const GemmConfig &config) {
            return
                    (offsetA == config.offsetA) &&
                    (offsetB == config.offsetB) &&
                    (offsetOutput == config.offsetOutput) &&
                    (bias == config.bias) &&
                    (multiplier == config.multiplier) &&
                    (rightShift == config.rightShift);
        }
    };

    //快速矩阵A，限制rows = 4096, cols = 256
    class FastGemm1stParam {
    private:
        tfblas::MmapBuf<uint8_t> * weight = nullptr;
        uint8_t* bufferRow = nullptr;
    public:
        int rows, cols, tfaccId;

        //创建矩阵A
        FastGemm1stParam(int tfaccId);

        ~FastGemm1stParam();

        void Free();

        void operator=(const FastGemm1stParam &ori);

        FastGemm1stParam(const FastGemm1stParam&);

        tfblas::MmapBuf<uint8_t> * GetWeight() const;

        int FillRows(int from, int to, const uint8_t *buffer);

        const uint8_t* GetRowData(int row) const;
    };

    //矩阵A
    class Gemm1stParam {
    private:
        tfblas::MmapBuf<uint8_t> * weight = nullptr;
    public:
        int rows, cols, tfaccId;

        //创建矩阵A
        Gemm1stParam(int rows, int cols, int tfaccId);

        ~Gemm1stParam();

        void Free();

        void operator=(const Gemm1stParam &ori);

        Gemm1stParam(const Gemm1stParam&);

        tfblas::MmapBuf<uint8_t> * GetWeight() const;

        int FillRows(int from, int to, const uint8_t *buffer);

        const uint8_t* GetRowData(int row) const;
    };

    //矩阵B
    class Gemm2ndParam {
    private:
        long long record = 0;
        uint8_t *data = nullptr;
    public:
        int rows, cols;

        //创建矩阵B
        Gemm2ndParam(int rows, int cols);

        ~Gemm2ndParam();

        uint8_t * GetData() const;

        int FillCols(int from, int to, const uint8_t *buffer);

        const uint8_t* GetColData(int col) const;

        long long GetRecord() const;
    };

    //结果矩阵
    class GemmOutput {
        uint8_t *data = nullptr;
        tfblas::MmapBuf<uint8_t> * space;
        bool fastMode = false;
    public:

        int rows, cols;

        ~GemmOutput();

        void Reset(int rows, int cols);

        uint8_t *GetColData(int col) const;

        int GetAddr() const;

        void SetFastMode(bool fastMode);
    };

    class GemmInt32Output {
        int *data = nullptr;
    public:

        int rows, cols;

        ~GemmInt32Output();

        void Reset(int rows, int cols);

        int *GetColData(int col) const;
    };

    int DoGemm(const Gemm1stParam &A, const Gemm2ndParam &B, const GemmConfig &config, GemmOutput &output);

    //for i in [0, len) : 用A[inputOffset + i], B, config计算出output[outputOffset + i]
    int DoBatchGemm(const std::vector <Gemm1stParam> &A, const Gemm2ndParam &B, const GemmConfig &config, std::vector <GemmOutput> &output, const int &inputOffset, const int &outputOffset, const int &len);

    int DoBatchGemm(const std::vector <Gemm1stParam> &A, const Gemm2ndParam &B, const GemmConfig &config, std::vector <GemmOutput> &output);

    int DoGemm(const FastGemm1stParam &A, const Gemm2ndParam &B, const GemmConfig &config, GemmOutput &output);

    //for i in [0, len) : 用A[inputOffset + i], B, config计算出output[outputOffset + i]
    int DoBatchGemm(const std::vector <FastGemm1stParam> &A, const Gemm2ndParam &B, const GemmConfig &config, std::vector <GemmOutput> &output, const int &inputOffset, const int &outputOffset, const int &len);

    int DoBatchGemm(const std::vector <FastGemm1stParam> &A, const Gemm2ndParam &B, const GemmConfig &config, std::vector <GemmOutput> &output);

    int DoGemm(const FastGemm1stParam &A, const Gemm2ndParam &B, const GemmConfig &config, GemmInt32Output &output);

    //for i in [0, len) : 用A[inputOffset + i], B, config计算出output[outputOffset + i]
    int DoBatchGemm(const std::vector <FastGemm1stParam> &A, const Gemm2ndParam &B, const GemmConfig &config, std::vector <GemmInt32Output> &output, const int &inputOffset, const int &outputOffset, const int &len);

    int DoBatchGemm(const std::vector <FastGemm1stParam> &A, const Gemm2ndParam &B, const GemmConfig &config, std::vector <GemmInt32Output> &output);
#endif
}

#endif //PROJECT_GEMM_H
