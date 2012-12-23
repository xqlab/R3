cmake_minimum_required(VERSION 2.8)

project(r3)

add_definitions(-DREB_API)
add_definitions(-DREB_CORE)
add_definitions(-DREB_EXE)

include_directories(${PROJECT_SOURCE_DIR}/src/include)
add_subdirectory(${PROJECT_SOURCE_DIR}/src/core)
add_subdirectory(${PROJECT_SOURCE_DIR}/src/os)
add_subdirectory(${PROJECT_SOURCE_DIR}/src/os/posix)

add_executable(r3 ${R3_SRCS})

# --- Linux ---

add_definitions(-DTO_LINUX)
add_definitions(-D_FILE_OFFSET_BITS=64)

set(CMAKE_C_FLAGS -m32)

find_library(LIBDL dl)
find_library(LIBM m)

target_link_libraries(r3 ${LIBDL} ${LIBM})
