cmake_minimum_required(VERSION 3.14)
project(tftpserver C)

set(CMAKE_C_STANDARD 99)

add_executable(tftpserver src/common/tftp.c src/common/tftp.h src/server/main.c)