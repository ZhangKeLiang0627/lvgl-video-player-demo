# Cross-compile toolchain for 64-bit ARM (aarch64) Linux targets,
# e.g. Rockchip RK35xx boards (RK3566 / RK3588).
#
# Usage (from the repo root, keep ENABLE_SDL=OFF for the device framebuffer):
#   cmake -S . -B build/arm64 \
#       -DCMAKE_TOOLCHAIN_FILE=cmake/aarch64-linux-gnu.cmake \
#       -DCMAKE_BUILD_TYPE=Release
#   cmake --build build/arm64 -j"$(nproc)"
#
# Requires the cross toolchain + aarch64 dev packages on the build host:
#   sudo apt install g++-aarch64-linux-gnu \
#        libasound2-dev:arm64 libavformat-dev:arm64 ...
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)
