add_executable(openssl_key_demo key_demo.c)
target_link_libraries(openssl_key_demo PRIVATE OpenSSL::Crypto)
target_compile_options(openssl_key_demo PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME openssl_key_demo COMMAND openssl_key_demo)

