cmake_minimum_required(VERSION 3.10)
project(BlabladorChatPlugin LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt5 REQUIRED COMPONENTS Core Widgets Network)

# CubeGUI & CubeLib paths
set(CUBEGUI_INCLUDE_DIR
    /home/aashika/Documents/work/cubegui-install/include/cubegui
)

set(CUBELIB_INCLUDE_DIR
    /home/aashika/Documents/work/cubelib-install/include/cubelib
)

add_library(BlabladorChatPlugin SHARED
    BlabladorChatPlugin.cpp
    BlabladorChatPlugin.h
)

target_include_directories(BlabladorChatPlugin PRIVATE
    ${CUBEGUI_INCLUDE_DIR}
    ${CUBELIB_INCLUDE_DIR}
)

target_link_libraries(BlabladorChatPlugin
    Qt5::Core
    Qt5::Widgets
    Qt5::Network
)

set_target_properties(BlabladorChatPlugin PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY
    /home/aashika/Documents/work/cubegui-install/lib/cube-plugins
)