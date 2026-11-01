cmake_minimum_required(VERSION 3.29)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(PLStudioAdventure VERSION 1.0.0)

add_library(${PROJECT_NAME} SHARED src/main.cpp)

if (NOT DEFINED ENV{GEODE_SDK})
    message(FATAL_ERROR "GEODE_SDK is not set. Run: geode sdk install")
endif()

add_subdirectory($ENV{GEODE_SDK} ${CMAKE_CURRENT_BINARY_DIR}/geode)
# node-ids is a runtime dependency provided by Geode's mod index. Mark it as
# an external during local builds so packaging can be validated without a
# configured Geode installation profile; mod.json still enforces it at load.
setup_geode_mod(${PROJECT_NAME} EXTERNALS geode.node-ids:v1.23.3)
