//
// Created by osboxes on 13/12/17.
//
#ifndef TFDL_BATCHNORMLAYER_H
#define TFDL_BATCHNORMLAYER_H

#include "../Layer.h"

namespace tfdl {
    template<typename T>
    class BatchNormLayer : public Layer<T> {
    public:
        BatchNormLayer(const json11::Json &config, string dataType) : Layer<T>(config, dataType) {
            this->weights.push_back(new Data<T>);
            this->weights.push_back(new Data<T>);
            this->weights.push_back(new Data<T>);
            auto &param = config["param"];
            _eps = param["eps"].is_null()? _eps:param["eps"].number_value();
            /*vector<int> sz;
            sz.push_back(_channels);

            _mean = new Data<T>();
            _mean->resize(sz);
            _mean->reallocate(0);

            _variance = new Data<T>();
            _variance->resize(sz);
            _variance->reallocate(0);

            _temp = new Data<T>();
            _temp->resize(this->inputs[0]->GetDims());
            _temp->reallocate(0);

            _batch_sum_multiplier = new Data<T>();
            sz[0] = this->inputs[0]->GetDim(0);
            _batch_sum_multiplier->resize(sz);
            _batch_sum_multiplier->reallocate(1.0);

            int spatial_dim = this->inputs[0]->Count(0) / (_channels * this->inputs[0]->GetDim(0));
            _spatial_sum_mutiplier = new Data<T>();
            sz[0] = spatial_dim;
            _spatial_sum_mutiplier->resize(sz);
            _spatial_sum_mutiplier->reallocate(1.0);

            int numbychans = _channels * this->inputs[0]->GetDim(0);
            sz[0] = numbychans;
            _num_by_chans = new Data<T>();
            _num_by_chans->resize(sz);
            _num_by_chans->reallocate(1.0);*/

        }

        ~BatchNormLayer() {
            if (!this->ShareAttr) {
                for (Data<T> *d : this->weights) {
                    delete d;
                }
            }
            /*if (_mean != nullptr) {
                delete _mean;
                _mean = nullptr;
            }

            if (_variance != nullptr) {
                delete _variance;
                _variance = nullptr;
            }
            if (_temp != nullptr) {
                delete _temp;
                _temp = nullptr;
            }

            if (_batch_sum_multiplier != nullptr) {
                delete _batch_sum_multiplier;
                _batch_sum_multiplier = nullptr;
            }
            if (_spatial_sum_mutiplier != nullptr) {
                delete _spatial_sum_mutiplier;
                _spatial_sum_mutiplier = nullptr;
            }
            if (_num_by_chans != nullptr) {
                delete _num_by_chans;
                _num_by_chans = nullptr;
            }*/
        }

        void Forward();
        void ForwardTF();
        void cpuFloatBN(float* input, float* output, int num, int spatial);
        void Prepare();

        json11::Json ToJson();


        int _channels;

        float _eps = 0.000010;


    };


}

#endif //CAFFE_BATCHNORMLAYER_H
