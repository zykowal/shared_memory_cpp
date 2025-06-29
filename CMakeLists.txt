cmake_minimum_required(VERSION 3.22)

project(
    "shared_memory"
    VERSION 1.0.0
    LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(LIBRARY_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/optimized_status.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/shared_memory_export.cpp"
)
set(LIBRARY_HEADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/optimized_status.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/shared_memory_export.h"
)

add_library(SHARED_MEM_MAP SHARED ${LIBRARY_SOURCES} ${LIBRARY_HEADERS})
target_include_directories(SHARED_MEM_MAP PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

if(APPLE)
    target_link_libraries(SHARED_MEM_MAP pthread)
else()
    target_link_libraries(SHARED_MEM_MAP pthread rt)
endif()

add_executable(write write.cpp)
add_executable(read read.cpp)
add_executable(clean_shared clean_shared.cpp)

target_link_libraries(write dl)
target_link_libraries(read dl)
target_link_libraries(clean_shared dl)
