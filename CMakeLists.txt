cmake_minimum_required(VERSION 3.30)
project(voxformat LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(/usr/local/include)
link_directories(/usr/local/lib)

add_executable(voxformat main.cpp)
target_link_libraries(voxformat PRIVATE portaudio)

if(APPLE)
    target_link_libraries(voxformat PRIVATE
            "-framework CoreAudio"
            "-framework AudioToolbox"
            "-framework AudioUnit"
            "-framework CoreServices"
            "-framework Carbon"
    )
endif()