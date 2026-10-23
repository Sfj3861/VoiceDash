cmake_minimum_required(VERSION 3.21)
project(VoiceDash VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(VoiceDash SHARED
    src/main.cpp
    src/VoiceDetector.cpp
    src/AudioCapture.cpp
)

target_include_directories(VoiceDash PRIVATE src)
target_compile_options(VoiceDash PRIVATE
    $<$<CXX_COMPILER_ID:Clang,GNU>:-Wall -Wextra -Wpedantic>
)

# Geode supplies setup_geode_mod() through its toolchain/template.
if(COMMAND setup_geode_mod)
    setup_geode_mod(VoiceDash)
else()
    message(FATAL_ERROR "Geode SDK toolchain not loaded. Configure this project with the Geode SDK build environment.")
endif()
