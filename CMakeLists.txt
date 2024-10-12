cmake_minimum_required(VERSION 3.30)
set(CMAKE_BUILD_TYPE Debug)
project(Sarlacc)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(lib/metal)
add_subdirectory(lib/sarlacc)
add_subdirectory(src)