cmake_minimum_required(VERSION 3.16)
project(openssl_learning C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

find_package(OpenSSL 3 REQUIRED COMPONENTS Crypto SSL)

enable_testing()
add_subdirectory(labs/01_hello)
add_subdirectory(labs/03_evp)

