cmake_minimum_required(VERSION 3.10)
project(CallTreeToJSONPlugin)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

# Qt 5.15
set(CMAKE_PREFIX_PATH "/usr/lib/x86_64-linux-gnu/cmake/Qt5")
find_package(Qt5 5.15 REQUIRED COMPONENTS Core Widgets)

# CubeGUI includes
set(CUBEGUI_INCLUDE_DIR
    /home/aashika/Documents/work/cubegui-install/include/cubegui
)

# CubeLib include + lib
set(CUBELIB_INCLUDE_DIR
    /home/aashika/Documents/work/cubelib-install/include/cubelib
)

set(CUBELIB_LIBRARY
    /home/aashika/Documents/work/cubelib-install/lib/libcube4.so.0.0.0
)

include_directories(
    ${CUBEGUI_INCLUDE_DIR}
    ${CUBELIB_INCLUDE_DIR}
)

add_library(CallTreeToJSONPlugin SHARED
    CallTreeToJSONPlugin.cpp
    CallTreeToJSONPlugin.h
)

target_link_libraries(CallTreeToJSONPlugin PRIVATE
    Qt5::Core
    Qt5::Widgets
    ${CUBELIB_LIBRARY}
)

set_target_properties(CallTreeToJSONPlugin PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY /home/aashika/Documents/work/cubegui-install/lib/cube-plugins
    RUNTIME_OUTPUT_DIRECTORY /home/aashika/Documents/work/cubegui-install/lib/cube-plugins
)
