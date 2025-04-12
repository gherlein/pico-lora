cmake_minimum_required(VERSION 3.12)

# Pull in SDK (must be before project)

# project(pico_lora_lib)

add_library(pico_lora_lib LoRa-RP2040.cpp LoRa-RP2040.h)
add_library(lora_print Print.h Print.cpp)

target_link_libraries(pico_lora_lib pico_stdlib hardware_spi hardware_interp lora_print)
