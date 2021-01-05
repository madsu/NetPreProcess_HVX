#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <cstring>

#include "dlfcn.h"
#include "loadimg.h"
#include "net_pre_process.h"
#include "rpcmem.h"
#include "rgba2bgr.h"

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
using namespace cv;

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

/*
static inline size_t alignSize(size_t sz, int n)
{
    return (sz + n - 1) & -n;
}
*/

int main(int argc, char **argv)
{
    long long start_time, total_cycles;

    int width = 1920;
    int height = 1080;

    int outWidth = 481;
    int outHeight = 289;

    unsigned char *image;
    LoadYUV(argv[1], width, height, &image);

    int srcLen = sizeof(unsigned char) * width * height * 3 / 2;
    int dstLen = sizeof(unsigned char) * outWidth * outHeight * 3;
    unsigned char *dstImage = (unsigned char *)malloc(dstLen);

    nv12_pre_process(image, width, height, dstImage, outWidth, outHeight, 0);

    cv::Mat cpu_img(outHeight, outWidth, CV_8UC3, dstImage);
    cv::imwrite("/data/local/tmp/HVX_test/cpu_outimg.bmp", cpu_img);

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

#ifdef __hexagon__
    pre_process_vec_abs(testVec, 1024);

    //pre_process_nv12_hvx(dspSrcBuf, width, height, dspDstBuf, outWidth, outHeight, 0);
#else
    void* H = nullptr;
    using func_ptr = int (*)(const uint8 *, int, int, int, uint8 *, int, int, int, int);

    H = dlopen("libpre_process_stub.so", RTLD_NOW);
    if (!H) {
        printf("---ERROR, Failed to load libpre_process_stub.so\n");
        return -1;
    }

    func_ptr pre_process_nv12_ori = reinterpret_cast<func_ptr>(dlsym(H, "pre_process_nv12_ori"));
    if (!pre_process_nv12_ori)
    {
        printf("---ERROR, pre_process_nv12_hvx not found\n");
        dlclose(H);
        return -1;
    }

    func_ptr pre_process_nv12_hvx = reinterpret_cast<func_ptr>(dlsym(H, "pre_process_nv12_hvx"));
    if (!pre_process_nv12_hvx) {
        printf("---ERROR, pre_process_nv12_hvx not found\n");
        dlclose(H);
        return -1;
    }

    memset(dspDstBuf, 0, dstLen * sizeof(unsigned char));
    int n = 0;
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

    cv::Mat dsp_img(outHeight, outWidth, CV_8UC3, dspDstBuf);
    cv::imwrite("/data/local/tmp/HVX_test/dsp_outimg.bmp", dsp_img);

    unsigned char *dspDstBuf1 = nullptr;
    int outStride = alignSize(outWidth, 128) * 4;
    dstLen = sizeof(unsigned char) * outStride * outHeight;
    if (0 == (dspDstBuf1 = (unsigned char *)rpcmem_alloc(heapid, RPCMEM_DEFAULT_FLAGS, dstLen))) {
        printf("---Error: alloc dspSrcBuf failed\n");
        return -1;
    }

    {
        ccosttime a("pre_process_nv12_hvx");
        for (int i = 0; i < 10; ++i) {
            n = pre_process_nv12_hvx(dspSrcBuf, srcLen, width, height, dspDstBuf1, dstLen, outWidth, outHeight, 0);
            RGBA2BGR(dspDstBuf1, outWidth, outHeight, outStride, dspDstBuf);
        }
    }

    {
        ccosttime a("rgba2bgr");
        for (int i = 0; i < 10; ++i) {
            RGBA2BGR(dspDstBuf1, outWidth, outHeight, outStride, dspDstBuf);
        }
    }

    if (n != 0) {
        printf("nv12 hvx on DSP failed, err = %d\n", n);
    } else {
        printf("pre_process nv12 hvx succ!\n");
    }

    cv::Mat img(outHeight, outWidth, CV_8UC3, dspDstBuf);
    cv::imwrite("/data/local/tmp/HVX_test/hvx_outimg.bmp", img);

    dlclose(H);
#endif

FAIL:
    free(image);
    free(dstImage);

    if (dspSrcBuf)
        rpcmem_free(dspSrcBuf);
    if (dspDstBuf)
        rpcmem_free(dspDstBuf);
    if (dspDstBuf1)
        rpcmem_free(dspDstBuf1);

    rpcmem_deinit();
    return 0;
}