# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 1.5.1)
set(toolchainVersion 13_3_Rel1)
set(picotoolVersion 2.0.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico_w CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(guitar_tuner C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1


add_executable(guitar_tuner 
    guitar_tuner.c 
    inc/ssd1306_i2c.c
    kissfft/kiss_fft.c
    kissfft/kiss_fftr.c
    )

pico_set_program_name(guitar_tuner "guitar_tuner")
pico_set_program_version(guitar_tuner "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(guitar_tuner 0)
pico_enable_stdio_usb(guitar_tuner 1)

# Add the standard library to the build
# This will pull in the standard C and C++ libraries, as well as the
# Raspberry Pi Pico SDK
target_link_libraries(guitar_tuner
        pico_stdlib)


# Add the standard include files to the build
# This will pull in our own header files
target_include_directories(guitar_tuner PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}
)


# Add any user requested libraries
target_link_libraries(guitar_tuner 
        hardware_i2c
        hardware_adc
        hardware_dma
        hardware_pwm
        )

# Inclui diretórios adicionais
target_include_directories(guitar_tuner PRIVATE inc ${CMAKE_CURRENT_LIST_DIR}/kissfft)

pico_add_extra_outputs(guitar_tuner)
