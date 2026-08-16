add_executable(openssl_x509_verify x509_verify.c)
target_link_libraries(openssl_x509_verify PRIVATE OpenSSL::Crypto)
target_compile_options(openssl_x509_verify PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME x509_verify COMMAND ${CMAKE_SOURCE_DIR}/scripts/run_x509_lab.sh $<TARGET_FILE:openssl_x509_verify>)

