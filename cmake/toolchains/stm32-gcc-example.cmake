# 示例模板：按目标 STM32/工具链修改路径后使用。
# cmake -S . -B build-stm32 -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/stm32-gcc-example.cmake

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(CMAKE_C_COMPILER arm-none-eabi-gcc CACHE FILEPATH "ARM GCC")
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc CACHE FILEPATH "ARM assembler")
set(CMAKE_AR arm-none-eabi-ar CACHE FILEPATH "ARM archiver")
set(CMAKE_OBJCOPY arm-none-eabi-objcopy CACHE FILEPATH "ARM objcopy")

# 按 MCU 修改；示例仅表达 OpenSSL 集成时需要的编译边界。
set(MCU_FLAGS "-mcpu=cortex-m4;-mthumb;-ffunction-sections;-fdata-sections")
add_compile_options(${MCU_FLAGS})
add_link_options(${MCU_FLAGS} -Wl,--gc-sections)

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

