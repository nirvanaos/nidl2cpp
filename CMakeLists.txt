set(CMAKE_TOOLCHAIN_FILE $ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake)
cmake_minimum_required(VERSION 3.20)
cmake_policy(SET CMP0010 NEW)
cmake_policy(SET CMP0053 NEW)
set(CMAKE_CONFIGURATION_TYPES Debug Release)

project(nidl2cpp CXX)

add_subdirectory(src)
install(TARGETS nidl2cpp)
