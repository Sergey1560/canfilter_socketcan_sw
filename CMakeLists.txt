cmake_minimum_required(VERSION 3.22)

set(CMAKE_PROJECT_NAME canfilet_socketcan)

# Setup compiler settings
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

set(CMAKE_BUILD_PARALLEL_LEVEL 4)

set(CMAKE_BUILD_TYPE "Release")

set(CMAKE_VERBOSE_MAKEFILE OFF)

option(SYSTEMVIEW_ENABLED "Enable SystemView" OFF)

# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

set(GCC_TC_PATH   "/home/sergey/soft/arm-none-eabi")
set(SREC_OBJCOPY  ${GCC_TC_PATH}/bin/arm-none-eabi-objcopy)

# Include toolchain file
include("cmake/gcc-arm-none-eabi.cmake")

include("cmake/pvs-studio-cmake-module/PVS-Studio.cmake")

project(${CMAKE_PROJECT_NAME} C ASM)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

set(CMAKE_C_FLAGS_DEBUG "" CACHE STRING "" FORCE)
set(CMAKE_C_FLAGS_RELEASE "" CACHE STRING "" FORCE)

# Core project settings
message("Build type: " ${CMAKE_BUILD_TYPE})

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/segger)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PUBLIC
Startup/startup_stm32g474xx.s
Lib/CMSIS/system_stm32g4xx.c
Lib/Mylib/can.c
Lib/Mylib/dwt.c
Lib/Mylib/newlib_stub.c
Lib/Mylib/rcc.c
Lib/Mylib/ws2812b.c
Src/led.c
Src/common_data.c
Src/main.c
# Add user sources here
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PUBLIC
Lib/CMSIS
Lib/Mylib
Src
# Add user defined include paths
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PUBLIC
STM32G474xx
# Add user defined symbols
)

if(SYSTEMVIEW_ENABLED)
    message("SystemView enabled")
    target_compile_definitions(${CMAKE_PROJECT_NAME} PUBLIC
    SEGGER_SYSVIEW
    # Add user defined symbols
    )
else()
    message("SystemView disabled")
endif(SYSTEMVIEW_ENABLED)
unset(SYSTEMVIEW_ENABLED CACHE) # <---- this is the important!!


# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
#    stm32cubemx
    segger
    #freertos
    # Add user defined libraries
)

add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary -I elf32-little --only-section=.isr_vector --only-section=.text --only-section=.rodata --only-section=.data --only-section=.init_array --only-section=.fini_array --only-section=.ARM ${CMAKE_PROJECT_NAME}.elf ${CMAKE_PROJECT_NAME}.bin
    COMMAND ${SREC_OBJCOPY}  --output-target srec -I elf32-little --only-section=.isr_vector --only-section=.text --only-section=.rodata --only-section=.data --only-section=.init_array --only-section=.fini_array --only-section=.ARM ${CMAKE_PROJECT_NAME}.elf ${CMAKE_PROJECT_NAME}.srec
    VERBATIM
)

add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
    COMMAND ln -fs  ${CMAKE_PROJECT_NAME}.elf firmware.elf
)

set_property(
        TARGET ${CMAKE_PROJECT_NAME}
        APPEND
        PROPERTY ADDITIONAL_CLEAN_FILES ${CMAKE_PROJECT_NAME}.map ${CMAKE_PROJECT_NAME}.bin firmware.elf
)

# cmake .. -DPVS_ENABLED=1
if(PVS_ENABLED)
    message("PVS enabled")
    pvs_studio_add_target(TARGET ${CMAKE_PROJECT_NAME}.analyze ALL
                        OUTPUT FORMAT json
                        ANALYZE ${CMAKE_PROJECT_NAME}
                        MODE GA:1,2
                        LOG pvs_report.json
                        )
else()
    message("PVS disabled")
endif(PVS_ENABLED)
