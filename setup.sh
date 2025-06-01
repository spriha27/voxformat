#!/bin/bash

echo "--- VoxFormat Setup ---"

# 1. Initialize and update Git submodules
echo "[INFO] Initializing and updating Git submodules (PortAudio, Whisper.cpp)..."
git submodule init
git submodule update --recursive --progress
if [ $? -ne 0 ]; then
    echo "[ERROR] Failed to update submodules. Please check your Git setup and internet connection."
    exit 1
fi
echo "[SUCCESS] Submodules updated."
echo

# 2. Download Whisper.cpp model
echo "[INFO] Navigating to Whisper.cpp directory to download model..."
if [ -d "external/whisper.cpp" ]; then
    cd external/whisper.cpp
    echo "[INFO] Downloading ggml-base.en model for Whisper.cpp..."
    # Check if models directory exists, if not, the script might create it or be inside it
    if [ -f "./models/download-ggml-model.sh" ]; then
        bash ./models/download-ggml-model.sh base.en
    elif [ -f "./download-ggml-model.sh" ]; then # If script is in root of whisper.cpp
        bash ./download-ggml-model.sh base.en
    else
        echo "[WARNING] Could not find download-ggml-model.sh in standard locations."
        echo "          Please download 'ggml-base.en.bin' manually into 'external/whisper.cpp/models/'."
    fi

    if [ -f "./models/ggml-base.en.bin" ]; then
        echo "[SUCCESS] Whisper model ggml-base.en.bin downloaded."
    else
        echo "[ERROR] Failed to download Whisper model. Please check 'external/whisper.cpp/models/'."
    fi
    cd ../.. # Go back to project root
else
    echo "[ERROR] external/whisper.cpp directory not found. Submodule update might have failed."
    exit 1
fi
echo

# 3. Build Whisper.cpp (and its dependencies like ggml)
# This is important because your project links against libwhisper.a/dylib
# and it needs ggml-metal.metal to be compiled into default.metallib for Metal support.
echo "[INFO] Building Whisper.cpp library (this may take a few minutes)..."
if [ -d "external/whisper.cpp" ]; then
    cd external/whisper.cpp
    make clean # Optional: for a clean build
    make libwhisper.so # Or libwhisper.dylib on macOS, or just 'make' if it builds the library
    # On macOS, 'make libwhisper.dylib' or just 'make' might be appropriate
    # Check Whisper.cpp's Makefile for the correct target to build the library
    # If it generates default.metallib, that's good.
    if [ $? -ne 0 ]; then
        echo "[ERROR] Failed to build Whisper.cpp library."
    else
        echo "[SUCCESS] Whisper.cpp library built."
    fi
    cd ../..
else
    echo "[ERROR] external/whisper.cpp directory not found."
fi
echo

# 4. Copy ggml-metal.metal (if needed, though building whisper.cpp often handles this by creating default.metallib)
# This step is a fallback if Metal still doesn't work after building whisper.cpp
# The build process of whisper.cpp with Metal enabled should ideally create default.metallib
# in a place where your main application can find it when libwhisper is linked.
# If not, copying ggml-metal.metal to the CMake build directory is a workaround.
BUILD_DIR="cmake-build-debug" # Or whatever your typical build dir is
GGML_METAL_SOURCE_PATH_1="external/whisper.cpp/ggml-metal.metal"
GGML_METAL_SOURCE_PATH_2="external/whisper.cpp/src/ggml-metal.metal" # Alternative common location

echo "[INFO] Checking for Metal shader for application runtime..."
if [ ! -d "$BUILD_DIR" ]; then
    echo "[INFO] CMake build directory ($BUILD_DIR) not found. Skipping ggml-metal.metal copy."
    echo "       Please run CMake configuration and build first if you encounter Metal issues."
else
    if [ -f "$GGML_METAL_SOURCE_PATH_1" ]; then
        echo "[INFO] Copying $GGML_METAL_SOURCE_PATH_1 to $BUILD_DIR/ggml-metal.metal"
        cp "$GGML_METAL_SOURCE_PATH_1" "$BUILD_DIR/ggml-metal.metal"
    elif [ -f "$GGML_METAL_SOURCE_PATH_2" ]; then
        echo "[INFO] Copying $GGML_METAL_SOURCE_PATH_2 to $BUILD_DIR/ggml-metal.metal"
        cp "$GGML_METAL_SOURCE_PATH_2" "$BUILD_DIR/ggml-metal.metal"
    else
        echo "[WARNING] ggml-metal.metal not found in typical locations. Metal GPU acceleration might require manual setup if default.metallib was not created by whisper.cpp build."
    fi
fi
echo

# 5. Build your main VoxFormat project
echo "[INFO] You can now configure and build VoxFormat using CMake:"
echo "       mkdir build && cd build"
echo "       cmake .."
echo "       make"
echo "(Or open the project in CLion and let it configure/build)."
echo
echo "--- Setup Complete ---"