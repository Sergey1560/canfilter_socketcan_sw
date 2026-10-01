cmake_minimum_required(VERSION 3.22)

project(segger)
add_library(segger INTERFACE)

# Enable CMake support for ASM and C languages
enable_language(C ASM)

target_compile_definitions(segger INTERFACE
)

target_include_directories(segger INTERFACE
../../Lib/Segger
)


target_sources(segger INTERFACE
../../Lib/Segger/SEGGER_RTT_printf.c
../../Lib/Segger/SEGGER_RTT_Syscalls_GCC.c
../../Lib/Segger/SEGGER_RTT.c
../../Lib/Segger/SEGGER_RTT_ASM_ARMv7M.s
)

if(SYSTEMVIEW_ENABLED)
    message("Segger SystemView enabled")
    target_sources(segger INTERFACE
#    ../../Lib/Segger/SEGGER_SYSVIEW_Config_FreeRTOS.c
#    ../../Lib/Segger/SEGGER_SYSVIEW_FreeRTOS.c
    ../../Lib/Segger/SEGGER_SYSVIEW.c
    ../../Lib/Segger/SEGGER_SYSVIEW_Config_NoOS.c
    )
    else()
    message("Segger SystemView disabled")
endif(SYSTEMVIEW_ENABLED)

target_link_directories(segger INTERFACE
)

target_link_libraries(segger INTERFACE
)

# Validate that segger code is compatible with C standard
if(CMAKE_C_STANDARD LESS 11)
    message(ERROR "Generated code requires C11 or higher")
endif()


