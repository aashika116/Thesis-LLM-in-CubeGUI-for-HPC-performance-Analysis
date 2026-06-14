cmake_minimum_required(VERSION 3.10)
project(CubePlugins)

# Global settings
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

# Add plugin subdirectories
add_subdirectory(src/AIPlugin)
add_subdirectory(src/CallTreeToJSON)
add_subdirectory(src/SystemTreeToJSON)