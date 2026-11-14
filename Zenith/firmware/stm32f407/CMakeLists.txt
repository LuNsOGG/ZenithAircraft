# Set the minimum version of CMake that can be used
cmake_minimum_required(VERSION 3.20)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)
set(CMAKE_C_COMPILER_FORCED TRUE)

# Set target definition variables required by the toolchain file
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/target.cmake)

# Set the toolchain file for cross-compilation
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/gcc-arm-none-eabi.cmake)

# Project definition
project(zenith_aircraft LANGUAGES C ASM)

# Firmware in C; the device startup file uses assembly.
enable_language(C ASM)

set(PROJECT_NAME zenith_aircraft)

# Create an executable object type
add_executable(${PROJECT_NAME})

# Include additional CMake files for flags, components, and files
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/flags.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/components.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/files.cmake)

# Add sources to executable/library
target_sources(${PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/application.c
  ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/bmp388.c
  ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/barometric_altitude.c
  ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/serial_output.c
  ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/tinyusb/src/tusb.c
  ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/tinyusb/src/common/tusb_fifo.c
  ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/tinyusb/src/device/usbd.c
  ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/tinyusb/src/device/usbd_control.c
  ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/tinyusb/src/class/cdc/cdc_device.c
  ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/tinyusb/src/portable/synopsys/dwc2/dcd_dwc2.c
  ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/tinyusb/src/portable/synopsys/dwc2/dwc2_common.c
)

# Add include directories
target_include_directories(${PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/Core/Inc
  ${CMAKE_CURRENT_SOURCE_DIR}/usb
)
target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/tinyusb/src
  ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/cmsis_device_f4/Include
  ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/cmsis_core/CMSIS/Core/Include
)

# Compiler options
target_compile_options(${PROJECT_NAME} PRIVATE
  # User defined compiler options
)

# Add additional defined symbols
target_compile_definitions(${PROJECT_NAME} PRIVATE
  STM32F407xx HSE_VALUE=8000000U
  # Add additional defined symbols here
)

# Linker options
target_link_options(${PROJECT_NAME} PRIVATE 
  # User defined linker options
)

# Add additional linker search directories
target_link_directories(${PROJECT_NAME} PRIVATE
  # Add additional linker search directories here
)

# Add additional linker libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
  m # pow/sqrt for the standard-atmosphere model and reference statistics.
  # Add additional linker libraries here
)

# Firmware images for later programming; building never flashes the board.
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
  COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:${PROJECT_NAME}> ${PROJECT_NAME}.hex
  COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${PROJECT_NAME}> ${PROJECT_NAME}.bin
)
