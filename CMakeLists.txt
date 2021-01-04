cmake_minimum_required(VERSION 3.1)

project(DSPDemo C CXX ASM)

enable_language(ASM)

set(CMAKE_DEBUG_TARGET_PROPERTIES
    INCLUDE_DIRECTORIES
    COMPILE_DEFINITIONS
    POSITION_INDEPENDENT_CODE
    CONTAINER_SIZE_REQUIRED
    LIB_VERSION
    )
include(${HEXAGON_CMAKE_ROOT}/hexagon_fun.cmake)

option(BUILD_DSP_SHARED "build dsp so" OFF)

if(${CMAKE_SYSTEM_NAME} MATCHES "Android")
    set(ignoreMe ${DSP_TYPE})

    set(LIB_NAME pre_process)
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

    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIE")
    SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -fPIE")
    SET(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS}  -fPIE -pie")

    message(STATUS "ANDROID_NDK_REVISION:${ANDROID_NDK_REVISION}")
    message(STATUS "ANDROID_NDK_RELEASE:${ANDROID_NDK_RELEASE}")

    #This is to work around the NDK r19c C++ issues
     if(CMAKE_HOST_SYSTEM_NAME STREQUAL Linux)
        SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -isystem ${ANDROID_NDK}/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/include/c++/v1/")
    endif()
    if(CMAKE_HOST_SYSTEM_NAME STREQUAL Windows)
        SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -isystem ${ANDROID_NDK}/toolchains/llvm/prebuilt/windows-x86_64/sysroot/usr/include/c++/v1/")
    endif()

    set(incs
        ${CMAKE_CURRENT_SOURCE_DIR}/otherlibs/opencv/inc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/
        ${CMAKE_CURRENT_BINARY_DIR}/
        )

    buildIDL(inc/pre_process.idl ${LIB_NAME})

    include_directories(
        ${incs}
        )
    
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

    prepare_libraries_hexagon(hexagonTargets_2 hexagonIncs_2 hexagonLibs_2 hexagonSharedLibs_2
        rpcmem.a
        )

    include_directories(
        ${hexagonIncs_2}
        )

    set (EXTRA_LIBS ${EXTRA_LIBS} ${hexagonLibs_2})
    set (EXTRA_LIBS ${EXTRA_LIBS} ${LIB_NAME})

    add_dependencies(${PROJECT_NAME} ${LIB_NAME} rpcmem)

    target_link_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/otherlibs/opencv/libs/android_arm64-v8a)
    target_link_libraries( ${PROJECT_NAME} ${EXTRA_LIBS} m z log jnigraphics mediandk opencv_java4)

elseif(BUILD_DSP_SHARED)
    aux_source_directory(${CMAKE_SOURCE_DIR}/src LIB_SRCS)
    set(LIB_NAME pre_process_skel)
    add_library(${LIB_NAME}
            SHARED 
            ${CMAKE_CURRENT_BINARY_DIR}/pre_process_skel
            ${LIB_SRCS}
    )

    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fexceptions")

    set(incs 
        ${CMAKE_CURRENT_SOURCE_DIR}/otherlibs/dsp/inc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/
        ${CMAKE_CURRENT_BINARY_DIR}/
        ${HEXAGON_SDK_ROOT}/incs/HAP/
        )
    
    prepare_libraries_hexagon(hexagonTargets hexagonIncs hexagonLibs hexagonSharedLibs
        libdspCV_skel
        libqprintf.a
        )
    message(STATUS "hexagonTarget:${hexagonTargets}")
    message(STATUS "hexagonLibs:${hexagonLibs}")
    message(STATUS "hexagonIncs:${hexagonIncs}")

    include_directories(
            ${incs}
            ${hexagonIncs}
            )

    buildIDL(inc/pre_process.idl ${LIB_NAME} incs)

    add_dependencies(${LIB_NAME} ${hexagonTargets})

    target_link_libraries(${LIB_NAME} ${hexagonLibs})

else()
    aux_source_directory(${CMAKE_SOURCE_DIR}/src LIB_SRCS)
    aux_source_directory(${CMAKE_SOURCE_DIR}/testbed/src BIN_SRCS)
    add_executable(${PROJECT_NAME} ${BIN_SRCS} ${LIB_SRCS})
    set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX "-smi")

    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fexceptions")

    set(incs 
        ${CMAKE_CURRENT_SOURCE_DIR}/src/
        ${CMAKE_CURRENT_BINARY_DIR}/
        ${HEXAGON_SDK_ROOT}/incs/HAP/
        #${HEXAGON_TOOLCHAIN_ROOT}/Tools/target/hexagon/include/
        )

    prepare_libraries_hexagon(hexagonTargets hexagonIncs hexagonLibs hexagonSharedLibs
        libqprintf.a
        rpcmem.a
        )

    include_directories(
        ${incs}
        ${hexagonIncs}
        )

    set (EXTRA_LIBS ${EXTRA_LIBS} ${hexagonLibs})

    buildIDL(inc/pre_process.idl ${PROJECT_NAME} ${hexagonIncs})

    add_dependencies(${PROJECT_NAME} ${hexagonTargets})
    
    target_link_libraries(${PROJECT_NAME} ${EXTRA_LIBS})
endif()