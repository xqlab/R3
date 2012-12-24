cmake_minimum_required(VERSION 2.8)

project(r3)

# -- Target platform detection

if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
	set(R3_OSX on)
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
	set(R3_WIN32 on)
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
	set(R3_LINUX on)
endif()

# -- Common R3 build configuration

add_definitions(-DREB_API)
add_definitions(-DREB_CORE)
add_definitions(-DREB_EXE)

include_directories(${PROJECT_SOURCE_DIR}/src/include)

add_subdirectory(${PROJECT_SOURCE_DIR}/src/core)
add_subdirectory(${PROJECT_SOURCE_DIR}/src/os)

# -- Platform-specific configuration

if(R3_OSX)
	# Host sources
	add_subdirectory(${PROJECT_SOURCE_DIR}/src/os/posix)
	# Definitions
	add_definitions(-DTO_OSXI)
	add_definitions(-D_FILE_OFFSET_BITS=64)
	# Compiler/linker flags
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -arch i386")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-pointer-sign")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fvisibility=hidden")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-common")
	# Libraries
	find_library(LIBM m)
	set(R3_LIBS ${LIBM})
endif()

if(R3_WIN32)
	# Host sources
	add_subdirectory(${PROJECT_SOURCE_DIR}/src/os/win32)
	# Definitions
	add_definitions(-DTO_WIN32)
	add_definitions(-DUNICODE)
	add_definitions(-D_FILE_OFFSET_BITS=64)
	# Compiler/linker flags
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2")
	set(CMAKE_EXE_LINKER_FLAGS "-Wl,--stack=4194300")
	# Libraries
	find_library(LIBM m)
	find_library(LIBCOMDLG32 comdlg32)
	find_library(LIBWSOCK32 wsock32)
	set(R3_LIBS ${LIBM} ${LIBCOMDLG32} ${LIBWSOCK32})
endif()

if(R3_LINUX)
	# Host sources
	add_subdirectory(${PROJECT_SOURCE_DIR}/src/os/posix)
	# Definitions
	add_definitions(-DTO_LINUX)
	add_definitions(-D_FILE_OFFSET_BITS=64)
	# Compiler/linker flags
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m32")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fvisibility=hidden")
	# Libraries
	find_library(LIBDL dl)
	find_library(LIBM m)
	set(R3_LIBS ${LIBDL} ${LIBM})
endif()

# -- Build targets

add_executable(r3 ${R3_SRCS})
target_link_libraries(r3 ${R3_LIBS})
