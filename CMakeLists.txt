cmake_minimum_required(VERSION 2.8)

project(contention_prof)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -fPIC -std=c++11")

include_directories(
    src
)

file (GLOB_RECURSE SRC
    src/*.cpp
)

add_library(contention_prof SHARED ${SRC})
target_link_libraries(contention_prof
    pthread
    dl
    gflags
)

add_executable(sample examples/sample/main.cpp)
target_link_libraries(sample
    contention_prof
    pthread
)
