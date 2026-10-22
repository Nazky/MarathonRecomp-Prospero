cmake_policy(SET CMP0077 NEW)

set(MSDF_ATLAS_BUILD_STANDALONE OFF)
set(MSDF_ATLAS_USE_SKIA OFF)
set(MSDF_ATLAS_NO_ARTERY_FONT ON)
set(MSDFGEN_DISABLE_PNG ON)

set(SDL2MIXER_DEPS_SHARED OFF)
set(SDL2MIXER_VENDORED ON)
set(SDL2MIXER_FLAC OFF)
set(SDL2MIXER_MOD OFF)
set(SDL2MIXER_MP3 OFF)
set(SDL2MIXER_MIDI OFF)
set(SDL2MIXER_OPUS OFF)
set(SDL2MIXER_VORBIS "VORBISFILE")
set(SDL2MIXER_WAVPACK OFF)

if (CMAKE_SYSTEM_NAME MATCHES "Linux")
    set(PLUME_SDL_VULKAN_ENABLED ON CACHE BOOL "")
endif()

if (WIN32)
    set(PLUME_D3D12_AGILITY_SDK_ENABLED ON CACHE BOOL "")
endif()

if (APPLE AND CMAKE_OSX_ARCHITECTURES)
    set(BASE_ARCHITECTURE "${CMAKE_OSX_ARCHITECTURES}")
elseif (CMAKE_SYSTEM_PROCESSOR)
    set(BASE_ARCHITECTURE "${CMAKE_SYSTEM_PROCESSOR}")
else()
    set(BASE_ARCHITECTURE "${CMAKE_HOST_SYSTEM_PROCESSOR}")
endif()

# Next, match common architecture strings down to a known common value.
if (BASE_ARCHITECTURE MATCHES "(x86)|(X86)|(amd64)|(AMD64)")
    set(ARCHITECTURE "x86_64")
elseif (BASE_ARCHITECTURE MATCHES "(aarch64)|(AARCH64)|(arm64)|(ARM64)")
    set(ARCHITECTURE "arm64")
else()
    message(FATAL_ERROR "Unsupported CPU architecture: ${BASE_ARCHITECTURE}")
endif()

add_subdirectory("${MARATHON_RECOMP_THIRDPARTY_ROOT}/msdf-atlas-gen")
add_subdirectory("${MARATHON_RECOMP_THIRDPARTY_ROOT}/nativefiledialog-extended")
add_subdirectory("${MARATHON_RECOMP_THIRDPARTY_ROOT}/o1heap")
add_subdirectory("${MARATHON_RECOMP_THIRDPARTY_ROOT}/SDL")
add_subdirectory("${MARATHON_RECOMP_THIRDPARTY_ROOT}/SDL_mixer")
add_subdirectory("${MARATHON_RECOMP_THIRDPARTY_ROOT}/plume")

if (CMAKE_SYSTEM_NAME STREQUAL "Prospero")
    # PacBrew supplies PS5-compiled FFmpeg archives; the ffmpeg-core upstream
    # prebuilt downloads are desktop-only and must not be linked into Prospero.
    if (DEFINED ENV{PS5_DEPS_ROOT} AND NOT "$ENV{PS5_DEPS_ROOT}" STREQUAL "")
        set(_MARATHON_PS5_DEPS_ROOT "$ENV{PS5_DEPS_ROOT}")
    elseif (DEFINED ENV{MARATHON_DEPS_ROOT} AND NOT "$ENV{MARATHON_DEPS_ROOT}" STREQUAL "")
        set(_MARATHON_PS5_DEPS_ROOT "$ENV{MARATHON_DEPS_ROOT}")
    elseif (DEFINED ENV{XDG_CACHE_HOME} AND NOT "$ENV{XDG_CACHE_HOME}" STREQUAL "")
        set(_MARATHON_PS5_DEPS_ROOT "$ENV{XDG_CACHE_HOME}/MarathonRecomp/deps")
    else()
        set(_MARATHON_PS5_DEPS_ROOT "$ENV{HOME}/.cache/MarathonRecomp/deps")
    endif()
    set(PACBREW_SYSROOT "${_MARATHON_PS5_DEPS_ROOT}/pacbrew/v0.40.2/sysroot/user/homebrew"
        CACHE PATH "PacBrew user/homebrew prefix for Prospero builds")
    set(PACBREW_ROOT "${PACBREW_SYSROOT}/../..")
    set(ENV{PKG_CONFIG_SYSROOT_DIR} "${PACBREW_ROOT}")
    set(ENV{PKG_CONFIG_LIBDIR} "${PACBREW_SYSROOT}/lib/pkgconfig:${PACBREW_SYSROOT}/libdata/pkgconfig")
    set(ENV{PKG_CONFIG_PATH} "${PACBREW_SYSROOT}/libdata/pkgconfig")
    set(ENV{PKG_CONFIG_DIR} "")

    find_package(PkgConfig REQUIRED)
    execute_process(
        COMMAND ${PKG_CONFIG_EXECUTABLE} --static --cflags libavcodec
        OUTPUT_VARIABLE PACBREW_FFMPEG_CFLAGS
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE PACBREW_FFMPEG_CFLAGS_STATUS
    )
    execute_process(
        COMMAND ${PKG_CONFIG_EXECUTABLE} --static --libs libavcodec
        OUTPUT_VARIABLE PACBREW_FFMPEG_LIBS
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE PACBREW_FFMPEG_LIBS_STATUS
    )
    if (NOT PACBREW_FFMPEG_CFLAGS_STATUS EQUAL 0 OR NOT PACBREW_FFMPEG_LIBS_STATUS EQUAL 0)
        message(FATAL_ERROR "PacBrew FFmpeg is required on Prospero. Set PACBREW_SYSROOT to its user/homebrew prefix.")
    endif()
    separate_arguments(PACBREW_FFMPEG_CFLAGS_LIST NATIVE_COMMAND "${PACBREW_FFMPEG_CFLAGS}")
    separate_arguments(PACBREW_FFMPEG_LIBS_LIST NATIVE_COMMAND "${PACBREW_FFMPEG_LIBS}")

    add_library(ffmpeg INTERFACE)
    target_include_directories(ffmpeg INTERFACE "${PACBREW_SYSROOT}/include")
    target_compile_options(ffmpeg INTERFACE ${PACBREW_FFMPEG_CFLAGS_LIST})
    target_link_libraries(ffmpeg INTERFACE ${PACBREW_FFMPEG_LIBS_LIST})
else()
    add_subdirectory("${MARATHON_RECOMP_THIRDPARTY_ROOT}/ffmpeg-core")
endif()
