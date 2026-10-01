set(CMAKE_SYSTEM_NAME               Generic)
set(CMAKE_SYSTEM_PROCESSOR          arm)

set(CMAKE_C_COMPILER_FORCED TRUE)
set(CMAKE_CXX_COMPILER_FORCED TRUE)
set(CMAKE_C_COMPILER_ID GNU)
set(CMAKE_CXX_COMPILER_ID GNU)

# Some default GCC settings
# arm-none-eabi- must be part of path environment
#set(TOOLCHAIN_PREFIX                clang-18)

set(CMAKE_C_COMPILER                clang-18)
set(CMAKE_ASM_COMPILER              clang-18)
set(CMAKE_CXX_COMPILER              clang-18)
set(CMAKE_LINKER                    ld.lld-18)
set(CMAKE_OBJCOPY                   llvm-objcopy-18)
set(CMAKE_SIZE                      llvm-size-18)

set(CMAKE_EXECUTABLE_SUFFIX_ASM     ".elf")
set(CMAKE_EXECUTABLE_SUFFIX_C       ".elf")
set(CMAKE_EXECUTABLE_SUFFIX_CXX     ".elf")

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# MCU specific flags
set(TARGET_FLAGS "--target=armv7m-none-none-eabi -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${TARGET_FLAGS}")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}  -Wnewline-eof -fdata-sections -ffunction-sections")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -I/usr/lib/llvm-18/lib/clang/18/include/")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -I/usr/include/newlib/")

#-Wall -Wextra -Wpedantic

#-Wextra -Wpedantic

if(CMAKE_BUILD_TYPE MATCHES Debug)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0 -g3")
endif()
if(CMAKE_BUILD_TYPE MATCHES Release)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Oz -g3")
endif()

set(CMAKE_ASM_FLAGS "${CMAKE_C_FLAGS} -x assembler-with-cpp -MP")

set(CMAKE_C_LINK_FLAGS "${TARGET_FLAGS}")
set(CMAKE_C_LINK_FLAGS "${CMAKE_C_LINK_FLAGS} -T \"${CMAKE_SOURCE_DIR}/Startup/STM32G431CBTx_FLASH.ld\"")
#set(CMAKE_C_LINK_FLAGS "${CMAKE_C_LINK_FLAGS} --specs=nano.specs")
set(CMAKE_C_LINK_FLAGS "${CMAKE_C_LINK_FLAGS} -lc -lm")
set(CMAKE_C_LINK_FLAGS "${CMAKE_C_LINK_FLAGS} -Wl,-Map=${CMAKE_PROJECT_NAME}.map -Wl,--gc-sections")
set(CMAKE_C_LINK_FLAGS "${CMAKE_C_LINK_FLAGS} -Wl,--print-memory-usage")
set(CMAKE_C_LINK_FLAGS "${CMAKE_C_LINK_FLAGS} -L /home/sergey/soft/arm-none-eabi/lib/gcc/arm-none-eabi/14.2.1/thumb/v7-m/nofp")
set(CMAKE_C_LINK_FLAGS "${CMAKE_C_LINK_FLAGS} -L /home/sergey/soft/arm-none-eabi/arm-none-eabi/lib/thumb/v7-m/nofp")

SET(CMAKE_C_LINK_EXECUTABLE "<CMAKE_C_COMPILER> <OBJECTS> <CMAKE_C_LINK_FLAGS> -o <TARGET>")


