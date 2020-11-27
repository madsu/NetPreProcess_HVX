#include "base.h"

#include <iostream>
#include <memory>

#define CHECK_RET(err, desc)            \
    if (err) {                         \
        std::cout << desc << std::endl; \
        return;                         \
    }

PUBLIC void OpenCLInit()
{
    cl_int err;
    cl_uint num_platforms;
    err = clGetPlatformIDs(0, nullptr, &num_platforms);
    if (err != CL_SUCCESS || num_platforms <= 0) {
        std::cout << "can't find available openCL platform!" << std::endl;
        return;
    }

    auto platforms = std::make_unique<cl_platform_id[]>(num_platforms);
    CHECK_RET(!platforms, "memory malloc error!");

    err = clGetPlatformIDs(num_platforms, platforms.get(), nullptr);
    if (err != CL_SUCCESS || num_platforms <= 0) {
        std::cout << "can't find available openCL platform!" << std::endl;
        return;
    }

    for (uint32_t i = 0; i < num_platforms; i++) {
        size_t size;
        err = clGetPlatformInfo(platforms[i], CL_PLATFORM_NAME, 0, nullptr, &size);
        CHECK_RET(err != CL_SUCCESS, "get platform name error!");

        auto platform_name = std::make_unique<char[]>(size);
        CHECK_RET(!platform_name, "memory malloc error!");

        err = clGetPlatformInfo(platforms[i], CL_PLATFORM_NAME, size, platform_name.get(), nullptr);
        CHECK_RET(err != CL_SUCCESS, "get platform name error!");

        std::cout << std::endl;
        std::cout << "CL_PLATFORM_NAME: " << platform_name.get() << std::endl;
    }
}

PUBLIC void OpenCLUnInit()
{

}
