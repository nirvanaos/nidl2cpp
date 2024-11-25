cmake_minimum_required(VERSION 3.28)
cmake_policy(SET CMP0010 NEW)
cmake_policy(SET CMP0053 NEW)

set(CMAKE_CONFIGURATION_TYPES Debug Release)

project(nidl2cpp CXX)
set(CMAKE_CXX_SCAN_FOR_MODULES 0)

add_subdirectory(src)
install(TARGETS nidl2cpp)
