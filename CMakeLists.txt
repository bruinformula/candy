cmake_minimum_required(VERSION 3.26)

project(Candy VERSION 0.1 LANGUAGES CXX C ASM)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/lib")

include(CTest)
enable_testing()

#add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/test")
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/template")
