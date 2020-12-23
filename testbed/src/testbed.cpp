
#include <stdio.h>
#include <stdlib.h>
#include "dlfcn.h"
#include "loadimg.h"
#include "net_pre_process.h"
#include "rpcmem.h"

extern "C"
{
    int pre_process_vec_abs(short *buf, int len);
}

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

    for(int i = 0; i < srcLen; ++i) {
        dspSrcBuf[i] = image[i];
    }

    unsigned char *dspDstBuf = nullptr;
    if (0 == (dspDstBuf = (unsigned char *)rpcmem_alloc(heapid, RPCMEM_DEFAULT_FLAGS, dstLen))) {
        printf("---Error: alloc dspSrcBuf failed\n");
        return -1;
    }

    int *testVec = (int *)rpcmem_alloc(heapid, RPCMEM_DEFAULT_FLAGS, sizeof(int) * 1024);
    for (int i = 0; i < 1024; ++i) {
        testVec[i] = i;
    }

#ifdef __hexagon__
    pre_process_vec_abs(testVec, 1024);

    //pre_process_nv12_hvx(dspSrcBuf, width, height, dspDstBuf, outWidth, outHeight, 0);
#else
    void* H = nullptr;
    int (*func_ptr)(int* test, int len, int64* result);
    int (*pre_process_nv12_hvx)(unsigned char *pSrc, int srcWidth, int srcHeight, unsigned char *pDst, int dstWidth, int dstHeight, int rotate);
    int (*pre_process_vec_abs)(short *buf, int len);
    H = dlopen("libpre_process_stub.so", RTLD_NOW);
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

    pre_process_vec_abs = (int (*)(short *, int))dlsym(H, "pre_process_vec_abs");
    if (!pre_process_vec_abs) {
        printf("---ERROR, pre_process_vec_abs not found\n");
        dlclose(H);
        return -1;
    }
    else {
        printf("get pre_process_vec_abs succ!\n");
    }

    func_ptr = (int (*)(int*, int, int64*))dlsym(H, "pre_process_sum");
    if(!func_ptr) {
        printf("---ERROR, pre_process_sum not found\n");
        dlclose(H);
        return -1;
    }
    else {
        printf("get pre_process_sum succ!\n");
    }
    printf("compute on DSP\n");
    int64 result = 0;
    int n = (*func_ptr)(testVec, 10, &result);
    if (n != 0) {
        printf("compute on DSP failed, err = %d\n", n);
    }
    else {
        printf("Sum = %lld\n", result);
    }

    n = pre_process_nv12_hvx(dspSrcBuf, width, height, dspDstBuf, outWidth, outHeight, 0);
    if (n != 0) {
        printf("nv12 on DSP failed, err = %d\n", n);
    }
    else {
        printf("pre_process succ!");
    }

    dlclose(H);

   /*
   int retVal = 0;
   remote_handle64 handle = -1;
   char* preprocess_URI_Domain = pre_process_URI "&_dom=cdsp";
   printf("domain=%s\n", preprocess_URI_Domain);
   retVal = pre_process_open(preprocess_URI_Domain, &handle);
   if(retVal) {
       printf("unable to create fastrpc session on CDSP: err=%d\n", retVal);
   }

   pre_process_vec_abs(handle, testVec, 1024);
    
   pre_process_close(handle);
   */
#endif
    SaveBMP("./dsp_outimg.bmp", dspDstBuf, outWidth, outHeight, 24);
    printf("vec: \n");
    for (int i = 0; i < 20; ++i) {
        printf("%d ", testVec[i]);
    }

FAIL:
    free(image);
    free(dstImage);

    if (dspSrcBuf)
        rpcmem_free(dspSrcBuf);
    if (dspDstBuf)
        rpcmem_free(dspDstBuf);
    if(testVec)
        rpcmem_free(testVec);

    rpcmem_deinit();
    return 0;
}