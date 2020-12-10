#ifndef TNN_SOURCE_TNN_DEVICE_OPENCL_OPENCL_WRAPPER_H_
#define TNN_SOURCE_TNN_DEVICE_OPENCL_OPENCL_WRAPPER_H_

#include <memory>

//support opencl min version is 1.1
#ifndef CL_TARGET_OPENCL_VERSION
#define CL_TARGET_OPENCL_VERSION 200
#endif
#ifndef CL_HPP_TARGET_OPENCL_VERSION
#define CL_HPP_TARGET_OPENCL_VERSION 110
#endif
#ifndef CL_HPP_MINIMUM_OPENCL_VERSION
#define CL_HPP_MINIMUM_OPENCL_VERSION 110
#endif

#include "CL/cl.hpp"
#include <iostream>

#define CHECK_NOTNULL(X)                                \
    if (X == NULL)                                      \
    {                                                   \
        std::cout << "OpenCL API is null" << std::endl; \
    }

#define CHECK_CL_SUCCESS(error)                                    \
    if (error != CL_SUCCESS)                                       \
    {                                                              \
        std::cout << "OpenCL ERROR CODE : " << error << std::endl; \
    }

// OpenCLSymbols is a opencl function wrapper.
// if device not support opencl or opencl target function, 
// app will not crash and can get error code.
class OpenCLSymbols {
public:
    static OpenCLSymbols *GetInstance();

    ~OpenCLSymbols();
    OpenCLSymbols(const OpenCLSymbols &) = delete;
    OpenCLSymbols &operator=(const OpenCLSymbols &) = delete;

    bool LoadOpenCLLibrary();
    bool UnLoadOpenCLLibrary();
    //get platfrom id 
    using clGetPlatformIDsFunc  = cl_int (*)(cl_uint, cl_platform_id *,
                                            cl_uint *);
    //get platform info
    using clGetPlatformInfoFunc = cl_int (*)(cl_platform_id, cl_platform_info,
                                             size_t, void *, size_t *);
    // build program
    using clBuildProgramFunc =
        cl_int (*)(cl_program, cl_uint, const cl_device_id *, const char *,
                   void (*pfn_notify)(cl_program, void *), void *);
    //enqueue run kernel
    using clEnqueueNDRangeKernelFunc  = cl_int (*)(cl_command_queue, cl_kernel,
                                                cl_uint, const size_t *,
                                                const size_t *,
                                                const size_t *, cl_uint,
                                                const cl_event *, cl_event *);
    //set kernel parameter
    using clSetKernelArgFunc          = cl_int (*)(cl_kernel, cl_uint, size_t,
                                            const void *);
    using clRetainMemObjectFunc       = cl_int (*)(cl_mem);
    using clReleaseMemObjectFunc      = cl_int (*)(cl_mem);
    using clEnqueueUnmapMemObjectFunc = cl_int (*)(cl_command_queue, cl_mem,
                                                    void *, cl_uint,
                                                    const cl_event *,
                                                    cl_event *);
    using clRetainCommandQueueFunc = cl_int (*)(cl_command_queue command_queue);
    //create context
    using clCreateContextFunc      = cl_context (*)(
        const cl_context_properties *, cl_uint, const cl_device_id *,
        void(CL_CALLBACK *)(  // NOLINT(readability/casting)
            const char *, const void *, size_t, void *),
        void *, cl_int *);
    using clEnqueueCopyImageFunc = cl_int (*)(cl_command_queue, cl_mem, cl_mem,
                                            const size_t *, const size_t *,
                                            const size_t *, cl_uint,
                                            const cl_event *, cl_event *);

    using clCreateContextFromTypeFunc =
        cl_context (*)(const cl_context_properties *, cl_device_type,
                        void(CL_CALLBACK *)(  // NOLINT(readability/casting)
                        const char *, const void *, size_t, void *),
                        void *, cl_int *);
    using clReleaseContextFunc      = cl_int (*)(cl_context);
    using clWaitForEventsFunc       = cl_int (*)(cl_uint, const cl_event *);
    using clReleaseEventFunc        = cl_int (*)(cl_event);
    using clEnqueueWriteBufferFunc  = cl_int (*)(cl_command_queue, cl_mem,
                                                cl_bool, size_t, size_t,
                                                const void *, cl_uint,
                                                const cl_event *, cl_event *);
    using clEnqueueReadBufferFunc   = cl_int (*)(cl_command_queue, cl_mem,
                                                cl_bool, size_t, size_t, void *,
                                                cl_uint, const cl_event *,
                                                cl_event *);
    using clGetProgramBuildInfoFunc = cl_int (*)(cl_program, cl_device_id,
                                                cl_program_build_info, size_t,
                                                void *, size_t *);
    using clRetainProgramFunc       = cl_int (*)(cl_program program);
    using clEnqueueMapBufferFunc = void *(*)(cl_command_queue, cl_mem, cl_bool,
                                            cl_map_flags, size_t, size_t,
                                            cl_uint, const cl_event *,
                                            cl_event *, cl_int *);
    using clEnqueueMapImageFunc  = void *(*)(cl_command_queue, cl_mem, cl_bool,
                                            cl_map_flags, const size_t *,
                                            const size_t *, size_t *, size_t *,
                                            cl_uint, const cl_event *,
                                            cl_event *, cl_int *);
    using clCreateCommandQueueFunc = cl_command_queue(CL_API_CALL *)(  // NOLINT
        cl_context, cl_device_id, cl_command_queue_properties, cl_int *);
    using clGetCommandQueueInfoFunc     = cl_int (*)(cl_command_queue,
                                                cl_command_queue_info, size_t,
                                                void *, size_t *);
    using clReleaseCommandQueueFunc     = cl_int (*)(cl_command_queue);
    using clCreateProgramWithBinaryFunc = cl_program (*)(cl_context, cl_uint,
                                                        const cl_device_id *,
                                                        const size_t *,
                                                        const unsigned char **,
                                                        cl_int *, cl_int *);
    using clRetainContextFunc           = cl_int (*)(cl_context context);
    using clGetContextInfoFunc = cl_int (*)(cl_context, cl_context_info, size_t,
                                            void *, size_t *);
    using clReleaseProgramFunc = cl_int (*)(cl_program program);
    //flush command queue to target device
    using clFlushFunc          = cl_int (*)(cl_command_queue command_queue);
    //sync device command queue
    using clFinishFunc         = cl_int (*)(cl_command_queue command_queue);
    using clGetProgramInfoFunc = cl_int (*)(cl_program, cl_program_info, size_t,
                                            void *, size_t *);
    //create kernel with kernel name
    using clCreateKernelFunc   = cl_kernel (*)(cl_program, const char *,
                                            cl_int *);
    using clRetainKernelFunc   = cl_int (*)(cl_kernel kernel);
    using clCreateBufferFunc   = cl_mem (*)(cl_context, cl_mem_flags, size_t,
                                            void *, cl_int *);
    //create image 2d
    using clCreateImage2DFunc  = cl_mem(CL_API_CALL *)(cl_context,  // NOLINT
                                                    cl_mem_flags,
                                                    const cl_image_format *,
                                                    size_t, size_t, size_t,
                                                    void *, cl_int *);
    //create image 3d
    using clCreateImage3DFunc  = cl_mem(CL_API_CALL *)(cl_context,  // NOLINT
                                                    cl_mem_flags,
                                                    const cl_image_format *,
                                                    size_t, size_t, size_t,
                                                    size_t, size_t, void *,
                                                    cl_int *);
    //crete program with source code
    using clCreateProgramWithSourceFunc = cl_program (*)(cl_context, cl_uint,
                                                        const char **,
                                                        const size_t *,
                                                        cl_int *);
    using clReleaseKernelFunc           = cl_int (*)(cl_kernel kernel);
    using clGetDeviceInfoFunc = cl_int (*)(cl_device_id, cl_device_info, size_t,
                                           void *, size_t *);
    //get device id, two device have different device id
    using clGetDeviceIDsFunc  = cl_int (*)(cl_platform_id, cl_device_type,
                                        cl_uint, cl_device_id *, cl_uint *);
    using clRetainEventFunc   = cl_int (*)(cl_event);
    using clGetKernelWorkGroupInfoFunc = cl_int (*)(cl_kernel, cl_device_id,
                                                    cl_kernel_work_group_info,
                                                    size_t, void *, size_t *);
    using clGetEventInfoFunc           = cl_int (*)(cl_event event,
                                            cl_event_info param_name,
                                            size_t param_value_size,
                                            void *param_value,
                                            size_t *param_value_size_ret);
    using clGetEventProfilingInfoFunc  = cl_int (*)(
        cl_event event, cl_profiling_info param_name, size_t param_value_size,
        void *param_value, size_t *param_value_size_ret);
    using clGetImageInfoFunc = cl_int (*)(cl_mem, cl_image_info, size_t, void *,
                                          size_t *);
    using clEnqueueCopyBufferToImageFunc = cl_int(CL_API_CALL *)(
        cl_command_queue, cl_mem, cl_mem, size_t, const size_t *,
        const size_t *, cl_uint, const cl_event *, cl_event *);
    using clEnqueueCopyImageToBufferFunc = cl_int(CL_API_CALL *)(
        cl_command_queue, cl_mem, cl_mem, const size_t *, const size_t *,
        size_t, cl_uint, const cl_event *, cl_event *);
#if CL_HPP_TARGET_OPENCL_VERSION >= 120
    using clRetainDeviceFunc        = cl_int (*)(cl_device_id);
    using clReleaseDeviceFunc       = cl_int (*)(cl_device_id);
    using clCreateImageFunc         = cl_mem (*)(cl_context, cl_mem_flags,
                                        const cl_image_format *,
                                        const cl_image_desc *, void *,
                                        cl_int *);
#endif
#if CL_HPP_TARGET_OPENCL_VERSION >= 200
    //opencl 2.0 can get sub group info and wave size.
    using clGetKernelSubGroupInfoKHRFunc = cl_int(CL_API_CALL *)(
        cl_kernel, cl_device_id, cl_kernel_sub_group_info, size_t, const void  *, size_t, void *, size_t *);
    using clCreateCommandQueueWithPropertiesFunc = cl_command_queue(CL_API_CALL *)(
        cl_context, cl_device_id, const cl_queue_properties *, cl_int *);
    using clGetExtensionFunctionAddressFunc = void * (CL_API_CALL *)(const char *);
#endif

#define TNN_CL_DEFINE_FUNC_PTR(func) func##Func func = nullptr

    TNN_CL_DEFINE_FUNC_PTR(clGetPlatformIDs);
    TNN_CL_DEFINE_FUNC_PTR(clGetPlatformInfo);
    TNN_CL_DEFINE_FUNC_PTR(clBuildProgram);
    TNN_CL_DEFINE_FUNC_PTR(clEnqueueNDRangeKernel);
    TNN_CL_DEFINE_FUNC_PTR(clSetKernelArg);
    TNN_CL_DEFINE_FUNC_PTR(clReleaseKernel);
    TNN_CL_DEFINE_FUNC_PTR(clCreateProgramWithSource);
    TNN_CL_DEFINE_FUNC_PTR(clCreateBuffer);
    TNN_CL_DEFINE_FUNC_PTR(clCreateImage2D);
    TNN_CL_DEFINE_FUNC_PTR(clCreateImage3D);
    TNN_CL_DEFINE_FUNC_PTR(clRetainKernel);
    TNN_CL_DEFINE_FUNC_PTR(clCreateKernel);
    TNN_CL_DEFINE_FUNC_PTR(clGetProgramInfo);
    TNN_CL_DEFINE_FUNC_PTR(clFlush);
    TNN_CL_DEFINE_FUNC_PTR(clFinish);
    TNN_CL_DEFINE_FUNC_PTR(clReleaseProgram);
    TNN_CL_DEFINE_FUNC_PTR(clRetainContext);
    TNN_CL_DEFINE_FUNC_PTR(clGetContextInfo);
    TNN_CL_DEFINE_FUNC_PTR(clCreateProgramWithBinary);
    TNN_CL_DEFINE_FUNC_PTR(clCreateCommandQueue);
    TNN_CL_DEFINE_FUNC_PTR(clGetCommandQueueInfo);
    TNN_CL_DEFINE_FUNC_PTR(clReleaseCommandQueue);
    TNN_CL_DEFINE_FUNC_PTR(clEnqueueMapBuffer);
    TNN_CL_DEFINE_FUNC_PTR(clEnqueueMapImage);
    TNN_CL_DEFINE_FUNC_PTR(clEnqueueCopyImage);
    TNN_CL_DEFINE_FUNC_PTR(clRetainProgram);
    TNN_CL_DEFINE_FUNC_PTR(clGetProgramBuildInfo);
    TNN_CL_DEFINE_FUNC_PTR(clEnqueueReadBuffer);
    TNN_CL_DEFINE_FUNC_PTR(clEnqueueWriteBuffer);
    TNN_CL_DEFINE_FUNC_PTR(clWaitForEvents);
    TNN_CL_DEFINE_FUNC_PTR(clReleaseEvent);
    TNN_CL_DEFINE_FUNC_PTR(clCreateContext);
    TNN_CL_DEFINE_FUNC_PTR(clCreateContextFromType);
    TNN_CL_DEFINE_FUNC_PTR(clReleaseContext);
    TNN_CL_DEFINE_FUNC_PTR(clRetainCommandQueue);
    TNN_CL_DEFINE_FUNC_PTR(clEnqueueUnmapMemObject);
    TNN_CL_DEFINE_FUNC_PTR(clRetainMemObject);
    TNN_CL_DEFINE_FUNC_PTR(clReleaseMemObject);
    TNN_CL_DEFINE_FUNC_PTR(clGetDeviceInfo);
    TNN_CL_DEFINE_FUNC_PTR(clGetDeviceIDs);
    TNN_CL_DEFINE_FUNC_PTR(clRetainEvent);
    TNN_CL_DEFINE_FUNC_PTR(clGetKernelWorkGroupInfo);
    TNN_CL_DEFINE_FUNC_PTR(clGetEventInfo);
    TNN_CL_DEFINE_FUNC_PTR(clGetEventProfilingInfo);
    TNN_CL_DEFINE_FUNC_PTR(clGetImageInfo);
    TNN_CL_DEFINE_FUNC_PTR(clEnqueueCopyBufferToImage);
    TNN_CL_DEFINE_FUNC_PTR(clEnqueueCopyImageToBuffer);
#if CL_HPP_TARGET_OPENCL_VERSION >= 120
    TNN_CL_DEFINE_FUNC_PTR(clRetainDevice);
    TNN_CL_DEFINE_FUNC_PTR(clReleaseDevice);
    TNN_CL_DEFINE_FUNC_PTR(clCreateImage);
#endif
#if CL_HPP_TARGET_OPENCL_VERSION >= 200
    TNN_CL_DEFINE_FUNC_PTR(clGetKernelSubGroupInfoKHR);
    TNN_CL_DEFINE_FUNC_PTR(clCreateCommandQueueWithProperties);
    TNN_CL_DEFINE_FUNC_PTR(clGetExtensionFunctionAddress);
#endif

#undef TNN_CL_DEFINE_FUNC_PTR

private:
    OpenCLSymbols();
    bool LoadLibraryFromPath(const std::string &path);

private:
    static std::shared_ptr<OpenCLSymbols> opencl_symbols_singleton_;
    void *handle_ = nullptr;
};

#endif