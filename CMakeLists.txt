cmake_minimum_required(VERSION 3.28.1)
project(live C)

file(GLOB_RECURSE source_files "src/*.c")

add_executable(live ${source_files})

target_include_directories(live PRIVATE lib)