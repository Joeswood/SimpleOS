# Project cmakelist file
cmake_minimum_required(VERSION 3.0.0)

set(CMAKE_VERBOSE_MAKEFILE on)   # open hint when complier

# gcc toollink prefix config
if (CMAKE_HOST_WIN32 OR CMAKE_HOST_APPLE)
    set(TOOL_PREFIX  "x86_64-elf-")
elseif (CMAKE_HOST_WIN32)
    set(TOOL_PREFIX  "x86_64-linux-gnu-")    
endif ()

# set(TOOL_PREFIX  "x86_64-elf-")
# set(TOOL_PREFIX  "i686-elf-")

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_VERSION "0.0.0")

# c complier config
set(CMAKE_C_COMPILER "${TOOL_PREFIX}gcc")
set(CMAKE_C_FLAGS "-g -c -O0 -m32 -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables")

# assemblier config
set(CMAKE_ASM_COMPILER "${TOOL_PREFIX}gcc")
set(CMAKE_ASM_FLAGS "-m32 -g")
set(CMAKE_ASM_SOURCE_FILE_EXTENSIONS "asm")

# liner tool
set(LINKER_TOOL "${TOOL_PREFIX}ld")

# tool for debug
set(OBJCOPY_TOOL "${TOOL_PREFIX}objcopy")
set(OBJDUMP_TOOL "${TOOL_PREFIX}objdump")
set(READELF_TOOL "${TOOL_PREFIX}readelf")

# enable C and assembly for project
project(os LANGUAGES C)  
enable_language(ASM)

# head file path
include_directories(
    ${PROJECT_SOURCE_DIR}/source
     ${PROJECT_SOURCE_DIR}/source/kernel/include
     ${PROJECT_SOURCE_DIR}/newlib/i686-elf/include
)

# subdirectory
add_subdirectory(./source/boot)
add_subdirectory(./source/loader)
add_subdirectory(./source/kernel)
add_subdirectory(./source/applib)
add_subdirectory(./source/shell)
add_subdirectory(./source/init)
add_subdirectory(./source/loop)
add_subdirectory(./source/snake)


# add add_dependencies, generate app lib then kernel and shell
add_dependencies(shell app)
add_dependencies(kernel app)
# add_dependencies(loop app)
# add_dependencies(kernel init)
