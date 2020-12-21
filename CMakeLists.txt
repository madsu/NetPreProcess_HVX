cmake_minimum_required(VERSION 3.1)

project(DSPDemo)

message(${CMAKE_SOURCE_DIR})
message(${CMAKE_CURRENT_SOURCE_DIR})

option(ENABLE_HVX "hvx compute support" OFF)

set(incs
    ${CMAKE_CURRENT_SOURCE_DIR}/src/
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/otherlibs/common/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/otherlibs/dsp/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/otherlibs/rpcmem/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/otherlibs/qmath/inc
    ${CMAKE_CURRENT_BINARY_DIR}/
    ${HEXAGON_SDK_ROOT}/incs/
    ${HEXAGON_SDK_ROOT}/incs/qlist
    ${HEXAGON_SDK_ROOT}/incs/stddef
    ${HEXAGON_SDK_ROOT}/incs/a1std
)

# Add necessary includes for the base build
include_directories(
    ${incs}        
)

set(CMAKE_CXX_STANDARD 11)

if(ANDROID)
    set(LIB_NAME netprocess)
    add_library(${LIB_NAME}
            SHARED 
            ${CMAKE_CURRENT_BINARY_DIR}/pre_process_stub
    )
    set_target_properties(${LIB_NAME} PROPERTIES SUFFIX "_stub.so")

    aux_source_directory(${CMAKE_SOURCE_DIR}/testbed/src BIN_SRCS)
    add_executable(${PROJECT_NAME} ${BIN_SRCS})

    target_compile_definitions(
        ${PROJECT_NAME}
        PUBLIC VERIFY_PRINT_ERROR
    )

    target_link_options(${PROJECT_NAME} PUBLIC -llog -lm)

    message(STATUS "ANDROID_NDK_REVISION:${ANDROID_NDK_REVISION}")
    message(STATUS "ANDROID_NDK_RELEASE:${ANDROID_NDK_RELEASE}")

    #This is to work around the NDK r19c C++ issues
    if(CMAKE_HOST_SYSTEM_NAME STREQUAL Linux)
        SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -isystem ${ANDROID_NDK}/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/include/c++/v1/")
    endif()
    if(CMAKE_HOST_SYSTEM_NAME STREQUAL Windows)
        SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -isystem ${ANDROID_NDK}/toolchains/llvm/prebuilt/windows-x86_64/sysroot/usr/include/c++/v1/")
    endif()

    # slect the required RPC Build
    #default is CDSP
    SET(dsprpc "cdsprpc")        
    if(${DSP_TYPE} MATCHES "mdsp" )
        #  MDSP
        SET(dsprpc "mdsprpc")    
    elseif(${DSP_TYPE} MATCHES "adsp")
        #  ADSP
        SET(dsprpc "adsprpc")        
    elseif(${DSP_TYPE} MATCHES "sdsp")
        #  SDSP
        SET(dsprpc "sdsprpc")        
    else()
        #  Default CDSP
        SET(dsprpc "cdsprpc")
    endif()
    add_library(${dsprpc} SHARED IMPORTED) # or STATIC instead of SHARED
    set_target_properties(${dsprpc} PROPERTIES
        IMPORTED_LOCATION "${HEXAGON_SDK_ROOT}/libs/common/remote/ship/${V}/lib${dsprpc}.so"
        INTERFACE_INCLUDE_DIRECTORIES "${HEXAGON_SDK_ROOT}/libs/common/remote/ship/${V}/"
    )

    add_dependencies(${LIB_NAME} ${dsprpc})
    target_link_libraries(${LIB_NAME} ${dsprpc})
    target_link_libraries(${PROJECT_NAME} ${CMAKE_SOURCE_DIR}/otherlibs/rpcmem/libs/android_aarch64/rpcmem.a)

elseif(ENABLE_HVX)
    aux_source_directory(${CMAKE_SOURCE_DIR}/src LIB_SRCS)
    set(LIB_NAME netprocess)
    add_library(${LIB_NAME}
            SHARED 
            ${CMAKE_CURRENT_BINARY_DIR}/pre_process_skel
            ${LIB_SRCS}
    )

    set_target_properties(${LIB_NAME} PROPERTIES SUFFIX "_skel.so")

    aux_source_directory(${CMAKE_SOURCE_DIR}/src LIB_SRCS)
    aux_source_directory(${CMAKE_SOURCE_DIR}/otherlibs/dsp/src BIN_SRCS)
    aux_source_directory(${CMAKE_SOURCE_DIR}/testbed/src BIN_SRCS)
    add_executable(${PROJECT_NAME} ${BIN_SRCS} ${LIB_SRCS})

    target_link_libraries(${PROJECT_NAME} hexagon m)
    target_link_libraries(${PROJECT_NAME} ${CMAKE_SOURCE_DIR}/otherlibs/rpcmem/libs/hexagon_aarch64/rpcmem.a)
endif()