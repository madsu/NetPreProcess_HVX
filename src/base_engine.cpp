#include "base_engine.h"

void vec_abs(signed short AL64 *buf, unsigned int len)
{
    int i;
    HVX_Vector *vbuf = (HVX_Vector *)(buf);
    for (i = 0; i < len / 32; i++)
    {
        vbuf[i] = Q6_Vh_vabs_Vh(vbuf[i]);
    }
}

void nv13_pre_process_hvx(const unsigned char *pSrc, int srcWidth, int srcHeight,
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

    for(int j = 0; j < dstHeight; ++j) {
        
    }
}