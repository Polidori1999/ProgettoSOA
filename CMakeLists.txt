cmake_minimum_required(VERSION 3.20)

project(ProgettoSOA C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(syscall_throttle_ctl
        main.c
)

target_include_directories(syscall_throttle_ctl PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)
