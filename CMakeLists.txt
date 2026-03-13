cmake_minimum_required(VERSION 3.28)
project(amp LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(miniaudio_headers INTERFACE)
target_include_directories(miniaudio_headers INTERFACE vendor)

add_subdirectory(lib)
add_subdirectory(demo)
