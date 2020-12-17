cmake_minimum_required(VERSION 3.1)

project(DSPDemo)

message(${CMAKE_SOURCE_DIR})
message(${CMAKE_CURRENT_SOURCE_DIR})

option(ENABLE_HVX "hvx compute support" OFF)

include_directories(inc)
include_directories(otherlibs/common/inc)

if(ENABLE_HVX)
    add_compile_definitions(DQDSP6SS_PUB_BASE=0xFE200000)
    include_directories(${CMAKE_SOURCE_DIR}/otherlibs/dsp/inc)
    aux_source_directory(${CMAKE_SOURCE_DIR}/otherlibs/dsp/src LIB_SRCS)
    aux_source_directory(${CMAKE_SOURCE_DIR}/src LIB_SRCS)
    set(LIB_NAME DspVec)
    add_library(${LIB_NAME} STATIC ${LIB_SRCS})
endif()

aux_source_directory(${CMAKE_SOURCE_DIR}/testbed/src BIN_SRCS)
add_executable(${PROJECT_NAME} ${BIN_SRCS})

if(ENABLE_HVX)
    target_link_libraries(${PROJECT_NAME} ${LIB_NAME})
    target_link_libraries(${PROJECT_NAME} hexagon)
else()
    target_link_libraries(${PROJECT_NAME} m)
endif()