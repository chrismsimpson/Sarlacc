cmake_minimum_required(VERSION 3.30)
set(CMAKE_BUILD_TYPE Debug)
project(libSarlacc)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(Sarlacc SHARED 
    Error.cpp
    Parsing.cpp
    Path.cpp
)

target_link_libraries(Sarlacc Metal)

set_target_properties(Sarlacc PROPERTIES LINKER_LANGUAGE CXX)
