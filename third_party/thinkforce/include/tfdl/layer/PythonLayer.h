//
// Created by feng.jianhao on 19-2-21.
//

#ifndef PROJECT_PYTHONLAYER_H
#define PROJECT_PYTHONLAYER_H

#include <boost/python.hpp>
#include <boost/python/numpy.hpp>
#include <numpy/arrayobject.h>
#include <numpy/arrayobject.h>
#include <numpy/ndarrayobject.h>
#include <numpy/ndarraytypes.h>
#include <numpy/npy_math.h>
#include <vector>

#include "Layer.h"

namespace bp = boost::python;

namespace tfdl{

    template<typename T>
    class PythonLayer : public Layer<T>{
    public:
        PythonLayer(bp::object module, const json11::Json &config, string dataType) : Layer<T>(config, dataType){
            auto param = config["param"];
            string pythonlayername =  param["pythonlayer"].string_value();
            string json;
            param.dump(json);
            self_ = module.attr(pythonlayername.c_str())(json,dataType,this->outputDataType);

        }

        void Forward();
        void ForwardTF(){
            self_.attr("ForwardTF")();
        };
        void Reshape();
        json11::Json ToJson(){};
    private:
        bp::object self_;
    };

}
#endif //PROJECT_PYTHONLAYER_H
