cmake_minimum_required(VERSION 3.24)
project(green_pass C)

set(CMAKE_C_STANDARD 23)

add_executable(green_pass client.c green_pass.h centro_vaccinale.c)
