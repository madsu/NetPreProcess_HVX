#ifndef BASE_H_
#define BASE_H_

#include "CL/cl.hpp"

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
    void OpenCLRun(std::string file_name, std::string kernel_name);
    void OpenCLUnInit();
}

#endif