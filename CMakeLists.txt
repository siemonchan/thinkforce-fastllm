cmake_minimum_required(VERSION 3.5)

project(fastllm LANGUAGES CXX)

option(USE_CUDA "use cuda" OFF)

option(USE_TFACC "use tfacc" OFF)

option(PY_API "python api" OFF)
option(USE_MMAP "use mmap" OFF)

message(STATUS "USE_CUDA: ${USE_CUDA}")

message(STATUS "USE_TFACC: ${USE_TFACC}")

message(STATUS "PYTHON_API: ${PY_API}")

set(CMAKE_BUILD_TYPE "Release")

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread --std=c++17 -O2")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DNOMINMAX -O2 /std:c++17 /arch:AVX /source-charset:utf-8")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread --std=c++17 -O2 -march=native")
endif()

# add_compile_definitions(DEBUG) # uncomment this to record profile when inferencing

message(STATUS "CMAKE_CXX_FLAGS" ${CMAKE_CXX_FLAGS})
set(FASTLLM_CXX_SOURCES src/fastllm.cpp src/device.cpp src/model.cpp src/executor.cpp
        src/devices/cpu/cpudevice.cpp src/devices/cpu/cpudevicebatch.cpp
        src/models/chatglm.cpp src/models/moss.cpp src/models/llama.cpp src/models/qwen.cpp src/models/decicoder.cpp src/models/basellm.cpp)

include_directories(include)
include_directories(include/utils)
include_directories(include/models)

if (USE_MMAP)
    add_compile_definitions(USE_MMAP)
endif()

if (USE_CUDA)
    enable_language(CUDA)
    add_compile_definitions(USE_CUDA)
    include_directories(include/devices/cuda)
    #message(${CMAKE_CUDA_IMPLICIT_LINK_DIRECTORIES})
    set(FASTLLM_CUDA_SOURCES src/devices/cuda/cudadevice.cpp src/devices/cuda/cudadevicebatch.cpp src/devices/cuda/fastllm-cuda.cu)
    set(FASTLLM_LINKED_LIBS ${FASTLLM_LINKED_LIBS} cublas)
    set(CMAKE_CUDA_ARCHITECTURES "native")
endif()

if (USE_TFACC)
    add_definitions(-DARM)
    add_definitions(-DUSE_TFDL)
    add_definitions(-DUSE_TFACC40T)

    execute_process(
        COMMAND "./insmodTFDriver.sh"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/third_party/thinkforce"
    )

    set(FASTLLM_TFACC_SOURCES src/devices/tfacc/tfaccdevice.cpp src/devices/tfacc/fastllm-tfacc.cpp)

    include_directories(include/devices/tfacc)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/third_party/thinkforce/include/tfdl)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/third_party/thinkforce/include/tfacc40t)

    set(lib_tfacc ${CMAKE_CURRENT_SOURCE_DIR}/third_party/thinkforce/lib/lib_tfdl.so
                  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/thinkforce/lib/libtfnn.so
                  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/thinkforce/lib/libtfacc40t.so)
    set(FASTLLM_LINKED_LIBS ${FASTLLM_LINKED_LIBS} ${lib_tfacc} numa)
endif()

if (PY_API)
    set(PYBIND third_party/pybind11)
    add_subdirectory(${PYBIND})
    add_compile_definitions(PY_API)
    
    set(Python3_ROOT_DIR "/usr/bin/")
    find_package(Python3 REQUIRED)

    include_directories(third_party/pybind11/include)
    file(GLOB FASTLLM_CXX_HEADERS include/**/*.h)
    add_library(pyfastllm MODULE src/pybinding.cpp ${FASTLLM_CXX_SOURCES} ${FASTLLM_CXX_HEADERS} ${FASTLLM_CUDA_SOURCES} ${FASTLLM_TFACC_SOURCES})
    target_link_libraries(pyfastllm PUBLIC pybind11::module ${FASTLLM_LINKED_LIBS})
    pybind11_extension(pyfastllm)
else()
add_library(fastllm OBJECT
            ${FASTLLM_CXX_SOURCES}
            ${FASTLLM_CUDA_SOURCES}
            ${FASTLLM_TFACC_SOURCES}
            )
target_link_libraries(fastllm PUBLIC ${FASTLLM_LINKED_LIBS})

add_executable(main main.cpp)
target_link_libraries(main fastllm)

add_executable(quant tools/src/quant.cpp)
target_link_libraries(quant fastllm)

add_executable(webui example/webui/webui.cpp)
target_link_libraries(webui fastllm)
add_custom_command(
        TARGET webui
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory web
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/example/webui/web ${CMAKE_BINARY_DIR}/web
)

add_executable(benchmark example/benchmark/benchmark.cpp)
target_link_libraries(benchmark fastllm)

add_library(fastllm_tools SHARED ${FASTLLM_CXX_SOURCES} ${FASTLLM_CUDA_SOURCES} ${FASTLLM_TFACC_SOURCES} tools/src/pytools.cpp)
target_link_libraries(fastllm_tools PUBLIC ${FASTLLM_LINKED_LIBS})

if (${CMAKE_HOST_WIN32})
    add_custom_command(
            TARGET fastllm_tools
            POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory tools
            COMMAND ${CMAKE_COMMAND} -E make_directory tools/fastllm_pytools
            COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/tools/fastllm_pytools ${CMAKE_BINARY_DIR}/tools/fastllm_pytools/.
            COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/tools/scripts ${CMAKE_BINARY_DIR}/tools/.
            COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/fastllm_tools.dll ${CMAKE_BINARY_DIR}/tools/fastllm_pytools/.
            COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_BINARY_DIR}/fastllm_tools.dll
    )
else()
    add_custom_command(
            TARGET fastllm_tools
            POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory tools
            COMMAND ${CMAKE_COMMAND} -E make_directory tools/fastllm_pytools
            COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/tools/fastllm_pytools ${CMAKE_BINARY_DIR}/tools/fastllm_pytools/.
            COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/tools/scripts ${CMAKE_BINARY_DIR}/tools/.
            COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/libfastllm_tools.* ${CMAKE_BINARY_DIR}/tools/fastllm_pytools/.
            COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_BINARY_DIR}/libfastllm_tools.*
    )
endif()

endif()