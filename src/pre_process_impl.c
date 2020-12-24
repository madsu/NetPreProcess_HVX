#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include <hexagon_types.h>
#include <hvx_hexagon_protos.h>

#include "HAP_farf.h"
#include "hvx.cfg.h"

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

int pre_process_vec_abs(int* vec, int vecLen)
{
    HVX_Vector *vbuf = (HVX_Vector *)(vec);
    for (int i = 0; i < vecLen / 32; i++) {
        vbuf[i] = Q6_Vw_vabs_Vw(vbuf[i]);
    }

    FARF(RUNTIME_HIGH, "===============     DSP: run vec_abs ===============");
    return 0;
}

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))
int pre_process_nv12_ori(const uint8* pSrc, int pSrcLen, int srcWidth, int srcHeight, uint8* pDst, int pDstLen, int dstWidth, int dstHeight, int rotate)
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

int pre_process_nv12_hvx(const uint8 *pSrc, int pSrcLen, int srcWidth, int srcHeight, uint8 *pDst, int pDstLen, int dstWidth, int dstHeight, int rotate)
{
    const int scale = 1 << 8;
    float xratio = 0;
    float yratio = 0;

    if (rotate == 0 || rotate == 180) {
        xratio = (float)srcWidth / dstWidth;
        yratio = (float)srcHeight / dstHeight;
    } else {
        xratio = (float)srcWidth / dstHeight;
        yratio = (float)srcHeight / dstWidth;
    }

    //计算xy在原图上的坐标
    int *sxAry = (int *)memalign(VLEN, srcWidth * sizeof(int));
    int *syAry = (int *)memalign(VLEN, srcHeight * sizeof(int));

    unsigned char *fxAry = (unsigned char *)memalign(VLEN, srcWidth * sizeof(unsigned short));
    unsigned char *fyAry = (unsigned char *)memalign(VLEN, srcHeight * sizeof(unsigned short));

    for (int dx = 0; dx < dstWidth; ++dx) {
        float fx = (float)((dx + 0.5) * xratio - 0.5);

        int x = (int)floor(fx);
        float u = fx - x;
        if (x < 0) {
            x = 0;
            u = 0.f;
        } else if (x >= srcWidth) {
            x = srcWidth - 1;
            u = 0.f;
        }

        sxAry[dx] = x;
        fxAry[dx] = (unsigned char)(u * scale);
    }

    for (int dy = 0; dy < dstHeight; ++dy) {
        float fy = (float)((dy + 0.5f) * yratio - 0.5f);

        int y = (int)floor(fy);
        float v = fy - y;
        if (y < 0) {
            y = 0;
            v = 0.f;
        } else if (y >= srcHeight) {
            y = srcHeight - 1;
            v = 0.f;
        }

        syAry[dy] = y;
        fyAry[dy] = (unsigned char)(v * scale);
    }

    unsigned char *buf = (unsigned char *)memalign(VLEN, sizeof(unsigned char) * VLEN * 4);
    unsigned char *pX0Y0 = buf;
    unsigned char *pX1Y0 = buf + VLEN;
    unsigned char *pX0Y1 = buf + VLEN * 2;
    unsigned char *pX1Y1 = buf + VLEN * 3;

    //每次算128个像素
    for (int dy = 0; dy < 1; ++dy) {
        //Load V
        HVX_Vector vVScaleh = Q6_Vh_vsplat_R(scale);
        HVX_Vector vV0uh = Q6_Vh_vsplat_R(fyAry[dy]);
        HVX_Vector vV1uh = Q6_Vh_vsub_VhVh(vVScaleh, vV0uh);

        int ii = dstWidth / VLEN;
        int remain = dstWidth & VLEN;
        for (int i = 0; i < 1; i++) {
            int startX = i * VLEN;

            //Load U
            HVX_Vector vVScaleb = Q6_Vb_vsplat_R(scale);
            HVX_Vector *pU0 = (HVX_Vector *)(fxAry + startX);
            HVX_Vector vU0ub = pU0[0];
            HVX_Vector vU1ub = Q6_Vb_vsub_VbVb(vVScaleb, vU0ub);

            //load Y to buf;
            for (int dx = startX; dx < 128; ++dx) {
                pX0Y0[dx] = pSrc[syAry[dy] * srcWidth + sxAry[dx]];
                pX1Y0[dx] = pSrc[syAry[dy] * srcWidth + sxAry[dx] + 1];
                pX0Y1[dx] = pSrc[(syAry[dy] + 1) * srcWidth + sxAry[dx]];
                pX0Y1[dx] = pSrc[(syAry[dy] + 1) * srcWidth + (sxAry[dx] + 1)];
                pDst[dx] = pX0Y0[dx];
            }
            HVX_Vector *vX0Y0 = (HVX_Vector *)(pX0Y0);
            HVX_Vector *vX1Y0 = (HVX_Vector *)(pX1Y0);
            HVX_Vector *vX0Y1 = (HVX_Vector *)(pX0Y1);
            HVX_Vector *vX1Y1 = (HVX_Vector *)(pX1Y1);
            HVX_Vector vX0Y0ub = vX0Y0[0];
            HVX_Vector vX1Y0ub = vX1Y0[0];
            HVX_Vector vX0Y1ub = vX0Y1[0];
            HVX_Vector vX1Y1ub = vX1Y1[0];

            HVX_VectorPair pt0 = Q6_Wuh_vmpy_VubVub(vX0Y0ub, vU1ub);
            pt0 = Q6_Wuh_vmpyacc_WuhVubVub(pt0, vX1Y0ub, vU0ub);
            HVX_VectorPair pt1 = Q6_Wuh_vmpy_VubVub(vX0Y0ub, vU0ub);
            pt1 = Q6_Wuh_vmpyacc_WuhVubVub(pt1, vX1Y0ub, vU1ub);

            HVX_VectorPair pt0H = Q6_Wuw_vmpy_VuhVuh(Q6_V_hi_W(pt0), vV1uh);
            HVX_VectorPair pt0L = Q6_Wuw_vmpy_VuhVuh(Q6_V_lo_W(pt0), vV1uh);
            HVX_VectorPair pt1H = Q6_Wuw_vmpy_VuhVuh(Q6_V_hi_W(pt1), vV0uh);
            HVX_VectorPair pt1L = Q6_Wuw_vmpy_VuhVuh(Q6_V_lo_W(pt1), vV0uh);

            HVX_Vector r0w = Q6_Vuw_vadd_VuwVuw_sat(Q6_V_hi_W(pt0H), Q6_V_hi_W(pt1H));
            HVX_Vector r1w = Q6_Vuw_vadd_VuwVuw_sat(Q6_V_lo_W(pt0H), Q6_V_lo_W(pt1H));
            HVX_Vector r2w = Q6_Vuw_vadd_VuwVuw_sat(Q6_V_hi_W(pt0L), Q6_V_hi_W(pt1L));
            HVX_Vector r3w = Q6_Vuw_vadd_VuwVuw_sat(Q6_V_lo_W(pt0L), Q6_V_lo_W(pt1L));
            HVX_Vector r0h = Q6_Vuh_vasr_VwVwR_sat(r0w, r1w, 8);
            HVX_Vector r1h = Q6_Vuh_vasr_VwVwR_sat(r2w, r3w, 8);

            HVX_Vector res = Q6_Vub_vasr_VhVhR_sat(Q6_Vh_vdeal_Vh(r0h), Q6_Vh_vdeal_Vh(r1h), 8);
            HVX_Vector resY = Q6_Vb_vdeal_Vb(res);

            HVX_Vector *p = (HVX_Vector *)(pDst + dy * dstWidth + startX);
            *p = resY;
        }
    }

    if(buf) {
        free(buf);
        buf = NULL;
    }

    if (sxAry) {
        free(sxAry);
        sxAry = NULL;
    }

    if (syAry) {
        free(syAry);
        syAry = NULL;
    }

    if (fxAry) {
        free(fxAry);
        fxAry = NULL;
    }

    if (fyAry) {
        free(fyAry);
        fyAry = NULL;
    }

    return 0;
}