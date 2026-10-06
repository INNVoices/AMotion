cmake_minimum_required(VERSION 3.20)
project(amotion LANGUAGES CXX)

option(AMOTION_BUILD_EXAMPLES "Build AMotion examples" ON)

add_library(amotion STATIC
    src/amotion.cpp
)

target_include_directories(amotion
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_compile_features(amotion PUBLIC cxx_std_20)

if (MSVC)
    target_compile_options(amotion PRIVATE /W4 /permissive-)
else()
    target_compile_options(amotion PRIVATE -Wall -Wextra -Wpedantic)
endif()

if (AMOTION_BUILD_EXAMPLES)
    add_executable(amotion_demo examples/demo.cpp)
    target_link_libraries(amotion_demo PRIVATE amotion)
endif()
