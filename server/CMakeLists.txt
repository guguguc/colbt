cmake_minimum_required(VERSION 3.16)
project(imserver CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT TARGET imcore)
    add_subdirectory(../core core)
endif()

add_executable(imserver src/main.cpp)
target_link_libraries(imserver PRIVATE imcore)
