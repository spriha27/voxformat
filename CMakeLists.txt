cmake_minimum_required(VERSION 3.30) # You had 3.20 in the last one I corrected, 3.30 is fine
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

# This set command for GGML_METAL_PATH_RESOURCES will only set a CMake variable,
# not a runtime environment variable. Runtime discovery of ggml-metal.metal
# depends on it being in the CWD or other paths Whisper.cpp checks.
# set(GGML_METAL_PATH_RESOURCES ${CMAKE_SOURCE_DIR}/external/whisper.cpp/models) # This line was in your version. Models dir is wrong for shaders.

add_executable(voxformat
        main.cpp
        document_formatter.cpp # <<< IT IS LISTED HERE!
        audio_capturer.cpp
        whisper_processor.cpp
        utils.cpp
)
target_link_libraries(voxformat PRIVATE portaudio samplerate whisper)

if (CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUXX)
    # For std::filesystem with GCC < 9, you might need to link stdc++fs
    # Modern GCC/Clang with C++17/20 usually don't need this explicitly for std::filesystem
    # target_link_libraries(voxformat PRIVATE stdc++fs)
elseif(MSVC)
    # For MSVC, /std:c++17 or /std:c++20 should be enough for std::filesystem
endif()

if(APPLE)
    target_link_libraries(voxformat PRIVATE
            "-framework CoreAudio"
            "-framework AudioToolbox"
            # "-framework AudioUnit" # Usually not needed unless directly using AudioUnits
            # "-framework CoreServices" # Usually pulled in if necessary
            "-framework Foundation"   # Often useful on Apple
            "-framework Accelerate"   # If whisper.cpp uses it
            "-framework Metal"        # If whisper.cpp uses it
    )
endif()

# set_property(TARGET voxformat PROPERTY CXX_STANDARD 20) # Already set globally