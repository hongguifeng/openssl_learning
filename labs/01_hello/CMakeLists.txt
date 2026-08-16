add_executable(openssl_hello hello.c)
target_link_libraries(openssl_hello PRIVATE OpenSSL::Crypto)
target_compile_options(openssl_hello PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME openssl_hello COMMAND openssl_hello)

