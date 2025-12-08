cmake_minimum_required(VERSION 3.20)
project(ProjectAuroraZeta LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules")

if (MINGW)
    add_compile_options(-pthread)
    add_link_options(-pthread)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ZETA_ENABLE_WARNINGS "Enable compiler warnings" ON)

add_subdirectory(engine)
add_subdirectory(game)

add_executable(zeta src/main.cpp)
target_link_libraries(zeta PRIVATE zeta_game engine)
target_include_directories(zeta PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

if (WIN32)
    target_link_options(zeta PRIVATE -mconsole)
endif()

if (ZETA_ENABLE_WARNINGS)
    if (MSVC)
        target_compile_options(zeta PRIVATE /W4 /permissive-)
    else()
        target_compile_options(zeta PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()
