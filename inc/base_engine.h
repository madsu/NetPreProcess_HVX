#ifndef BASE_H_
#define BASE_H_

#include "CL/cl.hpp"
#include <vector>

// Interface visibility
#if defined _WIN32 || defined __CYGWIN__
#ifdef BUILDING_DLL
#ifdef __GNUC__
#define PUBLIC __attribute__((dllexport))
#else
#define PUBLIC __declspec(dllexport)
#endif
#else
#ifdef __GNUC__
#define PUBLIC __attribute__((dllimport))
#else
#define PUBLIC __declspec(dllimport)
#endif
#endif
#define LOCAL
#else
#if __GNUC__ >= 4
#define PUBLIC __attribute__((visibility("default")))
#define LOCAL __attribute__((visibility("hidden")))
#else
#define PUBLIC
#define LOCAL
#endif
#endif

extern "C"
{
    void OpenCLInit();
    void OpenCLUnInit();

    ////kernel
    cl_kernel GetOpenClKernel(std::string file_name, std::string kernel_name);
    void RunKernel(cl_kernel kernel, const std::vector<size_t> &gws);
    void ReleaseOpenCLKernel(cl_kernel kernel);

    ////buffer
    cl_mem CreateBuffer(cl_mem_flags flags, size_t size, void *host_ptr);
    void ReadBuffer(cl_mem buffer, void *ptr, size_t size);
}

#endif