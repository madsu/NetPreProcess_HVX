#include "base_engine.h"

void vec_abs(short *buf, int len)
{
    int i;
    HVX_Vector *vbuf = (HVX_Vector *)(buf);
    for (i = 0; i < len / 32; i++)
    {
        vbuf[i] = Q6_Vh_vabs_Vh(vbuf[i]);
    }
}

void pre_process_nv12_hvx(const unsigned char *pSrc, int srcWidth, int srcHeight,
                          unsigned char *pDst, int dstWidth, int dstHeight, int rotate)
{
    float xratio = 0;
    float yratio = 0;

    if (rotate == 0 || rotate == 180) {
        xratio = (float)srcWidth / dstWidth;
        yratio = (float)srcHeight / dstHeight;
    }
    else {
        xratio = (float)srcWidth / dstHeight;
        yratio = (float)srcHeight / dstWidth;
    }

    //计算xy在原图上的坐标
    int *sx0 = 0; //(int *)malloc(srcWidth + srcHeight + srcWidth + srcHeight);
    int *sy0 = sx0 + srcWidth;
    int *sx1 = sy0 + srcHeight;
    int *sy1 = sx1 + srcWidth;

    float fx;
    float fy;

    for (int dx = 0; dx < dstWidth; ++dx)
    {
        fx = (float)((dx + 0.5) * xratio - 0.5);

        int x = fx; //(int)floor(fx);

        float u0 = fx - x;

        if(x < 0) {
            x = 0;
            u0 = 0.f;
        }
        else if(x >= srcWidth) {
            x = srcWidth - 1;
            u0 = 0.f;
        }

        float u1 = 1.f - u0;
    }

    for(int j = 0; j < dstHeight; ++j) {
        
    }
}