add_executable(openssl_evp_demo evp_demo.c)
target_link_libraries(openssl_evp_demo PRIVATE OpenSSL::Crypto)
target_compile_options(openssl_evp_demo PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME openssl_evp_demo COMMAND openssl_evp_demo)

