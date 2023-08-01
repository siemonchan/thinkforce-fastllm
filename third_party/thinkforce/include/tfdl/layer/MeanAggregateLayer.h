//
//
#ifndef TFDL_MEAN_AGGREGATE_LAYER_H
#define TFDL_MEAN_AGGREGATE_LAYER_H

#include "../Layer.h"
#include "IPlugin.h"

namespace tfdl {

class MeanAggregateLayer : public IPlugin {
    public:
        MeanAggregateLayer() : IPlugin () {}

        void Reshape(vector <vector <int> > inputDims, vector <vector <int> > &outputDims);

        // // (1 x 1 x H x W) X (1 x 1 x H x D) -> 1 x 1 x H x D (W <= H)
        void ForwardFloat(vector <float*> inputs, vector <vector <int> > inputDims,
                          vector <float*> outputs, vector <vector <int> > outputDims);
};

 
}

#endif //TFDL_MEAN_AGGREGATE_LAYER_H
