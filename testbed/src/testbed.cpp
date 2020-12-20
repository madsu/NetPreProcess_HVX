
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include "loadimg.h"
#include "net_pre_process.h"
#include "rpcmem.h"

#if defined(__hexagon__)
#include "base_engine.h"
#include "hexagon_standalone.h"
#include "subsys.h"
#include "io.h"
#include "hvx.cfg.h"
#endif

void PrintVec(short *vec, unsigned int len)
{
    printf("print vec: ");
    for (int i = 0; i < 10; ++i) {
        printf("%d ", vec[i]);
    }
    printf("\n");
}

int main(int argc, char **argv)
{
    long long start_time, total_cycles;

    int width = 1920;
    int height = 1080;

    int outWidth = 640;
    int outHeight = 360;

    unsigned char *image;
    LoadYUV("0.nv21", width, height, &image);

    int srcLen = sizeof(unsigned char) * width * height * 3 / 2;
    int dstLen = sizeof(unsigned char) * outWidth * outHeight * 3;
    unsigned char *dstImage = (unsigned char *)malloc(dstLen);

    nv12_pre_process(image, width, height, dstImage, outWidth, outHeight, 0);

    SaveBMP("./cpu_outimg.bmp", dstImage, outWidth, outHeight, 24);

    rpcmem_init();

    int heapid = RPCMEM_HEAP_ID_SYSTEM;
#if defined(SLPI) || defined(MDSP)
    heapid = RPCMEM_HEAP_ID_CONTIG;
#endif

    unsigned char* dspSrcBuf = nullptr;
    if (0 == (dspSrcBuf = (unsigned char *)rpcmem_alloc(heapid, RPCMEM_DEFAULT_FLAGS, srcLen))) {
        printf("---Error: alloc dspSrcBuf failed\n");
        return -1;
    }

    unsigned char *dspDstBuf = nullptr;
    if (0 == (dspSrcBuf = (unsigned char *)rpcmem_alloc(heapid, RPCMEM_DEFAULT_FLAGS, dstLen))) {
        printf("---Error: alloc dspSrcBuf failed\n");
        return -1;
    }

#ifdef __hexagon__
#else
    void* H = nullptr;
    int (*pre_process_nv12_hvx)(unsigned char *pSrc, int srcWidth, int srcHeight, unsigned char *pDst, int dstWidth, int dstHeight, int rotate);

    H = dlopen("libnetprocess.so", RTLD_NOW);
    if (!H) {
        printf("---ERROR, Failed to load libnetprocess.so\n");
        return -1;
    }

    pre_process_nv12_hvx = (int (*)(unsigned char *, int, int, unsigned char *, int, int, int))dlsym(H, "pre_process_nv12_hvx");
    if (!pre_process_nv12_hvx) {
        printf("---ERROR, pre_process_nv12_hvx not found\n");
        dlclose(H);
        return -1;
    }

    pre_process_nv12_hvx(dspSrcBuf, width, height, dspDstBuf, outWidth, outHeight, 0);
    dlclose(H);
#endif

FAIL:
    free(image);
    free(dstImage);

    if (dspSrcBuf)
        rpcmem_free(dspSrcBuf);
    if (dspDstBuf)
        rpcmem_free(dspDstBuf);

    rpcmem_deinit();
    return 0;
}