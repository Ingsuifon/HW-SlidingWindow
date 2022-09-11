cmake_minimum_required(VERSION 3.10)
project(Huawei-competition-2022)

include_directories(./fse)
include_directories(.)

add_subdirectory(fse)

aux_source_directory(. COMPRESS)
add_compile_options(
    -O3
    -g
)

add_executable(task ${COMPRESS})
target_link_libraries(task fse)