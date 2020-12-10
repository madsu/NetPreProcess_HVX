cmake_minimum_required(VERSION 3.1)

project(OpenCLDemo)

message(${CMAKE_SOURCE_DIR})
message(${CMAKE_CURRENT_SOURCE_DIR})

# ignore loop-vectorize warning
if(SYSTEM.Windows)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
else()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-pass-failed")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-pass-failed")
endif()

# ignore deprecated warning
if(SYSTEM.Windows)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
else()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-deprecated-declarations -Wno-ignored-attributes")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -Wno-deprecated-declarations -Wno-ignored-attributes")
endif()

if(UNIX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include_directories(inc)
include_directories(/usr/local/cuda/include)
include_directories(${CMAKE_SOURCE_DIR}/otherlibs/gtest/inc)
aux_source_directory(${CMAKE_SOURCE_DIR}/src LIB_SRCS)
set(LIB_NAME ClDemo)
add_library(${LIB_NAME} SHARED ${LIB_SRCS})

aux_source_directory(${CMAKE_SOURCE_DIR}/testbed/src BIN_SRCS)
add_executable(${PROJECT_NAME} ${BIN_SRCS})
if(ANDROID)
    target_link_libraries(${PROJECT_NAME} ${LIB_NAME} dl)
    target_link_libraries(${PROJECT_NAME} ${CMAKE_SOURCE_DIR}/otherlibs/gtest/libs/aarch64/libgtest.a)
else()
    target_link_libraries(${PROJECT_NAME} ${LIB_NAME} gtest dl)
endif()
