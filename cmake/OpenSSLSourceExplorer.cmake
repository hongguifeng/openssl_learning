include_guard(GLOBAL)
include(FetchContent)

# OpenSSL upstream uses Perl Configure rather than CMake. FetchContent owns
# download, hash verification and extraction; it does not add a subdirectory.
function(openssl_enable_source_explorer)
    set(_openssl_tag "openssl-3.0.2")
    set(_openssl_sha256
        "9f54d42aed56f62889e8384895c968e24d57eae701012776d5f18fb9f2ae48b0")

    if(OPENSSL_SOURCE_OVERRIDE)
        get_filename_component(_openssl_source
            "${OPENSSL_SOURCE_OVERRIDE}" ABSOLUTE BASE_DIR "${CMAKE_SOURCE_DIR}")
        message(STATUS "OpenSSL source explorer: using override ${_openssl_source}")
    else()
        FetchContent_Declare(openssl_source
            URL "https://github.com/openssl/openssl/archive/refs/tags/${_openssl_tag}.tar.gz"
            URL_HASH "SHA256=${_openssl_sha256}"
            TIMEOUT 180
        )
        FetchContent_GetProperties(openssl_source)
        if(NOT openssl_source_POPULATED)
            FetchContent_Populate(openssl_source)
        endif()
        set(_openssl_source "${openssl_source_SOURCE_DIR}")
    endif()

    foreach(_required
            VERSION.dat
            include/openssl/evp.h
            crypto/evp/evp_fetch.c
            crypto/provider_core.c
            providers/defltprov.c
            ssl/ssl_lib.c
            ssl/statem/statem.c)
        if(NOT EXISTS "${_openssl_source}/${_required}")
            message(FATAL_ERROR
                "OpenSSL source explorer: missing ${_required} under ${_openssl_source}")
        endif()
    endforeach()

    set(OPENSSL_EXPLORE_SOURCE_DIR "${_openssl_source}"
        CACHE INTERNAL "Fetched OpenSSL source tree")
    set(_index "${CMAKE_BINARY_DIR}/openssl-source-index.md")

    add_custom_target(openssl_source_index
        COMMAND "${CMAKE_SOURCE_DIR}/scripts/explore_openssl_source.sh"
                "${_openssl_source}" "${_index}"
        BYPRODUCTS "${_index}"
        COMMENT "Indexing OpenSSL modules and public-to-internal call paths"
        VERBATIM
    )

    add_custom_target(openssl_source_show
        COMMAND "${CMAKE_COMMAND}" -E echo
                "OpenSSL source: ${_openssl_source}"
        COMMAND "${CMAKE_COMMAND}" -E echo
                "Build the index target, then read: ${_index}"
        VERBATIM
    )

    add_custom_target(openssl_source_verify
        COMMAND "${CMAKE_SOURCE_DIR}/scripts/verify_openssl_source.sh"
                "${_openssl_source}"
        COMMENT "Verifying expected OpenSSL 3.0.2 module and symbol layout"
        VERBATIM
    )

    add_test(NAME openssl_source_structure
        COMMAND "${CMAKE_SOURCE_DIR}/scripts/verify_openssl_source.sh"
                "${_openssl_source}")

    message(STATUS "OpenSSL source explorer enabled: ${_openssl_source}")
    message(STATUS
        "Run: cmake --build ${CMAKE_BINARY_DIR} --target openssl_source_index")
endfunction()
