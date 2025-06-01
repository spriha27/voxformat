cmake_minimum_required(VERSION 3.30)
project(voxformat LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

FetchContent_Declare(
        libsamplerate
        GIT_REPOSITORY https://github.com/libsndfile/libsamplerate.git
        GIT_TAG        0.2.2
)

FetchContent_MakeAvailable(libsamplerate)

if(EXISTS "${CMAKE_SOURCE_DIR}/external/portaudio/CMakeLists.txt")
    add_subdirectory(external/portaudio portaudio_build)
else()
    message(FATAL_ERROR "PortAudio source not found in external/portaudio")
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/external/whisper.cpp/CMakeLists.txt")
    set(WHISPER_METAL ON CACHE BOOL "Enable Metal for whisper.cpp" FORCE)
    add_subdirectory(external/whisper.cpp whisper_cpp_build)
else()
    message(FATAL_ERROR "Whisper.cpp source not found in external/whisper.cpp")
endif()

set(GGML_METAL_PATH_RESOURCES ${CMAKE_SOURCE_DIR}/external/whisper.cpp/models)
add_executable(voxformat
        main.cpp
        document_formatter.cpp
        audio_capturer.cpp
        whisper_processor.cpp
        utils.cpp
)
target_link_libraries(voxformat PRIVATE portaudio samplerate whisper)

if (CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUXX)
    target_link_libraries(voxformat PRIVATE stdc++fs)
elseif(MSVC)
    # For MSVC, /std:c++17 or /std:c++20 should be enough
endif()

if(APPLE)
    target_link_libraries(voxformat PRIVATE
            "-framework CoreAudio"
            "-framework AudioToolbox"
            "-framework AudioUnit"
            "-framework CoreServices"
    )
endif()

set_property(TARGET voxformat PROPERTY CXX_STANDARD 20)