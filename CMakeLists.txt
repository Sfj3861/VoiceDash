cmake_minimum_required(VERSION 3.21)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

project(VoiceDash VERSION 0.1.0 LANGUAGES CXX)

file(GLOB_RECURSE VOICEDASH_SOURCES CONFIGURE_DEPENDS
    src/*.cpp
)

add_library(VoiceDash SHARED ${VOICEDASH_SOURCES})
target_include_directories(VoiceDash PRIVATE src)
target_compile_options(VoiceDash PRIVATE
    $<$<CXX_COMPILER_ID:Clang,GNU>:-Wall -Wextra -Wpedantic>
)

if (NOT DEFINED ENV{GEODE_SDK})
    message(FATAL_ERROR "GEODE_SDK is not set. Install the Geode SDK or use geode build.")
endif()

message(STATUS "Using Geode SDK at: $ENV{GEODE_SDK}")
add_subdirectory($ENV{GEODE_SDK} ${CMAKE_CURRENT_BINARY_DIR}/geode)
setup_geode_mod(VoiceDash)

if (ANDROID)
    # AndroidAudioCapture uses native AAudio (available from API 26).
    target_link_libraries(VoiceDash aaudio log)
endif()
