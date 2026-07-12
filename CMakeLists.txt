cmake_minimum_required(VERSION 3.20)
project(CTR-Native C)
include(CheckCCompilerFlag)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CTR_NATIVE_VERSION "0.1.0-beta.7.1")

# The decompiled game code assumes 32-bit pointers (PSX ILP32). On x86 hosts
# force -m32; other 32-bit targets (Android armeabi-v7a) are ILP32 natively
# and must NOT get x86 flags.
set(CTR_TARGET_IS_X86 FALSE)
if(CMAKE_C_COMPILER_TARGET MATCHES "i.86|x86_64|amd64" OR CMAKE_SYSTEM_PROCESSOR MATCHES "i.86|x86_64|AMD64|amd64")
    set(CTR_TARGET_IS_X86 TRUE)
endif()

if(CTR_TARGET_IS_X86 AND NOT CMAKE_C_COMPILER_TARGET MATCHES "i.86" AND NOT CMAKE_SYSTEM_PROCESSOR MATCHES "i.86")
    set(CMAKE_C_FLAGS "-m32 ${CMAKE_C_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS "-m32 ${CMAKE_EXE_LINKER_FLAGS}")
endif()

set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE NEVER)

execute_process(
    COMMAND git rev-parse --short=12 HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE CTR_NATIVE_GIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(NOT CTR_NATIVE_GIT_HASH)
    set(CTR_NATIVE_GIT_HASH "unknown")
endif()

execute_process(
    COMMAND git diff --quiet --exit-code
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    RESULT_VARIABLE CTR_NATIVE_GIT_DIRTY
    ERROR_QUIET
)
if(CTR_NATIVE_GIT_DIRTY EQUAL 0)
    set(CTR_NATIVE_BUILD_ID "${CTR_NATIVE_GIT_HASH}")
else()
    string(TIMESTAMP CTR_NATIVE_BUILD_TIMESTAMP "%Y%m%d%H%M%S" UTC)
    set(CTR_NATIVE_BUILD_ID "${CTR_NATIVE_GIT_HASH}-dirty-${CTR_NATIVE_BUILD_TIMESTAMP}")
endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-error -O3 -ffunction-sections -fdata-sections")
if(CTR_TARGET_IS_X86)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -msse")
endif()
if(ANDROID)
    # The NDK injects -Werror=format-security by default; retail game code has
    # non-literal sprintf format strings and must stay untouched, so demote it
    # back to a warning (appended after the toolchain's flags, so it wins).
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-error=format-security")
endif()

function(ctr_native_add_label_warning_if_supported target warning flag_check)
    set(saved_required_flags "${CMAKE_REQUIRED_FLAGS}")
    if(CMAKE_C_COMPILER_ID MATCHES "Clang")
        set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} -Werror=unknown-warning-option")
    endif()

    check_c_compiler_flag("${warning}" "${flag_check}")
    set(CMAKE_REQUIRED_FLAGS "${saved_required_flags}")

    if(${flag_check})
        target_compile_options(${target} PRIVATE "${warning}")
    endif()
endfunction()

# SDL3
# Android: SDLActivity loads libSDL3.so + libmain.so at runtime, so SDL must
# be shared there; everywhere else it stays statically linked as before.
if(ANDROID)
    set(SDL_SHARED ON CACHE BOOL "" FORCE)
    set(SDL_STATIC OFF CACHE BOOL "" FORCE)
else()
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
endif()
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
set(SDL_GPU OFF CACHE BOOL "" FORCE)
set(SDL_RENDER OFF CACHE BOOL "" FORCE)
set(SDL_CAMERA OFF CACHE BOOL "" FORCE)
set(SDL_SENSOR OFF CACHE BOOL "" FORCE)
# File dialog: Android uses it for the first-run ROM picker (SAF system
# picker, no storage permissions); other platforms don't need it.
if(ANDROID)
    set(SDL_DIALOG ON CACHE BOOL "" FORCE)
else()
    set(SDL_DIALOG OFF CACHE BOOL "" FORCE)
endif()
set(SDL_TRAY OFF CACHE BOOL "" FORCE)
set(SDL_POWER OFF CACHE BOOL "" FORCE)
set(SDL_VULKAN OFF CACHE BOOL "" FORCE)
# OpenGL ES/EGL support in SDL: on Android it IS the GL driver (without it
# SDL_CreateWindow(OPENGL) fails outright); on desktop the WGL/GLX path covers
# both GL and GLES contexts, so it stays off to keep the build lean.
if(ANDROID)
    set(SDL_OPENGLES ON CACHE BOOL "" FORCE)
else()
    set(SDL_OPENGLES OFF CACHE BOOL "" FORCE)
endif()
set(SDL_OFFSCREEN OFF CACHE BOOL "" FORCE)
set(SDL_SNDIO OFF CACHE BOOL "" FORCE)
set(SDL_SNDIO_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_X11_XSCRNSAVER OFF CACHE BOOL "" FORCE)
set(SDL_X11_XTEST OFF CACHE BOOL "" FORCE)
add_subdirectory(externals/SDL)

# CTR Native
# Android: the game is a shared library (libmain.so) that SDLActivity loads;
# everywhere else it is the usual executable.
if(ANDROID)
    add_library(ctr_native SHARED main.c)
    set_target_properties(ctr_native PROPERTIES OUTPUT_NAME main)
else()
    add_executable(ctr_native main.c)
endif()
target_include_directories(ctr_native PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_compile_definitions(ctr_native PRIVATE
    CTR_NATIVE
    BUILD=926
    CTR_INTERNAL
    CTR_NATIVE_VERSION=\"${CTR_NATIVE_VERSION}\"
    CTR_NATIVE_BUILD_ID=\"${CTR_NATIVE_BUILD_ID}\"
)
target_compile_options(ctr_native PRIVATE -O3)
if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(ctr_native PRIVATE -Wenum-conversion -Wenum-compare -Wswitch-enum -Wformat -Wparentheses)
endif()
if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_compile_options(ctr_native PRIVATE -Wall -Wextra -Wstrict-aliasing=2)
    ctr_native_add_label_warning_if_supported(ctr_native -Wfree-labels CTR_HAS_WFREE_LABELS)
elseif(CMAKE_C_COMPILER_ID MATCHES "Clang")
    ctr_native_add_label_warning_if_supported(ctr_native -Wc23-extensions CTR_HAS_WC23_EXTENSIONS)
endif()
target_link_libraries(ctr_native SDL3::SDL3)
target_link_options(ctr_native PRIVATE -Wl,--gc-sections $<$<NOT:$<CONFIG:Debug>>:-s>)
if(NOT ANDROID)
    target_link_options(ctr_native PRIVATE -static-libgcc)
endif()
if(WIN32)
    target_link_options(ctr_native PRIVATE -static)
endif()
