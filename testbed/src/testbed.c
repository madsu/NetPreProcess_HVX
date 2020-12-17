
#include <stdio.h>
#include "loadimg.h"

#if defined(__hexagon__)
#include "base_engine.h"
#include "hexagon_standalone.h"
#include "subsys.h"
#include "io.h"
#include "hvx.cfg.h"
#endif

#include "net_pre_process.h"

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

    int dstLen = sizeof(unsigned char) * outWidth * outHeight * 3;
    unsigned char *dstImage = (unsigned char *)malloc(dstLen);

    nv12_pre_process(image, width, height, dstImage, outWidth, outHeight, 0);

    SaveBMP("./cpu_outimg.bmp", dstImage, outWidth, outHeight, 24);

#if defined(__hexagon__)
    subsys_enable();
    SIM_ACQUIRE_HVX;
#if LOG2VLEN == 7
    SIM_SET_HVX_DOUBLE_MODE;
#endif
    /* -----------------------------------------------------*/
    /*  Call fuction                                        */
    /* -----------------------------------------------------*/
    RESET_PMU();
    start_time = READ_PCYCLES();

    //vec_abs(vec_buf, len);

    total_cycles = READ_PCYCLES() - start_time;
    DUMP_PMU();

    printf("dsp run cycles %lld\n", total_cycles);
#endif

    free(image);
    free(dstImage);

    return 0;
}