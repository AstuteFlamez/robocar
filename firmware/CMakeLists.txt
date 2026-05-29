# robocar Pico 2W motor-controller firmware — Pico SDK / C
# Build: see firmware/README.md.  Requires PICO_SDK_PATH and -DPICO_BOARD=pico2_w (RP2350).
cmake_minimum_required(VERSION 3.13)

# Pull in the Pico SDK (expects pico_sdk_import.cmake copied from the SDK, or PICO_SDK_PATH set)
include(pico_sdk_import.cmake OPTIONAL RESULT_VARIABLE PICO_SDK_IMPORT)
if(NOT PICO_SDK_IMPORT)
    include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)
endif()

project(robocar_firmware C CXX ASM)
set(CMAKE_C_STANDARD 11)
pico_sdk_init()

add_executable(robocar_firmware
    src/main.c
    src/wheel_control.c
    src/mecanum_kinematics.c
    src/wheel_pid.c
    src/tb6612.c
    src/quadrature_encoder.c
    # incremental_pid.c is a REFERENCE module (not linked into the running firmware by default).
)

# quadrature_encoder.pio — copy from pico-examples (see README), then this generates its header:
pico_generate_pio_header(robocar_firmware ${CMAKE_CURRENT_LIST_DIR}/src/quadrature_encoder.pio)

target_link_libraries(robocar_firmware
    pico_stdlib
    hardware_pwm
    hardware_pio
    hardware_watchdog
    hardware_clocks   # clock_get_hz() in tb6612.c — link explicitly, don't rely on transitive
    hardware_adc      # for the optional battery/current-sense ADC borrow (no call yet)
)

# USB serial (the Pi<->Pico link); disable UART stdio so GP0/GP1 stay free.
pico_enable_stdio_usb(robocar_firmware 1)
pico_enable_stdio_uart(robocar_firmware 0)

pico_add_extra_outputs(robocar_firmware)   # -> .uf2 / .elf / .bin
