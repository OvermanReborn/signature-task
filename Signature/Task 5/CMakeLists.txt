cmake_minimum_required(VERSION 3.20)
project(sigscan LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(FetchContent)
FetchContent_Declare(miniz
    GIT_REPOSITORY https://github.com/richgel999/miniz.git
    GIT_TAG 3.0.2)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(miniz)
add_executable(sigscan main.cpp)
target_link_libraries(sigscan PRIVATE miniz)
