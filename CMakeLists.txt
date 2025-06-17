cmake_minimum_required(VERSION 3.15)
project(termiplay C)
# find_package(FFMPEG REQUIRED)

set(SOURCE_FILES
    src/main.c
)
add_compile_options($<$<C_COMPILER_ID:MSVC>:/utf-8>)
add_executable(termiplay ${SOURCE_FILES})
target_include_directories(termiplay PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
 add_library(Shlwapi::Shlwapi INTERFACE IMPORTED)
set_property(TARGET Shlwapi::Shlwapi PROPERTY INTERFACE_LINK_LIBRARIES shlwapi)
add_library(Pathcch::Pathcch INTERFACE IMPORTED)
set_property(TARGET Pathcch::Pathcch PROPERTY INTERFACE_LINK_LIBRARIES pathcch)
target_link_libraries(termiplay PRIVATE Pathcch::Pathcch Shlwapi::Shlwapi)

# FFmpeg stuff.
# target_include_directories(termiplay PRIVATE ${FFMPEG_INCLUDE_DIRS})
# target_link_directories(termiplay PRIVATE ${FFMPEG_LIBRARY_DIRS})
# target_link_libraries(termiplay PRIVATE ${FFMPEG_LIBRARIES})

