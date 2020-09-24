cmake_minimum_required(VERSION 3.13)
project(tftpserver C)

set(CMAKE_C_STANDARD 99)

find_package(Threads REQUIRED)

add_executable(tftpserver src/common/tftp.c src/common/tftp.h src/server/main.c)
target_link_libraries(tftpserver pthread)

project(tftpserver-tests C)

add_executable(tftpserver-tests src/common/tftp.c src/common/tftp.h src/server/tests.c)
