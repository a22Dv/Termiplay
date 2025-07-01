cmake_minimum_required(VERSION 3.15)
project(termiplay C)

find_package(yaml CONFIG REQUIRED)
find_package(lz4 CONFIG REQUIRED)
set(SRC
    src/app.c
    src/main.c
    src/utils.c
    src/proc.c
)
set(TST
    tests/test.c
)
add_executable(termiplay ${SRC} ${TST})
target_include_directories(termiplay PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(termiplay PRIVATE yaml)
target_link_libraries(termiplay PRIVATE lz4::lz4)

