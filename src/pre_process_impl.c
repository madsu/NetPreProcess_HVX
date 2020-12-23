#include <stdlib.h>
#include <math.h>

#include <hexagon_types.h>
#include <hvx_hexagon_protos.h>

#include "HAP_farf.h"
#include "pre_process.h"

int pre_process_sum(const int* vec, int vecLen, int64* res)
{
  int ii = 0;
  *res = 0;
  for (ii = 0; ii < vecLen; ++ii) {
    *res = *res + vec[ii];
  }
  FARF(RUNTIME_HIGH, "===============     DSP: sum result %lld ===============", *res);
  return 0;
}

int pre_process_vec_abs(short *buf, int len)
{
    /*
    HVX_Vector *vbuf = (HVX_Vector *)(buf);
    for (int i = 0; i < len / 32; i++)
    {
        vbuf[i] = Q6_Vh_vabs_Vh(vbuf[i]);
    }
    */

    for (int i = 0; i < len; i++)
    {
        buf[i] = buf[i] + 1024;
    }

    FARF(RUNTIME_HIGH, "===============     DSP: run vec_abs ===============");
    return 0;
}

int pre_process_nv12_hvx(uint8 *pSrc, int srcWidth, int srcHeight, uint8 *pDst, int dstWidth, int dstHeight, int rotate)
{
    for (int j = 0; j < dstHeight; ++j)
    {
        for (int i = 0; i < dstWidth; ++i)
        {
            int dx, dy;

            const unsigned char *ptrList[3];
            int stepList[3];
            unsigned char ret[3];

            stepList[0] = srcWidth;
            stepList[1] = srcWidth;
            stepList[2] = srcWidth;

            ptrList[0] = pSrc;                            //y
            ptrList[1] = pSrc + srcWidth * srcHeight;     //u
            ptrList[2] = pSrc + srcWidth * srcHeight + 1; //v

            float xratio = 0;
            float yratio = 0;

            if (rotate == 0 || rotate == 180)
            {
                xratio = (float)srcWidth / dstWidth;
                yratio = (float)srcHeight / dstHeight;
            }
            else
            {
                xratio = (float)srcWidth / dstHeight;
                yratio = (float)srcHeight / dstWidth;
            }

            //resize and rotato
            if (rotate == 0)
            {
                dx = i;
                dy = j;
            }
            else if (rotate == 90)
            {
                dx = dstHeight - j - 1;
                dy = i;
            }
            else if (rotate == 180)
            {
                dx = dstWidth - i - 1;
                dy = dstHeight - j - 1;
            }
            else
            {
                dx = j;
                dy = dstWidth - i - 1;
            }

            for (int chn = 0; chn < 3; chn++)
            {
                const unsigned char *ptr = ptrList[chn];
                int step = stepList[chn];

                float sx = (dx + 0.5f) * (float)xratio - 0.5f;
                float sy = (dy + 0.5f) * (float)yratio - 0.5f;

                int x = (int)floor(sx);
                int y = (int)floor(sy);

                float u = sx - x;
                float v = sy - y;

                if (x < 0)
                {
                    x = 0;
                    u = 0;
                }
                if (x >= srcWidth)
                {
                    x = srcWidth - 1;
                    u = 0;
                }
                if (y < 0)
                {
                    y = 0;
                    v = 0;
                }
                if (y >= srcHeight)
                {
                    y = srcHeight - 1;
                    v = 0;
                }

                float u1 = 1.0f - u;
                float v1 = 1.0f - v;

                int x_ = MIN(x + 1, srcWidth - 1);
                int y_ = MIN(y + 1, srcHeight - 1);
                if (chn != 0)
                {
                    x -= x % 2;
                    x_ -= x_ % 2;
                    y /= 2;
                    y_ /= 2;
                }

                unsigned char data00 = ptr[y * step + x];
                unsigned char data10 = ptr[y * step + x_];
                unsigned char data01 = ptr[y_ * step + x];
                unsigned char data11 = ptr[y_ * step + x_];

                float var = v1 * (u1 * data00 + u * data10) + v * (u1 * data01 + u * data11);
                if (var < 0)
                    var = 0;
                if (var > 255)
                    var = 255;

                ret[chn] = (unsigned char)var;
            }

            //nv21 -> bgr
            unsigned char y, u, v;
            int r, g, b;
            y = ret[0];
            u = ret[1];
            v = ret[2];

            r = y + (140 * (v - 128)) / 100;                         //r
            g = y - (34 * (u - 128)) / 100 - (71 * (v - 128)) / 100; //g
            b = y + (177 * (u - 128)) / 100;                         //b

            if (r > 255)
                r = 255;
            if (g > 255)
                g = 255;
            if (b > 255)
                b = 255;
            if (r < 0)
                r = 0;
            if (g < 0)
                g = 0;
            if (b < 0)
                b = 0;

            int dst_idx = j * dstWidth * 3 + i * 3;
            pDst[dst_idx] = (unsigned char)b;
            pDst[dst_idx + 1] = (unsigned char)g;
            pDst[dst_idx + 2] = (unsigned char)r;
        }
    }

    return 0;
}

/*
int pre_process_nv12_hvx(uint8 *pSrc, int srcWidth, int srcHeight, uint8 *pDst, int dstWidth, int dstHeight, int rotate)
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

    return 0;
}
*/