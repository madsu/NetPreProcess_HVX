#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <cstring>

#include "dlfcn.h"
#include "loadimg.h"
#include "net_pre_process.h"
#include "rpcmem.h"

class ccosttime
{
public:
	ccosttime(const char *szInfo)
	{
		strcpy(m_szInfo, szInfo);
		start = std::chrono::system_clock::now();
	}
	~ccosttime()
	{
		end = std::chrono::system_clock::now();
		std::chrono::duration<double> elapsed = end - start;
		auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
		printf("%s Elapsed time: %lld milliseconds\n", m_szInfo, millis);
	}

private:
	char m_szInfo[1024];
	std::chrono::time_point<std::chrono::system_clock> start, end;
};

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

    int outWidth = 600;
    int outHeight = 360;

    unsigned char *image;
    LoadYUV(argv[1], width, height, &image);

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
        testVec[i] = i - 1024;
    }

#ifdef __hexagon__
    pre_process_vec_abs(testVec, 1024);

    //pre_process_nv12_hvx(dspSrcBuf, width, height, dspDstBuf, outWidth, outHeight, 0);
#else
    void* H = nullptr;
    int (*func_ptr)(int* test, int len, int64* result);
    int (*pre_process_vec_abs)(int *vec, int vecLen);
    int (*pre_process_nv12_ori)(const uint8 *pSrc, int pSrcLen, int srcWidth, int srcHeight, uint8 *pDst, int pDstLen, int dstWidth, int dstHeight, int rotate);
    int (*pre_process_nv12_hvx)(const uint8 *pSrc, int pSrcLen, int srcWidth, int srcHeight, uint8 *pDst, int pDstLen, int dstWidth, int dstHeight, int rotate);

    H = dlopen("libpre_process_stub.so", RTLD_NOW);
    if (!H) {
        printf("---ERROR, Failed to load libpre_process_stub.so\n");
        return -1;
    }

    pre_process_nv12_ori = (int (*)(const uint8 *, int, int, int, uint8 *, int, int, int, int))dlsym(H, "pre_process_nv12_ori");
    if (!pre_process_nv12_ori) {
        printf("---ERROR, pre_process_nv12_hvx not found\n");
        dlclose(H);
        return -1;
    }

    pre_process_nv12_hvx = (int (*)(const uint8 *, int, int, int, uint8 *, int, int, int, int))dlsym(H, "pre_process_nv12_hvx");
    if (!pre_process_nv12_hvx) {
        printf("---ERROR, pre_process_nv12_hvx not found\n");
        dlclose(H);
        return -1;
    }

    pre_process_vec_abs = (int (*)(int *, int))dlsym(H, "pre_process_vec_abs");
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
    int n = func_ptr(testVec, 1024, &result);
    if (n != 0) {
        printf("compute on DSP failed, err = %d\n", n);
    }
    else {
        printf("Sum = %lld\n", result);
    }

    n = pre_process_vec_abs(testVec, 1024);
    if (n != 0) {
        printf("compute on DSP failed, err = %d\n", n);
    }
    else {
        printf("vec abs succ: ");
        for (int i = 0; i < 20; ++i)
        {
            printf("%d ", testVec[i]);
        }
        printf("\n");
    }

    {
        ccosttime a("pre_process_nv12_ori");
        for (int i = 0; i < 10; ++i) {
            n = pre_process_nv12_ori(dspSrcBuf, srcLen, width, height, dspDstBuf, dstLen, outWidth, outHeight, 0);
        }
    }

    if (n != 0) {
        printf("nv12 ori on DSP failed, err = %d\n", n);
    } else {
        printf("pre_process nv12 ori succ!\n");
    }
    SaveBMP("./dsp_outimg.bmp", dspDstBuf, outWidth, outHeight, 24);

    for (int i = 0; i < srcLen; ++i) {
        dspSrcBuf[i] = 125;
    }

    {
        ccosttime a("pre_process_nv12_hvx");
        for (int i = 0; i < 1; ++i) {
            n = pre_process_nv12_hvx(dspSrcBuf, srcLen, width, height, dspDstBuf, dstLen, outWidth, outHeight, 0);
        }
    }

    if (n != 0) {
        printf("nv12 hvx on DSP failed, err = %d\n", n);
    } else {
        printf("pre_process nv12 hvx succ!\n");
    }

    printf("nv12 hvx out: ");
    unsigned short *ptr = (unsigned short *)dspDstBuf;
    unsigned int *ptrI = (unsigned int *)dspDstBuf;
    float* pFloat = (float*)dspDstBuf;
    for(int i = 0; i < 128 / sizeof(unsigned int); ++i) {
        printf("%d ", ptrI[i]);
        //printf("%d ", dspDstBuf[i]);
    }
    printf("\n");
    //SaveBMP("./dsp_hvx_outimg.bmp", dspDstBuf, outWidth, outHeight, 24);

    dlclose(H);
#endif

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