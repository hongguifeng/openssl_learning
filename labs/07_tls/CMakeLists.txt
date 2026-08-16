add_executable(openssl_tls_loopback tls_loopback.c)
target_link_libraries(openssl_tls_loopback PRIVATE OpenSSL::SSL OpenSSL::Crypto)
target_compile_options(openssl_tls_loopback PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME tls_loopback COMMAND ${CMAKE_SOURCE_DIR}/scripts/run_tls_lab.sh $<TARGET_FILE:openssl_tls_loopback>)

