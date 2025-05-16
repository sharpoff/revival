cmake -B build -D CMAKE_BUILD_TYPE=Debug -D CMAKE_EXPORT_COMPILE_COMMANDS=1 -G Ninja && ninja -C build -j$(nproc)
