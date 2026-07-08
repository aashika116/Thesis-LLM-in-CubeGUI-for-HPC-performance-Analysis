cmake_minimum_required(VERSION 3.10)
project(ParseToJSONPlugin)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

# --- Qt 5.15.13 system install ---
set(CMAKE_PREFIX_PATH "/usr/lib/x86_64-linux-gnu/cmake/Qt5")
find_package(Qt5 5.15.0 REQUIRED COMPONENTS Core Widgets)

# --- CubeGUI include directory ---
set(CUBEGUI_INCLUDE_DIR
    /home/aashika/Documents/work/cubegui-install/include/cubegui
)

# --- CubeLib 4.9 include + library ---
set(CUBELIB_INCLUDE_DIR
    /home/aashika/Documents/work/cubelib-install/include/cubelib
)

set(CUBELIB_LIBRARY
    /home/aashika/Documents/work/cubelib-install/lib/libcube4.so.0.0.0
)

# Include all required directories
include_directories(
    ${CUBEGUI_INCLUDE_DIR}
    ${CUBELIB_INCLUDE_DIR}
)

add_library(ParseToJSONPlugin SHARED
    ParseToJSONPlugin.cpp
    ParseToJSONPlugin.h
)

target_link_libraries(ParseToJSONPlugin PRIVATE
    Qt5::Core
    Qt5::Widgets
    ${CUBELIB_LIBRARY}
)

# Output plugin as a .so file CubeGUI can load
# set_target_properties(ParseToJSONPlugin PROPERTIES
#    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
#)

set_target_properties(ParseToJSONPlugin PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY /home/aashika/Documents/work/cubegui-install/lib/cube-plugins
    RUNTIME_OUTPUT_DIRECTORY /home/aashika/Documents/work/cubegui-install/lib/cube-plugins
)