cmake_minimum_required(VERSION 3.16)
project(openssl_learning C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

find_package(OpenSSL 3 REQUIRED COMPONENTS Crypto SSL)

option(OPENSSL_FETCH_SOURCE
    "Fetch the pinned OpenSSL source tree for architecture exploration" OFF)
set(OPENSSL_SOURCE_OVERRIDE "" CACHE PATH
    "Use an existing OpenSSL source tree instead of downloading it")

if(OPENSSL_FETCH_SOURCE)
    include(cmake/OpenSSLSourceExplorer.cmake)
    openssl_enable_source_explorer()
endif()

enable_testing()
add_subdirectory(labs/01_hello)
add_subdirectory(labs/03_evp)
add_subdirectory(labs/04_keys)
add_subdirectory(labs/06_x509)
add_subdirectory(labs/07_tls)
add_subdirectory(labs/08_provider)
