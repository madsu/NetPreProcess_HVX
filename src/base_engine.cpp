#include "base_engine.h"
#include <cstdlib>
#include <cmath>
#include "qmath.h"

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
    const int scale = 1 << 22;
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
    int *sxAry = (int *)malloc((srcWidth + srcHeight) * sizeof(int));
    int *syAry = sxAry + srcWidth;

    int *fxAry = (int *)malloc((srcWidth + srcHeight) * sizeof(int));
    int *fyAry = fxAry + srcWidth;

    int *fxSubAry = (int *)malloc((srcWidth + srcHeight) * sizeof(int));
    int *fySubAry = fxAry + srcWidth;

    for (int dx = 0; dx < dstWidth; ++dx)
    {
        float fx = (float)((dx + 0.5) * xratio - 0.5);

        int x = (int)floor(fx);

        float u = fx - x;

        if (x < 0)
        {
            x = 0;
            u = 0.f;
        }
        else if (x >= srcWidth)
        {
            x = srcWidth - 1;
            u = 0.f;
        }

        sxAry[dx] = x;
        fxAry[dx] = (int)(u * scale);
        fxSubAry[dx] = scale - fxAry[dx];
    }

    for (int dy = 0; dy < dstHeight; ++dy)
    {
        float fy = (float)((dy + 0.5f) * yratio - 0.5f);
        int y = (int)floor(fy);

        float v = fy - y;

        if (y < 0)
        {
            y = 0;
            v = 0.f;
        }
        else if (y >= srcHeight)
        {
            y = srcHeight - 1;
            v = 0.f;
        }

        syAry[dy] = y;
        fyAry[dy] = (int)(v * scale);
        fySubAry[dy] = scale - fySubAry[dy];
    }

    unsigned char *buf = (unsigned char *)malloc(sizeof(unsigned char) * 128 * 4);
    unsigned char *pX0Y0 = buf;
    unsigned char *pX1Y0 = buf + 128;
    unsigned char *pX0Y1 = buf + 128 * 2;
    unsigned char *pX1Y1 = buf + 128 * 3;

    //每次算128个像素
    for(int dy = 0; dy < dstHeight; ++dy) {

        int ii = dstWidth / 128;
        for (int i = 0; i < ii; i++) {
            //load to buf;
            for (int dx = i * 128; dx < i * 128 + 128; ++dx) {
                pX0Y0[dx] = pSrc[syAry[dy] * srcWidth + sxAry[dx]];
                pX1Y0[dx] = pSrc[syAry[dy] * srcWidth + sxAry[dx] + 1];
                pX0Y1[dx] = pSrc[(syAry[dy] + 1) * srcWidth + sxAry[dx]];
                pX0Y1[dx] = pSrc[(syAry[dy] + 1) * srcWidth + (sxAry[dx] + 1)];
            }

            HVX_Vector *vX0Y0 = (HVX_Vector *)(pX0Y0);
            HVX_Vector *vX1Y0 = (HVX_Vector *)(pX1Y0);
            HVX_Vector *vX0Y1 = (HVX_Vector *)(pX0Y1);
            HVX_Vector *vX1Y1 = (HVX_Vector *)(pX1Y1);

            HVX_VectorPair pX0Y0h = Q6_Wuh_vzxt_Vub(vX0Y0[0]);
            HVX_VectorPair pX1Y0h = Q6_Wuh_vzxt_Vub(vX1Y0[0]);
            HVX_VectorPair pX0Y1h = Q6_Wuh_vzxt_Vub(vX0Y1[0]);
            HVX_VectorPair pX1Y1h = Q6_Wuh_vzxt_Vub(vX1Y1[0]);

            HVX_VectorPair pX0Y0wH = Q6_Wuw_vzxt_Vuh(Q6_V_hi_W(pX0Y0h));
            HVX_VectorPair pX0Y0wL = Q6_Wuw_vzxt_Vuh(Q6_V_lo_W(pX0Y0h));
            HVX_VectorPair pX1Y0wH = Q6_Wuw_vzxt_Vuh(Q6_V_hi_W(pX1Y0h));
            HVX_VectorPair pX1Y0wL = Q6_Wuw_vzxt_Vuh(Q6_V_lo_W(pX1Y0h));
            HVX_VectorPair pX0Y1wH = Q6_Wuw_vzxt_Vuh(Q6_V_hi_W(pX0Y1h));
            HVX_VectorPair pX0Y1wL = Q6_Wuw_vzxt_Vuh(Q6_V_lo_W(pX0Y1h));
            HVX_VectorPair pX1Y1wH = Q6_Wuw_vzxt_Vuh(Q6_V_hi_W(pX1Y1h));
            HVX_VectorPair pX1Y1wL = Q6_Wuw_vzxt_Vuh(Q6_V_lo_W(pX1Y1h));

            HVX_VectorPair *vU0 = (HVX_VectorPair *)(fxAry + i * 128);
            HVX_VectorPair *vV0 = (HVX_VectorPair *)(fyAry + i * 128);

            HVX_VectorPair *vU1 = (HVX_VectorPair *)(fxSubAry + i * 128);
            HVX_VectorPair *vV1 = (HVX_VectorPair *)(fySubAry + i * 128);

        }
    }
}