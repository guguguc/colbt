cmake_minimum_required(VERSION 3.16)
project(imclient CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

if(NOT TARGET imcore)
    add_subdirectory(../core core)
endif()

add_executable(imclient
    src/main.cpp
    src/app/appcontext.cpp
    src/ui/avatar.cpp
    src/ui/loginwindow.cpp
    src/ui/mainwindow.cpp
    src/ui/sessionlistwidget.cpp
    src/ui/contactlistwidget.cpp
    src/ui/chatpanel.cpp
    src/ui/bubblewidget.cpp
    resources.qrc
)

target_include_directories(imclient PRIVATE src)

target_link_libraries(imclient PRIVATE imcore Qt6::Widgets)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(imclient PRIVATE -Wall -Wextra)
endif()
