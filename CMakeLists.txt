cmake_minimum_required(VERSION 3.24)
project(green_pass C)

set(CMAKE_C_STANDARD 23)

add_executable(green_pass green_pass.h addresses.h client.c centro_vaccinale.c serverV.c clientS.c clientT.c serverG.c)