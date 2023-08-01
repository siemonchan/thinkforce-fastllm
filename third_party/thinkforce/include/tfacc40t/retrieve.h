#ifndef TFACC_RETRIEVE_H
#define TFACC_RETRIEVE_H

#include "memory.h"

#include <cstdint>
#include <cstddef>
#include <vector>
#include <unordered_map>

namespace tfacc40t {
    const uint64_t RETRIEVE_INVALID_ID = (1LL << 62) - 1;
    const int RETRIEVE_INVALID_NORM2 = 1<<31;

    /// 检索接口每批query最大query特征数目
    /// \return 最大的query特征数目
    int RetrieveMaxQueryBatchSize();

    /// 初始化一个检索实例，返回实例的句柄，新实例默认使用0号TFACC
    /// \param featureDim :     特征维度
    /// \param zero :           uint8_t特征中的零点
    /// \param handle :         返回的句柄Id
    /// \return                 一个TF_RET值代表运行成功，或失败的类型
    TF_RET RetrieveCreate(int featureDim, uint8_t zero, int *handle);

    /// 释放一个检索实例
    /// \param handle : 待释放的句柄id
    /// \return         一个TF_RET值代表运行成功，或失败的类型
    TF_RET RetrieveRelease(int *handle);

    /// 在指定的检索实例中，往指定仓库添加特征
    /// \param repoId :       仓库id
    /// \param featureNum :   待添加的特征数量
    /// \param ids :          待添加的特征id列表，需要在外部申请好至少(featureNum * sizeof(uint64_t))的空间
    /// \param features :     特征列表，需要在外部申请好至少(featureNum * featureDim)的空间
    ///                       每个特征是连续的featureDim个字节
    /// \param handle :       检索实例的句柄
    /// \return               一个TF_RET值代表运行成功，或失败的类型
    TF_RET RetrieveAddFeature(int repoId, int featureNum, uint64_t* ids, uint8_t *features, int *handle);

    /// 在指定的检索实例中，往指定仓库添加特征（每个特征的仓库不同)
    /// \param featureNum :   待添加的特征数量
    /// \param repoIds :      仓库id 列表， 每个特征入哪个库 需要在外部分配好 featureNum 的空间
    /// \param ids :          待添加的特征id列表，需要在外部申请好至少(featureNum * sizeof(uint64_t))的空间
    /// \param features :     特征列表，需要在外部申请好至少(featureNum * featureDim)的空间
    ///                       每个特征是连续的featureDim个字节
    /// \param handle :       检索实例的句柄
    /// \return               一个TF_RET值代表运行成功，或失败的类型
    TF_RET RetrieveAddFeature(int featureNum, int* repoIds, uint64_t* ids, uint8_t *features, int *handle);

    /// 在指定的检索实例的指定仓库中删除特征
    /// 注意：删除后的特征空间会在60秒后才能被复用
    /// \param repoId :       仓库id
    /// \param featureNum :   待删除的特征数量
    /// \param ids :          待删除的特征id列表，需要在外部申请好至少(featureNum * sizeof(uint64_t))的空间
    /// \param handle :       检索实例的句柄
    /// \return               一个TF_RET值代表运行成功，或失败的类型
    TF_RET RetrieveRemoveFeature(int repoId, int featureNum, uint64_t* ids, int *handle);

    /// 在单个仓库中查询
    /// \param repoId :       仓库id
    /// \param queryNum :     询问的数量
    /// \param thresholds :   每个询问的阈值，需要在外部申请好至少(queryNum * sizeof(int))的空间
    ///                       thresholds[i]代表第i个询问中，过滤掉 (欧式距离平方 > thresholds[i])的特征
    /// \param querys :       询问列表，需要在外部申请好至少(queryNum * featureDim)的空间
    ///                       每个询问是连续的featureDim个字节
    /// \param topK :         对于每个询问返回的特征数量
    /// \param outputIds :    返回的特征id列表，需要在外部申请至少(queryNum * topK * sizeof(uint64_t))的空间
    ///                       每个询问的答案时连续的topK个uint64_t值描述特征id，结果按欧式距离从小到大排列
    /// \param outputScores : 返回的分数列表，需要在外部申请至少(queryNum * topK * sizeof(int32_t))的空间
    ///                       每个询问的答案是连续的topK个int32_t值描述欧式距离的平方，结果从小到大排列
    /// \param handle :       检索实例的句柄
    /// \return               一个TF_RET值代表运行成功，或失败的类型
    TF_RET RetrieveSearch(int repoId, int queryNum, int *thresholds, uint8_t *querys, int topK,
                          uint64_t *outputIds, int *outputScores,
                          int *handle);

    /// 在多个仓库中查询
    /// \param repoNum :      待查询的仓库数量
    /// \param repoIds :      仓库id列表，需要在外部申请好至少(repoNum * sizeof(int))的空间
    /// \param queryNum :     询问的数量
    /// \param thresholds :   每个询问的阈值，需要在外部申请好至少(queryNum * sizeof(int))的空间
    ///                       thresholds[i]代表第i个询问中，过滤掉 (欧式距离平方 > thresholds[i])的特征
    /// \param querys :       询问列表，需要在外部申请好至少(queryNum * featureDim)的空间
    ///                       每个询问是连续的featureDim个字节
    /// \param topK :         对于每个询问返回的特征数量
    /// \param outputIds :    返回的特征id列表，需要在外部申请至少(queryNum * topK * sizeof(uint64_t))的空间
    ///                       每个询问的答案时连续的topK个uint64_t值描述特征id，结果按欧式距离从小到大排列
    /// \param outputScores : 返回的分数列表，需要在外部申请至少(queryNum * topK * sizeof(int32_t))的空间
    ///                       每个询问的答案是连续的topK个int32_t值描述欧式距离的平方，结果从小到大排列
    /// \param handle :       检索实例的句柄
    /// \return               一个TF_RET值代表运行成功，或失败的类型
    TF_RET RetrieveSearch(int repoNum, int *repoIds, int queryNum, int *thresholds, uint8_t *querys, int topK,
                          uint64_t *outputIds, int *outputScores,
                          int *handle);


    /// 在多个仓库中查询, 每个仓库上的query不同
    /// \param repoIds :      要检索的仓库 ID list 
    /// \param topK 
    /// \param queryNums :    每个要检索的库上的query数量， queryNums.size() == repoIds.size()
    /// \param qIds      :    每个要检索的库上的每个query的id(外部制定，唯一),  qIds[i].size() == queryNums[i]
    /// \param thresholds :   对应的每个query的检索阈值, thresholds[i].size() == queryNums[i]
    /// \param result:        检索结果, qid -> [(score, dbid)]
    TF_RET RetrieveSearch(const std::vector<int>& repoIds, int topK,
                          const std::vector<int>& queryNums,  // querys on repoIds[i]
                          const std::vector<std::vector<int> >& qIds,    // qIds[i].size() == queryNums[i]
                          const std::vector<std::vector<int> >& qNorms,
                          const std::vector<std::vector<int> >& thresholds,  // thresholds[i].size() == queryNums[i]
                          const std::vector<std::vector<uint8_t> >& querys,  // query[i].size() == queryNums[i] * featureDim 
                          std::unordered_map<int, std::vector<std::pair<int, uint64_t> > >& result,
                          int* handle);

    /// 在多个库中查询，每个仓库上的query相互独立，各自计算topK
    /// \param repoIds :        要检索的仓库ID List
    /// \param topK :
    /// \param queryNums :      每个库上的query数量， queryNums.size() == repoIds.size()
    /// \param thresholds :     每个query的阈值，thresholds[i].size() == queryNums[i]
    /// \param querys :         每个query的特征，query[i].size() == queryNums[i] * featureDim
    /// \param outputIds :      返回的特征id列表，需要在外部申请至少(queryNum * topK * sizeof(uint64_t))的空间
    ///                         每个询问的答案时连续的topK个uint64_t值描述特征id，结果按欧式距离从小到大排列
    ///                         vector的大小同repoIds，即每个repo上查询结果分开存放
    /// \param outputScores:    返回的分数列表，需要在外部申请至少(queryNum * topK * sizeof(int32_t))的空间
    ///                         每个询问的答案是连续的topK个int32_t值描述欧式距离的平方，结果从小到大排列
    ///                         vector的大小同repoIds，即每个repo上查询结果分开存放
    /// \param handle :
    /// \return :               TF_RET值代表运行成功，或失败的类型
    TF_RET RetrieveSearch(const std::vector<int>& repoIds, int topK,
                          const std::vector<int>& queryNums,  // querys on repoIds[i]
                          const std::vector<std::vector<int> >& thresholds,  // thresholds[i].size() == queryNums[i]
                          const std::vector<std::vector<uint8_t> >& querys,  // query[i].size() == queryNums[i] * featureDim
                          std::vector<uint64_t *> outputIds, std::vector<int *> outputScores,
                          int* handle);

    /// 在多个库中查询，每个仓库上的query相互独立，各自计算topK（带保护机制）
    /// \param repoIds :        要检索的仓库ID List
    /// \param topK :
    /// \param queryNums :      每个库上的query数量， queryNums.size() == repoIds.size()
    /// \param thresholds :     每个query的阈值，thresholds[i].size() == queryNums[i]
    /// \param querys :         每个query的特征，query[i].size() == queryNums[i] * featureDim
    /// \param limits :         当query[i][j]检索到limits[i][j]个距离在threshold内的特征后，不在对后续repo中数据进行检索
    ///                         limits.size() == repoIds.size(), limits[i].size() == queryNums[i]
    /// \param outputIds :      返回的特征id列表，需要在外部申请至少(queryNum * topK * sizeof(uint64_t))的空间
    ///                         每个询问的答案时连续的topK个uint64_t值描述特征id，结果按欧式距离从小到大排列
    ///                         vector的大小同repoIds，即每个repo上查询结果分开存放
    /// \param outputScores:    返回的分数列表，需要在外部申请至少(queryNum * topK * sizeof(int32_t))的空间
    ///                         每个询问的答案是连续的topK个int32_t值描述欧式距离的平方，结果从小到大排列
    ///                         vector的大小同repoIds，即每个repo上查询结果分开存放
    /// \param outputFlags :    每个query的返回标记, outputFlags.size() == repoIds.size(), outputFlags[i].size() == queryNums[i]
    ///                         变量内存空间需要外部准备
    ///                         outputFlags[i][j]:
    ///                             bit[0]      - 1发生截断
    ///                             bit[1-7]    - 未定义
    /// \param handle :
    /// \return :               TF_RET值代表运行成功，或失败的类型
    TF_RET RetrieveSearch(const std::vector<int>& repoIds, int topK,
                          const std::vector<int>& queryNums,  // querys on repoIds[i]
                          const std::vector<std::vector<int> >& thresholds,  // thresholds[i].size() == queryNums[i]
                          const std::vector<std::vector<uint8_t> >& querys,  // query[i].size() == queryNums[i] * featureDim
                          const std::vector<std::vector<int> >& limits,
                          const std::vector<uint64_t *>& outputIds, const std::vector<int *>& outputScores,
                          const std::vector<int *>& outputFlags,
                          int* handle);

    /// 异步特征检索
    /// \param taskId       handle内唯一的任务id
    /// \param topK
    /// \param limit        topK计算过程中，如果检索到阈值内的特征数已经达到limit，则停止检索
    /// \param queryNum     n
    /// \param queryNorms   每个query特征的norm2。size == queryNum
    /// \param threshold    检索阈值。size == queryNum
    /// \param querys       检索的特征数据，n个特征数据。size == queryNum*featureDim
    /// \param dbFeatureNum N
    /// \param dbFeatures   被检索的特征数据，N个特征数据。size == dbFeatureNum*featureDim
    /// \param dbNorms      每个被检索特征的norm2值
    /// \param outputOffsets    检索到的特征偏移，即在db中的第几个特征，从0开始编号。size == queryNum*topK
    /// \param outputScores     分值。size == queryNum*topK
    /// \param outputFlags      每个query的检索标志。size == queryNum
    ///                         outputFlags[i][j]:
    ///                             bit[0]          - 1发生截断
    ///                             bit[1-31]       - 未定义
    /// \param outputCounts     每个特征检索到并返回的特征个数。size == queryNum
    /// \param handle       retrieve handle
    TF_RET RetrieveSearchAsync(int taskId, int topK, int limit,
                                  int queryNum, int * queryNorms, int* threshold, uint8_t* querys,
                                  int dbFeatureNum, uint8_t* dbFeatures, int * dbNorms,
                                  uint32_t* outputOffsets, int* outputScores, int* outputFlags, int* outputCounts,
                                  int* handle);

    /// 获取已经完成异步特征检索的结果
    /// \param completedTaskIdList  已经完成的检索任务Id列表
    /// \param handle               retrieve handle
    TF_RET RetrieveSearchAsyncFetch(std::vector<int>& completedTaskIdList, int* handle);

    /// 获取上一次查询操作中topK排序操作的耗时
    /// \param topKTime :     输出参数，上一次查询操作中topK排序操作的耗时
    /// \param handle :       检索实例的句柄
    /// \return               一个TF_RET值代表运行成功，或失败的类型
    TF_RET RetrieveGetTopKTime(double *topKTime, int *handle);

    /// 获取一个特征id的具体特征
    /// \param repoId :       仓库id
    /// \param id :           需要获取的特征id
    /// \param feature :      输出参数，存储获取的特征，需要在外部申请至少featureDim字节的空间
    /// \param handle :       检索实例的句柄
    /// \return               一个TF_RET值代表运行成功，或失败的类型
    TF_RET RetrieveGetFeature(int repoId, uint64_t id, uint8_t *feature, int *handle);

    /// 删除一个仓库
    /// \param repoId :       待删除的仓库id
    /// \param handle :       检索实例的句柄
    /// \return               一个TF_RET值代表运行成功，或失败的类型
    TF_RET RetrieveRemoveRepo(int repoId, int *handle);

    /// 设置检索实例使用的TFACC设备集合
    /// \param deviceNum :  使用的设备数量
    /// \param deviceIds :  使用的设备id列表，长度至少为deviceNum 其中每个值为0~7之间的数代表设备id, 0~3号为tfacc full, 4~7为tfacc lite
    /// \param handle :     检索实例的句柄
    /// \return             一个TF_RET值代表运行成功，或失败的类型
    TF_RET SetDevices(int deviceNum, int *deviceIds, int *handle);

    /// 设置检索是否开启Profile 模式（会输出详细信息，默认关闭)
    /// \param isProfileMode :  是否开启
    /// \return                 一个TF_RET值代表运行成功，或失败的类型
    TF_RET SetProfileMode(bool isProfileMode);

    /// 获取是否开启profile模式
    /// \return     true-开启
    bool IsProfileMode();
}

#endif //TFACC_RETRIEVE_H
