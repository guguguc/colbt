cmake_minimum_required(VERSION 3.22.1)
project(imclient_native CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 定位 core 目录（Android 工程的 cpp 目录上两级 -> repo 根/core）
get_filename_component(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../.." ABSOLUTE)
set(IM_CORE "${REPO_ROOT}/core")

add_library(imclient SHARED
    ${CMAKE_CURRENT_SOURCE_DIR}/jni_imclient.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/jni_bridge.cpp
    ${IM_CORE}/src/net/socket.cpp
    ${IM_CORE}/src/protocol/codec.cpp
    ${IM_CORE}/src/client/clientcore.cpp
)

target_include_directories(imclient PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${IM_CORE}/include
    ${IM_CORE}/src
)

find_library(log-lib log)
target_link_libraries(imclient PRIVATE ${log-lib})
