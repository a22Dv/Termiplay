cmake_minimum_required(VERSION 3.15)
project(termiplay C)

find_package(lz4 CONFIG REQUIRED)
set(SRC
    src/main.c
    src/app.c
    src/utils.c
    src/video.c
    src/audio.c
)
add_executable(termiplay ${SRC})
target_compile_options(termiplay PRIVATE /W4 /WX 
    /wd4100   # Disable warning: unreferenced formal parameter
    /wd4101   # Disable warning: unreferenced local variable
    /wd4189   # Disable warning: local variable is initialized but not referenced
    /wd4702   # Disable warning: unreachable code.
)
target_include_directories(termiplay PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(termiplay PRIVATE shell32)
target_link_libraries(termiplay PRIVATE lz4::lz4)
target_link_libraries(termiplay PRIVATE Pathcch)

