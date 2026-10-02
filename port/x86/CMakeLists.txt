cmake_minimum_required(VERSION 3.13)
project(seedless_port_x86 C)

set(CMAKE_C_STANDARD 11)

add_executable(seedless_port_x86
    main.c
    platform_posix.c
    api_stub.c
)

target_include_directories(seedless_port_x86 PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../../include)
target_link_libraries(seedless_port_x86 PRIVATE pthread)
target_compile_definitions(seedless_port_x86 PRIVATE _GNU_SOURCE)
target_compile_options(seedless_port_x86 PRIVATE -Wall -Wextra -Werror=incompatible-pointer-types)
