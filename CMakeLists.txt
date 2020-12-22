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

option(ENABLE_HVX "hvx compute support" OFF)

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

    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIE -llog")
    SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -fPIE -llog")
    SET(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS}  -fPIE -pie -llog")

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
        ${CMAKE_CURRENT_SOURCE_DIR}/src/
        ${CMAKE_CURRENT_BINARY_DIR}/
        )

    prepare_libraries_hexagon(hexagonTargets_1 hexagonIncs_1 hexagonLibs_1 hexagonSharedLibs_1
        adsprpc
        )

    message(STATUS "hexagonTarget:${hexagonTargets_1}")
    message(STATUS "hexagonLibs:${hexagonLibs_1}")
    message(STATUS "hexagonIncs:${hexagonIncs_1}")

    buildIDL(inc/pre_process.idl ${LIB_NAME})

    include_directories(
        ${incs}
        ${hexagonIncs_1}
        )

    add_dependencies(${LIB_NAME} ${hexagonTargets_1})

    target_link_libraries(${LIB_NAME} ${hexagonLibs_1})

    prepare_libraries_hexagon(hexagonTargets_2 hexagonIncs_2 hexagonLibs_2 hexagonSharedLibs_2
        rpcmem.a
        )

    include_directories(
        ${hexagonIncs_2}
        )

    set (EXTRA_LIBS ${EXTRA_LIBS} ${hexagonLibs_2})
    set (EXTRA_LIBS ${EXTRA_LIBS} ${LIB_NAME})

    add_dependencies(${PROJECT_NAME} ${LIB_NAME})

    target_link_libraries( ${PROJECT_NAME} ${EXTRA_LIBS} m )

elseif(ENABLE_HVX)
    aux_source_directory(${CMAKE_SOURCE_DIR}/src LIB_SRCS)
    set(LIB_NAME pre_process_skel)
    add_library(${LIB_NAME}
            SHARED 
            ${CMAKE_CURRENT_BINARY_DIR}/pre_process_skel
            ${LIB_SRCS}
    )

    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fexceptions")

    set(incs 
        ${CMAKE_CURRENT_SOURCE_DIR}/src/
        ${CMAKE_CURRENT_BINARY_DIR}/
        ${HEXAGON_SDK_ROOT}/incs/HAP/
        #${HEXAGON_TOOLCHAIN_ROOT}/Tools/target/hexagon/include/
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
endif()