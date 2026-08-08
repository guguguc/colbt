cmake_minimum_required(VERSION 3.16)
project(colbt CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 导出 compile_commands.json（供 clangd/LSP/编辑器使用）
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BUILD_SERVER "Build TCP server" ON)
option(BUILD_CLIENT "Build Qt client" ON)

if(BUILD_SERVER)
    add_subdirectory(server)
endif()

if(BUILD_CLIENT)
    add_subdirectory(client)
endif()
