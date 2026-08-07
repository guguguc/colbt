cmake_minimum_required(VERSION 3.16)
project(imcore CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 复用仓库 core/ 下的客户端子集（socket + codec + clientcore，纯 POSIX，无 Qt/sqlite/OpenSSL）
if(NOT DEFINED IM_CORE)
    get_filename_component(IM_CORE "${CMAKE_CURRENT_SOURCE_DIR}/../core" ABSOLUTE)
endif()

add_library(imcore STATIC
    ${IM_CORE}/src/net/socket.cpp
    ${IM_CORE}/src/protocol/codec.cpp
    ${IM_CORE}/src/client/clientcore.cpp
)

target_include_directories(imcore PUBLIC ${IM_CORE}/include)
target_include_directories(imcore PRIVATE ${IM_CORE}/src)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(imcore PRIVATE -Wall -Wextra)
endif()
