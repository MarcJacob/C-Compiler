cmake_minimum_required(VERSION 3.20)
project(MyC_Compiler C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(MyC_Compiler
    src/main.c  "include/string_ansi.h" "include/core.h")

target_include_directories(MyC_Compiler PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
