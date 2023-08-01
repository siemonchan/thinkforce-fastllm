//
// Created by huangyuyang on 11/16/20.
//

#ifndef PROJECT_IPLUGIN_H
#define PROJECT_IPLUGIN_H

#include <map>
#include <string>
#include "../Layer.h"

#define REGISTER_TFDL_PLUGIN_WITHTYPE(type, name) \
class name##TFDLPluginManager : TFDLBasePluginManager { \
private: \
    std::map <long long, name*> dict; \
    long long index; \
public:  \
    name##TFDLPluginManager() { \
        GetTFDLPluginManagerDict()[#type] = this; \
    } \
    void GetNewLayer(int &retIndex, IPlugin* &plugin) { \
        index++; \
        dict[index] = new name(); \
        retIndex = index; \
        plugin = (IPlugin*)dict[index]; \
    } \
    void Remove(int index) { \
        delete dict[index]; \
        dict.erase(index); \
    } \
}; \
name##TFDLPluginManager name##TFDLPluginManagerInstance;

#define REGISTER_TFDL_PLUGIN(name) REGISTER_TFDL_PLUGIN_WITHTYPE(name, name)

namespace tfdl {
    class IPlugin {
    public:
        IPlugin ();

        virtual ~IPlugin ();

        virtual void Prepare();

        virtual void Reshape(vector <vector <int> > inputDims, vector <vector <int> > &outputDims);

        virtual void Init(json11::Json config);

        virtual void ForwardFloat(vector <float*> inputs, vector <vector <int> > inputDims,
                                  vector <float*> outputs, vector <vector <int> > outputDims) = 0;
    };

    class IActivationOp : public IPlugin {
    public:
        IActivationOp ();

        virtual ~IActivationOp();

        void Reshape(vector <vector <int> > inputDims, vector <vector <int> > &outputDims) final;

        virtual void ForwardFloat(vector <float*> inputs, vector <vector <int> > inputDims,
                                  vector <float*> outputs, vector <vector <int> > outputDims) final;

        virtual void Init(json11::Json config);

        virtual float Function(float x) = 0;
    };

    class IBinaryOp : public IPlugin {
    public:
        IBinaryOp ();

        virtual ~IBinaryOp();

        void Reshape(vector <vector <int> > inputDims, vector <vector <int> > &outputDims) final;

        virtual void ForwardFloat(vector <float*> inputs, vector <vector <int> > inputDims,
                                  vector <float*> outputs, vector <vector <int> > outputDims) final;

        virtual void Init(json11::Json config);

        virtual float Function(float x, float y) = 0;
    };

    class TFDLBasePluginManager {
    public:
        virtual void GetNewLayer(int &retIndex, IPlugin* &plugin) = 0;

        virtual void Remove(int index) = 0;
    };

    std::map<std::string, TFDLBasePluginManager *> &GetTFDLPluginManagerDict();

    template<typename T>
    class PluginLayer : public Layer<T> {
    public:
        PluginLayer(const json11::Json &config, string dataType);

        ~PluginLayer();

        void Forward();

        void ForwardTF();

        void Prepare();

        void Reshape();

        json11::Json ToJson();
    private:
        int index;
        std::string realType;
        IPlugin *plugin;
        vector <uint8_t> int8mapTable;

        json11::Json param;
    };
}

#endif //PROJECT_IPLUGIN_H
