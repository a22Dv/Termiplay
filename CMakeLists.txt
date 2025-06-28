cmake_minimum_required(VERSION 3.15)
project(termiplay C)

find_package(yaml CONFIG REQUIRED)
find_package(lz4 CONFIG REQUIRED)

add_executable(termiplay src/main.c)
target_include_directories(termiplay PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")

