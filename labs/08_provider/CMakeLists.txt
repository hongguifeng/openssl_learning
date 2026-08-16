add_library(learn_provider MODULE learn_provider.c)
set_target_properties(learn_provider PROPERTIES PREFIX "")
target_compile_options(learn_provider PRIVATE -Wall -Wextra -Wpedantic)

add_executable(openssl_provider_demo provider_demo.c)
target_link_libraries(openssl_provider_demo PRIVATE OpenSSL::Crypto)
target_compile_options(openssl_provider_demo PRIVATE -Wall -Wextra -Wpedantic)
add_dependencies(openssl_provider_demo learn_provider)
add_test(NAME openssl_provider_demo COMMAND openssl_provider_demo)
set_tests_properties(openssl_provider_demo PROPERTIES
    ENVIRONMENT "OPENSSL_MODULES=$<TARGET_FILE_DIR:learn_provider>")

