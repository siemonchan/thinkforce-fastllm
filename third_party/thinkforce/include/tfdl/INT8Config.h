//cd
// Created by yuyang.huang on 17-11-17.
//


#include <map>
#include <vector>
#include <algorithm>
#include <cassert>
#include <cstring>
#include <tgmath.h>
#ifdef _USE_NEON
#include <arm_neon.h>
#endif

#ifndef TFDL_INT8CONFIG_H
#define TFDL_INT8CONFIG_H

using namespace std;

typedef std::uint8_t magicType;

namespace tfdl {
    struct NonuniformTable {
        int bit = 10; // default high accuracy bit
        uint16_t *table = nullptr;
        uint8_t *invTable = nullptr;

        NonuniformTable() = default;

        ~NonuniformTable() {
            delete[] table;
            delete[] invTable;
        }

        bool update(const uint16_t *t) {
            // if this table has been initialized, and pointer to new table is same with present table, return directly
            if (t == this->table && this->table && this->invTable) {
                return true;
            }

            // if this table is null, create one and assign
            if (!this->table) {
                this->table = new uint16_t[256];
            }
            memcpy(table, t, sizeof(uint16_t) * 256);
            this->bit = int(ceil(log2(static_cast<double>(this->table[(1 << 8) - 1]))));
            if (!this->invTable) {
                this->invTable = new uint8_t[1 << bit];
                for (int i = 0; i < 256; i++) {
                    int st;
                    int end;
                    if (i == 0) {
                        st = -1;
                        end = this->table[1] / 2;
                    } else if (i == 255) {
                        st = (this->table[255] + this->table[254]) / 2;
                        end = this->table[255];
                    } else {
                        st = (this->table[i] + this->table[i - 1]) / 2;
                        end = (this->table[i] + this->table[i + 1]) / 2;
                    }
                    for (int j = st + 1; j <= end; j++) {
                        this->invTable[j] = uint8_t(i);
                    }
                }
            }
            return true;
        }
    };

    class QuantizationConfig {
    private:
        float min, max;
        bool minFixed = false, maxFixed = false;
        magicType zeroPoint;
        //double scale;
        float scale;
        NonuniformTable nonuniformTable;
    public:
        QuantizationConfig();
        QuantizationConfig(float min, float max);
        QuantizationConfig(float scale, magicType zeroPoint);
        QuantizationConfig(float scale, uint16_t zeroPoint);
        ~QuantizationConfig(){}
        void reset();
        void update(const float &value);
        void update(const float &minValue, const float &maxValue);
        magicType quantization(const float &realNumber) const;
        float invQuantization(const magicType &qNumber) const;
        float invQuantizationWithoutScale(const magicType &qNumber);
#ifdef ARM
        void quantall(int len, float* in, magicType * out);
        void invquantall(int len, magicType* in, float* out);
#endif
        float getScale() const ;
        magicType getZeroPoint() const ;
        float getMin() const;
        float getMax() const;

        // Once min or max is set fixed, you can`t update it, but you can free them by
        // calling freeMin() and freeMax()
        void fixMin(float min);
        void fixMax(float max);
        bool freeMin();
        bool freeMax();

        bool equal(const QuantizationConfig& b) const;

        void getParamWithBit(int bit, int &zeroPoint, float &scale) const; //获取任意bit量化参数
        void invQuantizationWithBit(int bit, int len, int* input, float* output) const; //任意bit量化
        void quantizationWithBit(int bit, int len, float* input, int* output) const; //任意bit反量化

        void getParamWithTFBit(uint8_t &zeroPoint); //获取非均匀int8的参数
        void invQuantizationWithTFBit(int bit, int len, uint8_t* input, float* output); //8位非均匀量化模拟bit位均匀量化 - 反量化(非均匀int8 -> float -> bit位均匀量化(float表示))
        void quantizationWithTFBit(int bit, int len, float* input, uint8_t* output); //8位非均匀量化模拟bit位均匀量化 - 量化(float -> bit位均匀量化 -> 非均匀int8)

        bool updateNonuniformTable(const uint16_t *table);
        NonuniformTable getNonuniformTable() const;
    };

    class Int16QuantizationConfig {
    private:
        float min, max;
        uint16_t zeroPoint;
        float scale;
    public:
        Int16QuantizationConfig();
        Int16QuantizationConfig(float min, float max);
        Int16QuantizationConfig(float scale, uint16_t zeroPoint);
        ~Int16QuantizationConfig() {}

        void reset();
        void update(const float &value);
        void update(const float &minValue, const float &maxValue);
        uint16_t quantization(const float &realNumber);
        float invQuantization(const uint16_t &qNumber);
        float invQuantizationWithoutScale(const uint16_t &qNumber);

        float getScale();
        uint16_t getZeroPoint();
        float getMin();
        float getMax();
    };

    class INT8Config {
    private:
        map<pair<string, pair <int, int> >, QuantizationConfig *> weightConfig;
        map<string, QuantizationConfig *> dataConfig;

    public:
        ~INT8Config(){
            for(auto it = weightConfig.begin();it!=weightConfig.end();it++){
                delete (*it).second;
                (*it).second = nullptr;
            }
            weightConfig.clear();
            for(map<string, QuantizationConfig *>::iterator it = dataConfig.begin();it!=dataConfig.end();it++){
                delete (*it).second;
                (*it).second = nullptr;
            }
            dataConfig.clear();
        }
        QuantizationConfig *addWeightConfig(const string &layerName, int weightId, int channels, const float &min, const float &max);
        QuantizationConfig *addWeightConfig(const string &layerName, int weightId, int channels, const float &scale, const magicType &zeroPoint);

        QuantizationConfig *addDataConfig(const string &dataName, const float &min, const float &max);
        QuantizationConfig *addDataConfig(const string &dataName, const float &sclae, const magicType &zeroPoint);

        QuantizationConfig *getWeightConfig(const string &layerName, int weightId, int channelId);

        QuantizationConfig *GetDataConfig(const string &layerName);

        bool IsPerChannelConfig(const string &layerName, int channel);

        void ExportDataConfig(map <string, QuantizationConfig> &config);

        void ImportDataConfig(map <string, QuantizationConfig> &config);
    };

    void QuantizeMultiplierSmallerThanOne(double real_multiplier,
                                                 std::int32_t *quantized_multiplier,
                                                 int *right_shift);
    void QuantizeMultiplier(double double_multiplier, int32_t* quantized_multiplier, int* shift);
    std::int32_t SaturatingRoundingDoublingHighMul(std::int32_t a,
                                                                 std::int32_t b);
}

#endif //CAFFE_INT8CONFIG_H
