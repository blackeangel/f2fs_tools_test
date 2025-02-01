cmake_minimum_required(VERSION 3.30)
project(f2fs_tools C)

set(CMAKE_C_STANDARD 23)
include(GNUInstallDirs)

include_directories(
        ${PROJECT_SOURCE_DIR}/src
)
file(GLOB SRCS
        "${PROJECT_SOURCE_DIR}/src/*"
)
add_executable(f2fs_tools ${SRCS})
