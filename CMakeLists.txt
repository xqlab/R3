cmake_minimum_required(VERSION 2.8)

project(rebol3)

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

# Rebol 3 command used for bootstrap (in the "prepare" target).
find_program(R3_MAKE r3-make)
set(R3_MAKE_CMD ${R3_MAKE} -qs)

# -- Platform-specific configuration

if(R3_OSX)
	set(R3_TARGET "2.5")
	# Host sources
	set(R3_HOST_SUBDIR "posix")
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
	set(R3_TARGET "3.1")
	# Host sources
	set(R3_HOST_SUBDIR "win32")
	# Definitions
	add_definitions(-DTO_WIN32)
	add_definitions(-DUNICODE)
	add_definitions(-D_FILE_OFFSET_BITS=64)
	if(NOT MSVC) # MinGW-specific settings (@@ Use more specific test.)
		# Compiler/linker flags
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2")
		set(CMAKE_EXE_LINKER_FLAGS "-Wl,--stack=4194300")
		# Libraries
		find_library(LIBCOMDLG32 comdlg32)
		find_library(LIBWSOCK32 wsock32)
		set(R3_LIBS ${LIBCOMDLG32} ${LIBWSOCK32})
	elseif(MSVC) # MSVC-specific settings
		# Definitions
		add_definitions(-D_CRT_SECURE_NO_WARNINGS)
		# Compiler/linker flags
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /subsystem:windows")
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /stack:4194300")
		# LIBRARIES
		set(R3_LIBS comdlg32.lib wsock32.lib)
	endif()
endif()

if(R3_LINUX)
	set(R3_TARGET "4.4")
	# Host sources
	set(R3_HOST_SUBDIR "posix")
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

add_subdirectory(${PROJECT_SOURCE_DIR}/src/os/${R3_HOST_SUBDIR})

# -- Generated files

set(R3_SRC_TOOLS_DIR ${PROJECT_SOURCE_DIR}/src/tools)

file(GLOB R3_SRCS_CORE "${PROJECT_SOURCE_DIR}/src/core/*.c")

file(GLOB R3_SRCS_MEZZ_BOOT # Basically mirroring src/mezz/boot-files.r
	"${PROJECT_SOURCE_DIR}/src/mezz/base-*.r"
	"${PROJECT_SOURCE_DIR}/src/mezz/sys-*.r"
	"${PROJECT_SOURCE_DIR}/src/mezz/mezz-*.r"
	"${PROJECT_SOURCE_DIR}/src/mezz/prot-*.r"
	)

add_custom_command(
	COMMAND ${R3_MAKE_CMD} ${R3_SRC_TOOLS_DIR}/make-boot.r 0.${R3_TARGET}
	OUTPUT
		${PROJECT_SOURCE_DIR}/src/include/tmp-evaltypes.h
		${PROJECT_SOURCE_DIR}/src/include/tmp-maketypes.h
		${PROJECT_SOURCE_DIR}/src/include/tmp-comptypes.h
		${PROJECT_SOURCE_DIR}/src/include/reb-types.h
		${PROJECT_SOURCE_DIR}/src/include/ext-types.h
		${PROJECT_SOURCE_DIR}/src/include/tmp-exttypes.h
		${PROJECT_SOURCE_DIR}/src/include/tmp-bootdefs.h
		${PROJECT_SOURCE_DIR}/src/include/tmp-sysobj.h
		${PROJECT_SOURCE_DIR}/src/include/reb-dialect.h
		${PROJECT_SOURCE_DIR}/src/include/reb-evtypes.h
		${PROJECT_SOURCE_DIR}/src/include/tmp-errnums.h
		${PROJECT_SOURCE_DIR}/src/include/tmp-portmodes.h
		${PROJECT_SOURCE_DIR}/src/include/tmp-sysctx.h
		${PROJECT_SOURCE_DIR}/src/boot/boot-code.r
		${PROJECT_SOURCE_DIR}/src/core/b-boot.c
		${PROJECT_SOURCE_DIR}/src/include/tmp-boot.h
	DEPENDS
		${PROJECT_SOURCE_DIR}/src/tools/form-header.r
		${PROJECT_SOURCE_DIR}/src/tools/systems.r
		${PROJECT_SOURCE_DIR}/src/boot/version.r
		${PROJECT_SOURCE_DIR}/src/boot/types.r
		${PROJECT_SOURCE_DIR}/src/boot/types-ext.r
		${PROJECT_SOURCE_DIR}/src/boot/strings.r
		${PROJECT_SOURCE_DIR}/src/boot/words.r
		${PROJECT_SOURCE_DIR}/src/boot/modes.r
		${PROJECT_SOURCE_DIR}/src/boot/actions.r
		${PROJECT_SOURCE_DIR}/src/boot/sysobj.r
		${PROJECT_SOURCE_DIR}/src/boot/platforms.r
		${PROJECT_SOURCE_DIR}/src/boot/errors.r
		${PROJECT_SOURCE_DIR}/src/mezz/boot-files.r
		${R3_SRCS_MEZZ_BOOT}
		${PROJECT_SOURCE_DIR}/src/boot/booters.r
		${PROJECT_SOURCE_DIR}/src/boot/natives.r
		${PROJECT_SOURCE_DIR}/src/boot/typespec.r
		${PROJECT_SOURCE_DIR}/src/boot/root.r
		${PROJECT_SOURCE_DIR}/src/boot/task.r
		${PROJECT_SOURCE_DIR}/src/boot/ops.r
	)

add_custom_command(
	COMMAND ${R3_MAKE_CMD} ${R3_SRC_TOOLS_DIR}/make-headers.r
	OUTPUT
		${PROJECT_SOURCE_DIR}/src/include/tmp-funcs.h
		${PROJECT_SOURCE_DIR}/src/include/tmp-funcargs.h
		${PROJECT_SOURCE_DIR}/src/include/tmp-strings.h
	DEPENDS
		${PROJECT_SOURCE_DIR}/src/tools/form-header.r
		${R3_SRCS_CORE}
		${PROJECT_SOURCE_DIR}/src/boot/actions.r
		${PROJECT_SOURCE_DIR}/src/boot/natives.r
		${PROJECT_SOURCE_DIR}/src/core/a-constants.c
	)

add_custom_command(
	COMMAND ${R3_MAKE_CMD} ${R3_SRC_TOOLS_DIR}/make-host-ext.r
	OUTPUT
		${PROJECT_SOURCE_DIR}/src/include/host-ext-graphics.h
		${PROJECT_SOURCE_DIR}/src/include/host-ext-draw.h
		${PROJECT_SOURCE_DIR}/src/include/host-ext-shape.h
		${PROJECT_SOURCE_DIR}/src/include/host-ext-text.h
	DEPENDS
		${PROJECT_SOURCE_DIR}/src/tools/form-header.r
		${PROJECT_SOURCE_DIR}/src/boot/graphics.r
		${PROJECT_SOURCE_DIR}/src/mezz/view-funcs.r
		${PROJECT_SOURCE_DIR}/src/boot/draw.r
		${PROJECT_SOURCE_DIR}/src/boot/shape.r
		${PROJECT_SOURCE_DIR}/src/boot/text.r
	)

add_custom_command(
	COMMAND ${R3_MAKE_CMD} ${R3_SRC_TOOLS_DIR}/make-host-init.r
	OUTPUT
		${PROJECT_SOURCE_DIR}/src/boot/host-init.r
		${PROJECT_SOURCE_DIR}/src/include/host-init.h
	DEPENDS
		${PROJECT_SOURCE_DIR}/src/tools/form-header.r
		${PROJECT_SOURCE_DIR}/src/mezz/prot-http.r
	)

add_custom_command(
	COMMAND ${R3_MAKE_CMD} ${R3_SRC_TOOLS_DIR}/make-os-ext.r
	OUTPUT
		${PROJECT_SOURCE_DIR}/src/include/host-lib.h
	DEPENDS
		${PROJECT_SOURCE_DIR}/src/tools/form-header.r
		${PROJECT_SOURCE_DIR}/src/boot/version.r
		${PROJECT_SOURCE_DIR}/src/tools/systems.r
		${PROJECT_SOURCE_DIR}/src/os/host-device.c
		${PROJECT_SOURCE_DIR}/src/os/${R3_HOST_SUBDIR}/host-lib.c
		${PROJECT_SOURCE_DIR}/src/os/${R3_HOST_SUBDIR}/host-window.c
	)

add_custom_command(
	COMMAND ${R3_MAKE_CMD} ${R3_SRC_TOOLS_DIR}/make-reb-lib.r
	OUTPUT
		${PROJECT_SOURCE_DIR}/src/reb-lib-doc.txt
		${PROJECT_SOURCE_DIR}/src/include/reb-lib.h
		${PROJECT_SOURCE_DIR}/src/include/reb-lib-lib.h
	DEPENDS
		${PROJECT_SOURCE_DIR}/src/tools/form-header.r
		${PROJECT_SOURCE_DIR}/src/boot/version.r
		${PROJECT_SOURCE_DIR}/src/core/a-lib.c
		${PROJECT_SOURCE_DIR}/src/core/f-extension.c
	)

add_custom_target(prepare ALL
	DEPENDS
		# -- boot
		${PROJECT_SOURCE_DIR}/src/include/tmp-evaltypes.h
		${PROJECT_SOURCE_DIR}/src/include/tmp-maketypes.h
		${PROJECT_SOURCE_DIR}/src/include/tmp-comptypes.h
		${PROJECT_SOURCE_DIR}/src/include/reb-types.h
		${PROJECT_SOURCE_DIR}/src/include/ext-types.h
		${PROJECT_SOURCE_DIR}/src/include/tmp-exttypes.h
		${PROJECT_SOURCE_DIR}/src/include/tmp-bootdefs.h
		${PROJECT_SOURCE_DIR}/src/include/tmp-sysobj.h
		${PROJECT_SOURCE_DIR}/src/include/reb-dialect.h
		${PROJECT_SOURCE_DIR}/src/include/reb-evtypes.h
		${PROJECT_SOURCE_DIR}/src/include/tmp-errnums.h
		${PROJECT_SOURCE_DIR}/src/include/tmp-portmodes.h
		${PROJECT_SOURCE_DIR}/src/include/tmp-sysctx.h
		#${PROJECT_SOURCE_DIR}/src/boot/boot-code.r			# unused
		${PROJECT_SOURCE_DIR}/src/core/b-boot.c
		${PROJECT_SOURCE_DIR}/src/include/tmp-boot.h
		# -- headers
		${PROJECT_SOURCE_DIR}/src/include/tmp-funcs.h
		${PROJECT_SOURCE_DIR}/src/include/tmp-funcargs.h
		${PROJECT_SOURCE_DIR}/src/include/tmp-strings.h
		# -- host-ext
		${PROJECT_SOURCE_DIR}/src/include/host-ext-graphics.h
		${PROJECT_SOURCE_DIR}/src/include/host-ext-draw.h
		${PROJECT_SOURCE_DIR}/src/include/host-ext-shape.h
		${PROJECT_SOURCE_DIR}/src/include/host-ext-text.h
		# -- host-init
		#${PROJECT_SOURCE_DIR}/src/boot/host-init.r			# unused
		${PROJECT_SOURCE_DIR}/src/include/host-init.h
		# -- os-ext
		${PROJECT_SOURCE_DIR}/src/include/host-lib.h
		# -- reb-lib
		${PROJECT_SOURCE_DIR}/src/reb-lib-doc.txt
		${PROJECT_SOURCE_DIR}/src/include/reb-lib.h
		${PROJECT_SOURCE_DIR}/src/include/reb-lib-lib.h
	)

# -- Build targets

add_executable(r3 ${R3_SRCS})
target_link_libraries(r3 ${R3_LIBS})

# -- Additional platform-specific configuration

if(MSVC)
	# Add target-level dependency from `r3` to `prepare`, as VS (and/or CMake's
	# VS generator) can't handle file-level cross-target dependencies properly.
	add_dependencies(r3 prepare)
endif()
