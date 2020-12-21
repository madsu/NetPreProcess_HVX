#ifndef BASE_H_
#define BASE_H_

#include <hexagon_types.h>
#include <hvx_hexagon_protos.h>

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

#define AL64 __attribute__((aligned(64)))

#ifdef _cplusplus
extern "C"
{
#endif

    void vec_abs(signed short AL64 *buf, unsigned int len);

    void pre_process_nv12_hvx(const unsigned char *pSrc, int srcWidth, int srcHeight,
                          unsigned char *pDst, int dstWidth, int dstHeight, int rotate);

#ifdef _cplusplus
}
#endif

#endif