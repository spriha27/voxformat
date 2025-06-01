## Setup Instructions

1.  **Clone the Repository:**
    ```bash
    git clone --recurse-submodules https://your-repo-url/voxformat.git
    cd voxformat
    ```
    (If you forgot `--recurse-submodules`, run `git submodule init && git submodule update --recursive` inside the `voxformat` directory.)

2.  **Run Setup Script:**
    This script will download necessary models and build dependencies.
    ```bash
    bash setup.sh
    ```
    *(For Windows, you might need to provide equivalent PowerShell commands or ask users to follow steps manually from `setup.sh`)*

3.  **Build VoxFormat:**
    *   **Using CMake directly:**
        ```bash
        mkdir build
        cd build
        cmake .. 
        make 
        ```
    *   **Using CLion:** Open the `voxformat` project directory in CLion. It should automatically detect `CMakeLists.txt` and allow you to build. Ensure CLion's working directory for the `voxformat` executable is set to your CMake build directory (e.g., `cmake-build-debug` or `build`).

4.  **Run:**
    Execute the `voxformat` binary from your build directory.
    ```bash
    ./build/voxformat 
    ```
    (Or run from CLion).