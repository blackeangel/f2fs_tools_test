cmake_minimum_required(VERSION 3.30)
project(f2fs_tools C)

set(CMAKE_C_STANDARD 23)
include(GNUInstallDirs)
set(CMAKE_CXX_LINK_GROUP_USING_cross_refs_SUPPORTED TRUE)
set(CMAKE_CXX_LINK_GROUP_USING_cross_refs
        "LINKER:--start-group"
        "LINKER:--end-group"
)

include_directories(
        ${PROJECT_SOURCE_DIR}/src
)
file(GLOB SRCS
        "${PROJECT_SOURCE_DIR}/src/*"
)
add_executable(f2fs_tools ${SRCS})
