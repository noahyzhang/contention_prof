cmake_minimum_required(VERSION 2.8)

project(contention_prof)

file (GLOB_RECURSE SRC
    src/*.cpp
)

add_library(contention_prof SHARED ${SRC})

target_link_libraries(contention_prof
    pthread
)