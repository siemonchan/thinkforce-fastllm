# thinkforce-fastllm

## 介绍

thinkfoce-fastllm是基于[fastllm](https://github.com/ztxz16/fastllm.git)开发的，支持使用ThinkForce TFACC的大模型推理库

支持ChatGLM-6B, ChatGLM2-6B, Baichuan, Baichuan2, Qwen, Llama2等开源大语言模型

## 使用方法

0. 登录到你的ThinkForce服务器(运行ChatGLM-6B起码需要双核心TF7140或单核心TF7180服务器)
1. 安装相关依赖

```sh
apt install gcc g++ cmake
apt install numactl libnuma-dev
```

2. 下载和编译

``` sh
git clone https://github.com/siemonchan/thinkforce-fastllm.git
cd thinkforce-fastllm
mkdir build
cd build
cmake .. -DUSE_TFACC=ON # 如果不使用TFCC, 那么使用 cmake .. -DUSE_TFACC=OFF
make -j
```

3. 模型获取, 参考fastllm[模型获取](#模型获取), 若要使用TFACC, 需要导出int8格式模型
4. 运行模型(以ChatGLM-6B为例)

``` sh
./main -p chatglm-6b-int8.bin -t 16
```

5. 运行web demo

``` sh
./webui -p chatglm-6b-int8.bin -t 16
```

## Python接口
thinkforce-fastllm同样支持使用python接口调用TFACC，你可以在完成编译后

```sh
cd tools
python3 setup.py install
```

然后通过python脚本调用大模型(以ChatGLM-6B为例)

```sh
cd -
python3 tools/cli_demo.py -p chatglm-6b-int8.bin -t 16
```

## 推理速度

ChatGLM-6B (token / s):
|       平台|   batch 16|   batch 64|  batch 256|
|----------:|----------:|----------:|----------:|
|     TF7180|       35.8|       82.6|       98.6|
| AMD 5975WX|       37.9|       43.0|       45.4|

ChatGLM2-6B (token / s):
|       平台|   batch 16|   batch 64|  batch 256|
|----------:|----------:|----------:|----------:|
|     TF7180|       40.9|       92.7|      111.1|
| AMD 5975WX|       35.2|       37.0|       41.8|

Llama2-7b-chat (token / s):
|       平台|   batch 16|   batch 64|  batch 256|
|----------:|----------:|----------:|----------:|
|     TF7180|       23.5|       41.8|       47.2|
| AMD 5975WX|       27.6|       40.9|       44.7|

Qwen7B-chat (token / s):
|       平台|   batch 16|   batch 64|  batch 256|
|----------:|----------:|----------:|----------:|
|     TF7180|       25.7|       52.9|       70.5|
| AMD 5975WX|       35.7|       38.7|       40.9|

Baichuan2-7b-chat (token/s)
|       平台|   batch 16|   batch 64|  batch 256|
|----------:|----------:|----------:|----------:|
|     TF7180|       34.0|       64.4|       74.0|
| AMD 5975WX|       23.8|       20.0|       20.4|

推理速度使用编译生成的benchmark程序得到，你可以通过如下方式查看它的用法

```sh
./benchmark -h
TFNN Compiled at Jul 28 2023, 01:49:36
Usage:
[-h|--help]:                  显示帮助
<-p|--path> <args>:           模型文件的路径
<-t|--threads> <args>:        使用的线程数量
<-l|--limit> <args>:          输出token数限制
<-b|--batch> <args>:          batch数
<-f|--file> <args>:           输入文件，文件中每行一个prompt，如果行数不足batch则用之前的prompt补充
```

## VISION LANGUAGE MODEL

大规模视觉语言模型是一种可以以图像、文字、检测框作为输入，并以文本和检测框作为输出的大语言模型，本项目也支持这样的多模态模型。以下是具体的介绍。

### [QWen-VL](https://huggingface.co/Qwen/Qwen-VL-Chat)

![](./example/visual/demo.jpeg)
```sh
./vl -p qwen-vl-chat-int8.flm -t 16 -i demo.jpeg # 类似于运行普通的大语言模型，但是增加了一个-i参数用于传入图像
欢迎使用 qwen 模型. 输入内容对话，reset清空历史记录，stop退出程序.
加载图片：demo.jpeg
用户: 这张图里有什么
qwen:这张图片中有一个黄色的拉布拉多犬和一个坐在沙滩上的女孩。他们坐在沙滩上，面前是蓝色的海洋。在女孩的脚边还放着一个黑色的袋子。在阳光的照射下，画面显得非常温馨。
用户: 输出“拉布拉多犬”的检测框
qwen:<ref>拉布拉多犬</ref><box>(233,430),(585,900)</box>
```

注意：运行VL模型需要用到opencv，你可以通过以下方式安装

```sh
sudo apt update
sudo apt install libopencv-dev
```


---
# Original fastllm README:

## 介绍

fastllm是纯c++实现，无第三方依赖的高性能大模型推理库

6~7B级模型在安卓端上也可以流畅运行

部署交流QQ群： 831641348

| [快速开始](#快速开始) | [模型获取](#模型获取) | [开发计划](#开发计划) |

## 功能概述

- 🚀 纯c++实现，便于跨平台移植，可以在安卓上直接编译
- 🚀 ARM平台支持NEON指令集加速，X86平台支持AVX指令集加速，NVIDIA平台支持CUDA加速，各个平台速度都很快就是了
- 🚀 支持浮点模型（FP32), 半精度模型(FP16), 量化模型(INT8, INT4) 加速
- 🚀 支持多卡部署，支持GPU + CPU混合部署
- 🚀 支持Batch速度优化
- 🚀 支持并发计算时动态拼Batch
- 🚀 支持流式输出，很方便实现打字机效果
- 🚀 支持python调用
- 🚀 前后端分离设计，便于支持新的计算设备
- 🚀 目前支持ChatGLM模型，各种LLAMA模型(ALPACA, VICUNA等)，BAICHUAN模型，MOSS模型

## 两行代码加速 （测试中，暂时只支持ubuntu）

使用如下命令安装fastllm_pytools包

``` sh
cd fastllm
mkdir build
cd build
cmake .. -DUSE_CUDA=ON # 如果不使用GPU编译，那么使用 cmake .. -DUSE_CUDA=OFF
make -j
cd tools && python setup.py install
```

然后只需要在原本的推理程序中加入两行即可使用fastllm加速

``` python
# 这是原来的程序，通过huggingface接口创建模型
from transformers import AutoTokenizer, AutoModel
tokenizer = AutoTokenizer.from_pretrained("THUDM/chatglm2-6b", trust_remote_code = True)
model = AutoModel.from_pretrained("THUDM/chatglm2-6b", trust_remote_code = True)

# 加入下面这两行，将huggingface模型转换成fastllm模型
# 目前from_hf接口只能接受原始模型，或者ChatGLM的int4, int8量化模型，暂时不能转换其它量化模型
from fastllm_pytools import llm
model = llm.from_hf(model, tokenizer, dtype = "float16") # dtype支持 "float16", "int8", "int4"

# 注释掉这一行model.eval()
#model = model.eval()
```

model支持了ChatGLM的API函数chat, stream_chat，因此ChatGLM的demo程序无需改动其他代码即可运行

model还支持下列API用于生成回复

``` python
# 生成回复
print(model.response("你好"))

# 流式生成回复
for response in model.stream_response("你好"):
    print(response, flush = True, end = "")
```

转好的模型也可以导出到本地文件，之后可以直接读取，也可以使用fastllm cpp接口读取

``` python
model.save("model.flm"); # 导出fastllm模型
new_model = llm.model("model.flm"); # 导入fastllm模型
```

注: 该功能处于测试阶段，目前仅验证了ChatGLM、ChatGLM2模型可以通过2行代码加速

## PEFT支持(测试中，目前仅支持ChatGLM + LoRA)

使用[🤗PEFT](https://huggingface.co/docs/peft/index)可以方便地运行finetune过的大模型，你可以使用如下的方式让你的PEFT模型使用fastllm加速：

```python
import sys
from peft import PeftModel
from transformers import AutoModel, AutoTokenizer
sys.path.append('..')
model = AutoModel.from_pretrained("THUDM/chatglm-6b", device_map='cpu', trust_remote_code=True)
model = PeftModel.from_pretrained(model, "path/to/your/own/adapter") # 这里使用你自己的peft adapter
model = model.eval()
tokenizer = AutoTokenizer.from_pretrained("THUDM/chatglm-6b", trust_remote_code=True)

# 如果模型中存在active_adapter，那么在fastllm模型中，这个adapter也会被默认启用
from fastllm_pytools import llm
model = llm.from_hf(model, tokenizer, dtype = "float16") # dtype支持 "float16", "int8", "int4"
```

接下来，你就可以像使用普通的模型一样(例如调用chat，stream_chat函数)

你也可以更换PEFT模型所使用的的adapter：

```python
model.set_adapter('your adapter name')
```

或者关闭PEFT，使用原本的预训练模型：

```python
model.disable_adapter()
```

## 推理速度

6B级int4模型单4090延迟最低约5.5ms

6B级fp16模型单4090最大吞吐量超过10000 token / s

6B级int4模型在骁龙865上速度大约为4~5 token / s

[详细测试数据点这里](docs/benchmark.md)

## CMMLU精度测试

|              模型  | Data精度 |  CMMLU分数 |
|-----------------: |-------- |------------|
| ChatGLM2-6b-fp16  | float32 |  50.16     |
| ChatGLM2-6b-int8  | float32 |  50.14     |
| ChatGLM2-6b-int4  | float32 |  49.63     |

目前测试了ChatGLM2模型，具体测试步骤点[这里](test/cmmlu/README.md)

## 快速开始

### 编译

建议使用cmake编译，需要提前安装c++编译器，make, cmake

gcc版本建议9.4以上，cmake版本建议3.23以上

GPU编译需要提前安装好CUDA编译环境，建议使用尽可能新的CUDA版本

使用如下命令编译

``` sh
cd fastllm
mkdir build
cd build
cmake .. -DUSE_CUDA=ON # 如果不使用GPU编译，那么使用 cmake .. -DUSE_CUDA=OFF
make -j
```

编译完成后，可以使用如下命令安装简易python工具包 (暂时只支持Linux)

``` sh
cd tools # 这时在fastllm/build/tools目录下
python setup.py install
```

### 运行demo程序

我们假设已经获取了名为`model.flm`的模型（参照 [模型获取](#模型获取)，初次使用可以先下载转换好的模型)

编译完成之后在build目录下可以使用下列demo:
``` sh
# 这时在fastllm/build目录下

# 命令行聊天程序, 支持打字机效果
./main -p model.flm 

# 简易webui, 使用流式输出 + 动态batch，可多路并发访问
./webui -p model.flm --port 1234 

# python版本的命令行聊天程序，使用了模型创建以及流式对话效果
python tools/cli_demo.py -p model.flm 

# python版本的简易webui，需要先安装streamlit-chat
streamlit run tools/web_demo.py model.flm 

```

### 简易python调用

编译后如果安装了简易python工具包，那么可以使用python来调用一些基本的API （如果没有安装，也可以在直接import编译生成的tools/fastllm_pytools来使用)

``` python
# 模型创建
from fastllm_pytools import llm
model = llm.model("model.flm")

# 生成回复
print(model.response("你好"))

# 流式生成回复
for response in model.stream_response("你好"):
    print(response, flush = True, end = "")
```

另外还可以设置cpu线程数等内容，详细API说明见 [fastllm_pytools](docs/fastllm_pytools)

这个包不包含low level api，如果需要使用更深入的功能请参考 [Python绑定](#Python绑定)


## Python绑定

```
mkdir build-py
cd build-py
cmake .. -DPY_API=ON -DUSE_CUDA=ON （只使用CPU则使用 cmake .. -DPY_API=ON 即可）
make -j
cd -
python cli.py  -m chatglm -p chatglm-6b-int8.bin 或  
python web_api.py  -m chatglm -p chatglm-6b-int8.bin  
```
上述web api可使用python web_api_client.py进行测试

## 多卡部署

### fastllm_pytools中使用多卡部署

``` python

from fastllm_pytools import llm
# 支持下列三种方式，需要在模型创建之前调用
llm.set_device_map("cuda:0") # 将模型部署在单一设备上
llm.set_device_map(["cuda:0", "cuda:1"]) # 将模型平均部署在多个设备上
llm.set_device_map({"cuda:0" : 10, "cuda:1" : 5, "cpu": 1}) # 将模型按不同比例部署在多个设备上

```

### pybinding中使用多卡部署

``` python
import pyfastllm as llm
# 支持以下方式，需要在模型创建之前调用
llm.set_device_map({"cuda:0" : 10, "cuda:1" : 5, "cpu": 1}) # 将模型按不同比例部署在多个设备上
```

### c++中使用多卡部署

``` cpp
// 支持以下方式，需要在模型创建之前调用
fastllm::SetDeviceMap({{"cuda:0", 10}, {"cuda:1", 5}, {"cpu", 1}}); // 将模型按不同比例部署在多个设备上
```

## Android上使用

### Docker 编译运行
docker 运行需要本地安装好 NVIDIA Runtime,且修改默认 runtime 为 nvidia

1. 安装 nvidia-container-runtime
```
sudo apt-get install nvidia-container-runtime
```

2. 修改 docker 默认 runtime 为 nvidia

/etc/docker/daemon.json
```
{
  "registry-mirrors": [
    "https://hub-mirror.c.163.com",
    "https://mirror.baidubce.com"
  ],
  "runtimes": {
      "nvidia": {
          "path": "/usr/bin/nvidia-container-runtime",
          "runtimeArgs": []
      }
   },
   "default-runtime": "nvidia" // 有这一行即可
}

```

3. 下载已经转好的模型到 models 目录下
```
models
  chatglm2-6b-fp16.flm
  chatglm2-6b-int8.flm
```

4. 编译并启动 webui
```
DOCKER_BUILDKIT=0 docker compose up -d --build
```

### 编译
``` sh
# 在PC上编译需要下载NDK工具
# 还可以尝试使用手机端编译，在termux中可以使用cmake和gcc（不需要使用NDK）
mkdir build-android
cd build-android
export NDK=<your_ndk_directory>
# 如果手机不支持，那么去掉 "-DCMAKE_CXX_FLAGS=-march=armv8.2a+dotprod" （比较新的手机都是支持的）
cmake -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-23 -DCMAKE_CXX_FLAGS=-march=armv8.2a+dotprod ..
make -j
```

### 运行

1. 在Android设备上安装termux软件
2. 在termux中执行termux-setup-storage获得读取手机文件的权限。
3. 将NDK编译出的main文件，以及模型文件存入手机，并拷贝到termux的根目录
4. 使用命令```chmod 777 main```赋权
5. 然后可以运行main文件，参数格式参见```./main --help```

## 模型获取

### 模型库

可以在以下链接中下载已经转换好的模型

[huggingface](https://huggingface.co/huangyuyang) 

### 模型导出

#### ChatGLM模型导出 (默认脚本导出ChatGLM2-6b模型)

``` sh
# 需要先安装ChatGLM-6B环境
# 如果使用自己finetune的模型需要修改chatglm_export.py文件中创建tokenizer, model的代码
cd build
python3 tools/chatglm_export.py chatglm2-6b-fp16.flm float16 #导出float16模型
python3 tools/chatglm_export.py chatglm2-6b-int8.flm int8 #导出int8模型
python3 tools/chatglm_export.py chatglm2-6b-int4.flm int4 #导出int4模型
```

### baichuan模型导出 (默认脚本导出baichuan-13b-chat模型)

``` sh
# 需要先安装baichuan环境
# 如果使用自己finetune的模型需要修改baichuan2flm.py文件中创建tokenizer, model的代码
# 根据所需的精度，导出相应的模型
cd build
python3 tools/baichuan2flm.py baichuan-13b-fp16.flm float16 #导出float16模型
python3 tools/baichuan2flm.py baichuan-13b-int8.flm int8 #导出int8模型
python3 tools/baichuan2flm.py baichuan-13b-int4.flm int4 #导出int4模型
```

### baichuan2模型导出 (默认脚本导出baichuan2-7b-chat模型)

``` sh
# 需要先安装baichuan2环境
# 如果使用自己finetune的模型需要修改baichuan2_2flm.py文件中创建tokenizer, model的代码
# 根据所需的精度，导出相应的模型
cd build
python3 tools/baichuan2_2flm.py baichuan2-7b-fp16.flm float16 #导出float16模型
python3 tools/baichuan2_2flm.py baichuan2-7b-int8.flm int8 #导出int8模型
python3 tools/baichuan2_2flm.py baichuan2-7b-int4.flm int4 #导出int4模型
```

### MOSS模型导出

``` sh
# 需要先安装MOSS环境
# 如果使用自己finetune的模型需要修改moss_export.py文件中创建tokenizer, model的代码
# 根据所需的精度，导出相应的模型
cd build
python3 tools/moss_export.py moss-fp16.flm float16 #导出float16模型
python3 tools/moss_export.py moss-int8.flm int8 #导出int8模型
python3 tools/moss_export.py moss-int4.flm int4 #导出int4模型
```

### LLAMA系列模型导出
``` sh
# 修改build/tools/alpaca2flm.py程序进行导出
# 不同llama模型使用的指令相差很大，需要参照torch2flm.py中的参数进行配置
```

### QWEN模型导出
```sh
# 需要先安装QWen环境
# 如果使用自己finetune的模型需要修改qwen2flm.py文件中创建tokenizer, model的代码
# 根据所需的精度，导出相应的模型
python3 tools/qwen2flm.py qwen-7b-fp16.flm float16 #导出float16模型
python3 tools/qwen2flm.py qwen-7b-int8.flm int8 #导出int8模型
python3 tools/qwen2flm.py qwen-7b-int4.flm int4 #导出int4模型
```

## 开发计划

也就是俗称的画饼部分，大家如果有需要的功能可以在讨论区提出

### 短期计划

- 添加MMLU, CMMLU等测试程序
- 支持直接转换已经量化好的huggingface模型
- 实现外推到8K长度

### 中期计划

- 支持更多后端，如opencl, vulkan, 以及一些NPU加速设备
- 支持、验证更多模型，完善模型库
- 优化tokenizer (由于目前在python中可以直接使用原模型的tokenizer来分词，所以这项工作暂时并不急迫)

### 长期计划

- 支持ONNX模型导入、推理
- 支持模型微调
