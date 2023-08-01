//
// Created by cjq on 18-9-29.
//

#ifndef PROJECT_NNAPIBUILDLAYERCONFIG_H
#define PROJECT_NNAPIBUILDLAYERCONFIG_H

#include "NNAPI/NNAPI.h"

int32_t ReadInt32(const void *value);
uint32_t ReadUInt32(const void *value);
float ReadFloat(const void *value);
vector<float> ReadFloatTensor(const void *value, uint32_t dimensionCount, vector<uint32_t> dimensions);
vector<int32_t > ReadInt32Tensor(const void *value, uint32_t dimensionCount, vector<uint32_t> dimensions);


const json11::Json BuildAddLayerConfig(uint32_t inputCount, const uint32_t *inputs,
                                       uint32_t outputCount, const uint32_t *outputs,
                                       std::vector<uint32_t> dataDimensionCount,
                                       std::vector<std::vector<uint32_t >> dataDimensions,
                                       std::vector<size_t> dataLength,
                                       std::map<uint32_t, const void *> &dataValue);

const json11::Json BuildAveragePool2DLayerConfig(uint32_t inputCount, const uint32_t *inputs,
                                                 uint32_t outputCount, const uint32_t *outputs,
                                                 std::vector<uint32_t> dataDimensionCount,
                                                 std::vector<std::vector<uint32_t >> dataDimensions,
                                                 std::vector<size_t> dataLength,
                                                 std::map<uint32_t, const void *> &dataValue);

const json11::Json BuildBatchToSpaceNDLayerConfig(uint32_t inputCount, const uint32_t *inputs,
                                                  uint32_t outputCount, const uint32_t *outputs,
                                                  std::vector<uint32_t> dataDimensionCount,
                                                  std::vector<std::vector<uint32_t >> dataDimensions,
                                                  std::vector<size_t> dataLength,
                                                  std::map<uint32_t, const void *> &dataValue);

const json11::Json BuildConcateLayerConfig(uint32_t inputCount, const uint32_t *inputs,
                                           uint32_t outputCount, const uint32_t *outputs,
                                           std::vector<uint32_t> dataDimensionCount,
                                           std::vector<std::vector<uint32_t >> dataDimensions,
                                           std::vector<size_t> dataLength,
                                           std::map<uint32_t, const void *> &dataValue);

const json11::Json BuildConv2DLayerConfig(uint32_t inputCount, const uint32_t *inputs,
                                          uint32_t outputCount, const uint32_t *outputs,
                                          std::vector<uint32_t> dataDimensionCount,
                                          std::vector<std::vector<uint32_t >> dataDimensions,
                                          std::vector<size_t> dataLength,
                                          std::map<uint32_t, const void *> &dataValue);

const json11::Json BuildDepthwiseConv2DLayerConfig(uint32_t inputCount, const uint32_t *inputs,
                                                   uint32_t outputCount, const uint32_t *outputs,
                                                   std::vector<uint32_t> dataDimensionCount,
                                                   std::vector<std::vector<uint32_t >> dataDimensions,
                                                   std::vector<size_t> dataLength,
                                                   std::map<uint32_t, const void *> &dataValue);

const json11::Json BuildDepthToShapeLayerConfig(uint32_t inputCount, const uint32_t *inputs,
                                                uint32_t outputCount, const uint32_t *outputs,
                                                std::vector<uint32_t> dataDimensionCount,
                                                std::vector<std::vector<uint32_t >> dataDimensions,
                                                std::vector<size_t> dataLength,
                                                std::map<uint32_t, const void *> &dataValue);

const json11::Json BuildDequantizeLayerConfig(uint32_t inputCount, const uint32_t *inputs,
                                              uint32_t outputCount, const uint32_t *outputs,
                                              std::vector<uint32_t> dataDimensionCount,
                                              std::vector<std::vector<uint32_t >> dataDimensions,
                                              std::vector<size_t> dataLength,
                                              std::map<uint32_t, const void *> &dataValue);

const json11::Json BuildEmbeddingLookupLayerConfig(uint32_t inputCount, const uint32_t *inputs,
                                                   uint32_t outputCount, const uint32_t *outputs,
                                                   std::vector<uint32_t> dataDimensionCount,
                                                   std::vector<std::vector<uint32_t >> dataDimensions,
                                                   std::vector<size_t> dataLength,
                                                   std::map<uint32_t, const void *> &dataValue);

const json11::Json BuildFloorLayerConfig(uint32_t inputCount, const uint32_t *inputs,
                                         uint32_t outputCount, const uint32_t *outputs,
                                         std::vector<uint32_t> dataDimensionCount,
                                         std::vector<std::vector<uint32_t >> dataDimensions,
                                         std::vector<size_t> dataLength,
                                         std::map<uint32_t, const void *> &dataValue);

const json11::Json BuildFullyConnectedLayerConfig(uint32_t inputCount, const uint32_t *inputs,
                                                  uint32_t outputCount, const uint32_t *outputs,
                                                  std::vector<uint32_t> dataDimensionCount,
                                                  std::vector<std::vector<uint32_t >> dataDimensions,
                                                  std::vector<size_t> dataLength,
                                                  std::map<uint32_t, const void *> &dataValue);

const json11::Json BuildLayerHashLookupConfig(uint32_t inputCount, const uint32_t *inputs,
                                              uint32_t outputCount, const uint32_t *outputs,
                                              std::vector<uint32_t> dataDimensionCount,
                                              std::vector<std::vector<uint32_t >> dataDimensions,
                                              std::vector<size_t> dataLength,
                                              std::map<uint32_t, const void *> &dataValue);

const json11::Json BuildL2NormLayerConfig(uint32_t inputCount, const uint32_t *inputs,
                                          uint32_t outputCount, const uint32_t *outputs,
                                          std::vector<uint32_t> dataDimensionCount,
                                          std::vector<std::vector<uint32_t >> dataDimensions,
                                          std::vector<size_t> dataLength,
                                          std::map<uint32_t, const void *> &dataValue);

const json11::Json BuildL2PoolLayerConfig(uint32_t inputCount, const uint32_t *inputs,
                                          uint32_t outputCount, const uint32_t *outputs,
                                          std::vector<uint32_t> dataDimensionCount,
                                          std::vector<std::vector<uint32_t >> dataDimensions,
                                          std::vector<size_t> dataLength,
                                          std::map<uint32_t, const void *> &dataValue);

const json11::Json BuildLRNLayerConfig(uint32_t inputCount, const uint32_t *inputs,
                                       uint32_t outputCount, const uint32_t *outputs,
                                       std::vector<uint32_t> dataDimensionCount,
                                       std::vector<std::vector<uint32_t >> dataDimensions,
                                       std::vector<size_t> dataLength,
                                       std::map<uint32_t, const void *> &dataValue);

const json11::Json BuildLogisticLayerConfig(uint32_t inputCount, const uint32_t *inputs,
                                            uint32_t outputCount, const uint32_t *outputs,
                                            std::vector<uint32_t> dataDimensionCount,
                                            std::vector<std::vector<uint32_t >> dataDimensions,
                                            std::vector<size_t> dataLength,
                                            std::map<uint32_t, const void *> &dataValue);

const json11::Json BuildLSHProjectionLayerConfig(uint32_t inputCount, const uint32_t *inputs,
                                                 uint32_t outputCount, const uint32_t *outputs,
                                                 std::vector<uint32_t> dataDimensionCount,
                                                 std::vector<std::vector<uint32_t >> dataDimensions,
                                                 std::vector<size_t> dataLength,
                                                 std::map<uint32_t, const void *> &dataValue);

const json11::Json BuildLSTMLayerConfig(uint32_t inputCount, const uint32_t *inputs,
                                        uint32_t outputCount, const uint32_t *outputs,
                                        std::vector<uint32_t> dataDimensionCount,
                                        std::vector<std::vector<uint32_t >> dataDimensions,
                                        std::vector<size_t> dataLength,
                                        std::map<uint32_t, const void *> &dataValue);

const json11::Json BuildMaxPoolLayerConfig(uint32_t inputCount, const uint32_t *inputs,
                                           uint32_t outputCount, const uint32_t *outputs,
                                           std::vector<uint32_t> dataDimensionCount,
                                           std::vector<std::vector<uint32_t >> dataDimensions,
                                           std::vector<size_t> dataLength,
                                           std::map<uint32_t, const void *> &dataValue);

const json11::Json BuildMulLayerConfig(uint32_t inputCount, const uint32_t *inputs,
                                       uint32_t outputCount, const uint32_t *outputs,
                                       std::vector<uint32_t> dataDimensionCount,
                                       std::vector<std::vector<uint32_t >> dataDimensions,
                                       std::vector<size_t> dataLength,
                                       std::map<uint32_t, const void *> &dataValue);

const json11::Json BuildReLuLayerConfig(uint32_t inputCount, const uint32_t *inputs,
                                        uint32_t outputCount, const uint32_t *outputs,
                                        std::vector<uint32_t> dataDimensionCount,
                                        std::vector<std::vector<uint32_t >> dataDimensions,
                                        std::vector<size_t> dataLength,
                                        std::map<uint32_t, const void *> &dataValue);

const json11::Json BuildReshapeLayerConfig(uint32_t inputCount, const uint32_t *inputs,
                                           uint32_t outputCount, const uint32_t *outputs,
                                           std::vector<uint32_t> dataDimensionCount,
                                           std::vector<std::vector<uint32_t >> dataDimensions,
                                           std::vector<size_t> dataLength,
                                           std::map<uint32_t, const void *> &dataValue);

const json11::Json BuildResizeBilinearLayerConfig(uint32_t inputCount, const uint32_t *inputs,
                                                  uint32_t outputCount, const uint32_t *outputs,
                                                  std::vector<uint32_t> dataDimensionCount,
                                                  std::vector<std::vector<uint32_t >> dataDimensions,
                                                  std::vector<size_t> dataLength,
                                                  std::map<uint32_t, const void *> &dataValue);

const json11::Json BuildRNNLayerConfig(uint32_t inputCount, const uint32_t *inputs,
                                       uint32_t outputCount, const uint32_t *outputs,
                                       std::vector<uint32_t> dataDimensionCount,
                                       std::vector<std::vector<uint32_t >> dataDimensions,
                                       std::vector<size_t> dataLength,
                                       std::map<uint32_t, const void *> &dataValue);

const json11::Json BuildSoftmaxLayerConfig(uint32_t inputCount, const uint32_t *inputs,
                                           uint32_t outputCount, const uint32_t *outputs,
                                           std::vector<uint32_t> dataDimensionCount,
                                           std::vector<std::vector<uint32_t >> dataDimensions,
                                           std::vector<size_t> dataLength,
                                           std::map<uint32_t, const void *> &dataValue);

const json11::Json BuildSpaceToDepthLayerConfig(uint32_t inputCount, const uint32_t *inputs,
                                                uint32_t outputCount, const uint32_t *outputs,
                                                std::vector<uint32_t> dataDimensionCount,
                                                std::vector<std::vector<uint32_t >> dataDimensions,
                                                std::vector<size_t> dataLength,
                                                std::map<uint32_t, const void *> &dataValue);

const json11::Json BuildDivLayerConfig(uint32_t inputCount, const uint32_t *inputs,
                                       uint32_t outputCount, const uint32_t *outputs,
                                       std::vector<uint32_t> dataDimensionCount,
                                       std::vector<std::vector<uint32_t >> dataDimensions,
                                       std::vector<size_t> dataLength,
                                       std::map<uint32_t, const void *> &dataValue);

const json11::Json BuildMeanLayerConfig(uint32_t inputCount, const uint32_t *inputs,
                                        uint32_t outputCount, const uint32_t *outputs,
                                        std::vector<uint32_t> dataDimensionCount,
                                        std::vector<std::vector<uint32_t >> dataDimensions,
                                        std::vector<size_t> dataLength,
                                        std::map<uint32_t, const void *> &dataValue);

const json11::Json BuildPadLayerConfig(uint32_t inputCount, const uint32_t *inputs,
                                       uint32_t outputCount, const uint32_t *outputs,
                                       std::vector<uint32_t> dataDimensionCount,
                                       std::vector<std::vector<uint32_t >> dataDimensions,
                                       std::vector<size_t> dataLength,
                                       std::map<uint32_t, const void *> &dataValue);

const json11::Json BuildSpaceToBatchNDLayerConfig(uint32_t inputCount, const uint32_t *inputs,
                                                  uint32_t outputCount, const uint32_t *outputs,
                                                  std::vector<uint32_t> dataDimensionCount,
                                                  std::vector<std::vector<uint32_t >> dataDimensions,
                                                  std::vector<size_t> dataLength,
                                                  std::map<uint32_t, const void *> &dataValue);

const json11::Json BuildSqueezeLayerConfig(uint32_t inputCount, const uint32_t *inputs,
                                           uint32_t outputCount, const uint32_t *outputs,
                                           std::vector<uint32_t> dataDimensionCount,
                                           std::vector<std::vector<uint32_t >> dataDimensions,
                                           std::vector<size_t> dataLength,
                                           std::map<uint32_t, const void *> &dataValue);

const json11::Json BuildStridedSliceLayerConfig(uint32_t inputCount, const uint32_t *inputs,
                                                uint32_t outputCount, const uint32_t *outputs,
                                                std::vector<uint32_t> dataDimensionCount,
                                                std::vector<std::vector<uint32_t >> dataDimensions,
                                                std::vector<size_t> dataLength,
                                                std::map<uint32_t, const void *> &dataValue);

const json11::Json BuildSubLayerConfig(uint32_t inputCount, const uint32_t *inputs,
                                       uint32_t outputCount, const uint32_t *outputs,
                                       std::vector<uint32_t> dataDimensionCount,
                                       std::vector<std::vector<uint32_t >> dataDimensions,
                                       std::vector<size_t> dataLength,
                                       std::map<uint32_t, const void *> &dataValue);

const json11::Json BuildTanhLayerConfig(uint32_t inputCount, const uint32_t *inputs,
                                        uint32_t outputCount, const uint32_t *outputs,
                                        std::vector<uint32_t> dataDimensionCount,
                                        std::vector<std::vector<uint32_t >> dataDimensions,
                                        std::vector<size_t> dataLength,
                                        std::map<uint32_t, const void *> &dataValue);

const json11::Json BuildTransposeLayerConfig(uint32_t inputCount, const uint32_t *inputs,
                                             uint32_t outputCount, const uint32_t *outputs,
                                             std::vector<uint32_t> dataDimensionCount,
                                             std::vector<std::vector<uint32_t >> dataDimensions,
                                             std::vector<size_t> dataLength,
                                             std::map<uint32_t, const void *> &dataValue);

const json11::Json BuildBatchNormalLayerConfig(json11::Json previousLayerConfig, json11::Json presentLayerConfig,
                                               string realAddInput, string realMulInput, bool subdiv);

const json11::Json BuildScaleLayerConfig(json11::Json previousLayerConfig, json11::Json presentLayerConfig,
                                         string realAddInput, string realMulInput);

const json11::Json BuildDetectionOutputLayerConfig(uint32_t inputCount, const uint32_t *inputs,
                                                   uint32_t outputCount, const uint32_t *outputs,
                                                   std::vector<uint32_t> dataDimensionCount,
                                                   std::vector<std::vector<uint32_t >> dataDimensions,
                                                   std::vector<size_t> dataLength,
                                                   std::map<uint32_t, const void *> &dataValue);

const json11::Json BuildFlattenLayerConfig(uint32_t inputCount, const uint32_t *inputs,
                                           uint32_t outputCount, const uint32_t *outputs,
                                           std::vector<uint32_t> dataDimensionCount,
                                           std::vector<std::vector<uint32_t >> dataDimensions,
                                           std::vector<size_t> dataLength,
                                           std::map<uint32_t, const void *> &dataValue);

const json11::Json BuildPriorBoxLayerConfig(uint32_t inputCount, const uint32_t *inputs,
                                            uint32_t outputCount, const uint32_t *outputs,
                                            std::vector<uint32_t> dataDimensionCount,
                                            std::vector<std::vector<uint32_t >> dataDimensions,
                                            std::vector<size_t> dataLength,
                                            std::map<uint32_t, const void *> &dataValue);

const json11::Json BuildShuffleChannelLayerConfig(uint32_t inputCount, const uint32_t *inputs,
                                                  uint32_t outputCount, const uint32_t *outputs,
                                                  std::vector<uint32_t> dataDimensionCount,
                                                  std::vector<std::vector<uint32_t >> dataDimensions,
                                                  std::vector<size_t> dataLength,
                                                  std::map<uint32_t, const void *> &dataValue);

const json11::Json BuildUpsampleLayerConfig(uint32_t inputCount, const uint32_t *inputs,
                                            uint32_t outputCount, const uint32_t *outputs,
                                            std::vector<uint32_t> dataDimensionCount,
                                            std::vector<std::vector<uint32_t >> dataDimensions,
                                            std::vector<size_t> dataLength,
                                            std::map<uint32_t, const void *> &dataValue);
#endif //PROJECT_NNAPIBUILDLAYERCONFIG_H
