cmake_minimum_required(VERSION 3.24)
project(green_pass C)

set(CMAKE_C_STANDARD 23)

add_executable(green_pass green_pass.h client.c centro_vaccinale.c)