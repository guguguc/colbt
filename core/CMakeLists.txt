cmake_minimum_required(VERSION 3.16)
project(imcore CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(OpenSSL REQUIRED)

add_library(imcore STATIC
    src/net/socket.cpp
    src/protocol/codec.cpp
    src/client/clientcore.cpp
    src/server/storage.cpp
    src/server/session.cpp
    src/server/servercore.cpp
)

target_include_directories(imcore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(imcore PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(imcore PUBLIC Threads::Threads SQLite::SQLite3 OpenSSL::Crypto)

# 警告
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(imcore PRIVATE -Wall -Wextra)
endif()
