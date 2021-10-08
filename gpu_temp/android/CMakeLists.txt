cmake_minimum_required(VERSION 3.4.1)

set(PROJECT_NAME "gpu_temp")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD   17)
# set(CMAKE_CXX11_EXTENSION_COMPILE_OPTION "-std=c++17")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} -std=c++17 -fexceptions")
include_directories(${CMAKE_SOURCE_DIR})
include_directories(${CMAKE_SOURCE_DIR}/fmt/include)
# add_subdirectory(xhook)
set(SOURCES
    gpu_temp.cpp
    )

add_executable(${PROJECT_NAME} ${SOURCES})
target_link_libraries(${PROJECT_NAME} log android base fmt)