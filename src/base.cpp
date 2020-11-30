#include "base.h"

#include <iostream>
#include <fstream>
#include <memory>
#include <map>

#define CHECK_RET(err, desc)            \
    if (err) {                         \
        std::cout << desc << std::endl; \
        return;                         \
    }

cl_context context;

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

    std::map<int, std::string> platform_info = {
        {CL_PLATFORM_NAME, "CL_PLATFORM_NAME"},
        {CL_PLATFORM_VENDOR, "CL_PLATFORM_VENDOR"},
        {CL_PLATFORM_VERSION, "CL_PLATFORM_VERSION"},
        {CL_PLATFORM_PROFILE, "CL_PLATFORM_PROFILE"},
        {CL_PLATFORM_EXTENSIONS, "CL_PLATFORM_EXTENSIONS"}
    };

    for (uint32_t i = 0; i < num_platforms; i++) {
        for (const auto& [info_type, info_desc] : platform_info) {
            size_t size;
            err = clGetPlatformInfo(platforms[i], info_type, 0, nullptr, &size);
            CHECK_RET(err != CL_SUCCESS, "get platform name error!");

            auto name = std::make_unique<char[]>(size);
            CHECK_RET(!name, "memory malloc error!");

            err = clGetPlatformInfo(platforms[i], info_type, size, name.get(), nullptr);
            CHECK_RET(err != CL_SUCCESS, "get platform name error!");

            std::cout << std::endl
                      << info_desc << ": " << name.get() << std::endl;
        }
    }

    cl_uint num_devices;
    err = clGetDeviceIDs(platforms[0], CL_DEVICE_TYPE_GPU, 0, nullptr, &num_devices);

    auto devices = std::make_unique<cl_device_id[]>(num_devices);
    err = clGetDeviceIDs(platforms[0], CL_DEVICE_TYPE_GPU, num_devices, devices.get(), 0);

    std::map<int, std::string> device_info = {
        {CL_DEVICE_VENDOR_ID, "CL_DEVICE_VENDOR_ID"},
        {CL_DEVICE_MAX_COMPUTE_UNITS, "CL_DEVICE_MAX_COMPUTE_UNITS"},
        {CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS, "CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS"},
        {CL_DEVICE_MAX_CLOCK_FREQUENCY, "CL_DEVICE_MAX_CLOCK_FREQUENCY"},
        {CL_DEVICE_GLOBAL_MEM_SIZE, "CL_DEVICE_GLOBAL_MEM_SIZE"},
        {CL_DEVICE_GLOBAL_MEM_CACHELINE_SIZE, "CL_DEVICE_GLOBAL_MEM_CACHELINE_SIZE"}
    };

    for(uint32_t i = 0; i < num_devices; i++) {
        for(const auto& [info_type, info_desc] : device_info) {
            size_t size;
            err = clGetDeviceInfo(devices[i], info_type, 0, nullptr, &size);
            CHECK_RET(err != CL_SUCCESS, "get platform name error!");

            auto name = std::make_unique<char[]>(size);
            CHECK_RET(!name, "memory malloc error!");

            err = clGetDeviceInfo(devices[i], info_type, size, name.get(), nullptr);
            CHECK_RET(err != CL_SUCCESS, "get platform name error!");

            std::cout << std::endl
                      << info_desc << ": " << *(cl_uint *)(name.get()) << std::endl;
        }
    }

    cl_context_properties properties[] = {
        CL_CONTEXT_PLATFORM, (cl_context_properties)platforms[0], 0
    };

    context = clCreateContext(properties, num_devices, devices.get(), nullptr, nullptr, &err);
    CHECK_RET(err != CL_SUCCESS, "create context failed!");

}

std::string ReadFileContext(const std::string file_name)
{
    std::fstream file(file_name, std::ios::in);
    std::string context("");
    if (file.is_open()) {
        while (file.good()) {
            std::string line;
            std::getline(file, line);
            context.append(line);
        }
    }

    return context;
}

PUBLIC void OpenCLRun(std::string file_name, std::string kernel_name)
{
    //read source file
    std::string text = ReadFileContext(file_name);
    CHECK_RET(text.empty(), "read file error!");

    const char* raw_text = text.c_str();
    const size_t raw_text_len = text.length();

    cl_int err;
    cl_program program = clCreateProgramWithSource(context, 1, &raw_text, &raw_text_len, &err);
    CHECK_RET(err != CL_SUCCESS, "create program failed!");

    err = clBuildProgram(program, 0, nullptr, nullptr, nullptr, nullptr);
    CHECK_RET(err != CL_SUCCESS, "build program failed!");

    cl_kernel kernel = clCreateKernel(program, kernel_name.c_str(), &err);
    CHECK_RET(err != CL_SUCCESS, "create kernel failed!");

    ////input
    

}

PUBLIC void OpenCLUnInit()
{
    clReleaseContext(context);
}
