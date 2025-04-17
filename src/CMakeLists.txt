cmake_minimum_required(VERSION 3.12)

# Pull in SDK (must be before project)

# project(pico_lora_lib)

add_library(pico-lora STATIC
    lora.c
    lora.h
    print.c
    print.h
)

target_include_directories(pico-lora PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Link required Pico SDK libraries
target_link_libraries(pico-lora PRIVATE
    pico_stdlib
    hardware_spi
    hardware_gpio
    hardware_uart
)
