#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include <hexagon_types.h>
#include <hvx_hexagon_protos.h>

#include "HAP_farf.h"
#include "hvx.cfg.h"
#include "qprintf.h"

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

    unsigned short *fuAry = (unsigned short *)memalign(VLEN, dstWidth * sizeof(unsigned short));
    unsigned short *fvAry = (unsigned short *)memalign(VLEN, dstHeight * sizeof(unsigned short));

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
        fuAry[dx] = (unsigned short)(u * scale);
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
        fvAry[dy] = (unsigned short)(v * scale);
    }

    unsigned char *pSrcY = (unsigned char *)pSrc;
    unsigned char *pSrcU = (unsigned char *)(pSrcY + srcHeight * srcWidth);
    unsigned char *pSrcV = pSrcU + 1;

    unsigned char *buf = (unsigned char *)memalign(VLEN, sizeof(unsigned char) * VLEN * 4);
    unsigned char *pX0Y0 = buf;
    unsigned char *pX1Y0 = buf + VLEN;
    unsigned char *pX0Y1 = buf + VLEN * 2;
    unsigned char *pX1Y1 = buf + VLEN * 3;
    HVX_Vector *vX0Y0 = (HVX_Vector *)(pX0Y0);
    HVX_Vector *vX1Y0 = (HVX_Vector *)(pX1Y0);
    HVX_Vector *vX0Y1 = (HVX_Vector *)(pX0Y1);
    HVX_Vector *vX1Y1 = (HVX_Vector *)(pX1Y1);

    unsigned char *bufUV = (unsigned char *)memalign(VLEN, sizeof(unsigned char) * VLEN * 4);
    unsigned char *pX0Y0uv = bufUV;
    unsigned char *pX1Y0uv = bufUV + VLEN;
    unsigned char *pX0Y1uv = bufUV + VLEN * 2;
    unsigned char *pX1Y1uv = bufUV + VLEN * 3;
    HVX_Vector *vX0Y0uv = (HVX_Vector *)(pX0Y0uv);
    HVX_Vector *vX1Y0uv = (HVX_Vector *)(pX1Y0uv);
    HVX_Vector *vX0Y1uv = (HVX_Vector *)(pX0Y1uv);
    HVX_Vector *vX1Y1uv = (HVX_Vector *)(pX1Y1uv);

    HVX_Vector vVScaleh = Q6_Vh_vsplat_R(scale);
    for (int dy = 0; dy < dstHeight; dy++) {
        int sy = syAry[dy];
        int sy_ = MIN(sy + 1, srcHeight - 1);

        //Load fv 
        HVX_Vector vV0uh = Q6_Vh_vsplat_R(fvAry[dy]);
        HVX_Vector vV1uh = Q6_Vh_vsub_VhVh(vVScaleh, vV0uh);
        int ii = dstWidth / VLEN;
        int remain = dstWidth & VLEN;
        for (int i = 0; i < ii; i++) {
            int startX = i * VLEN;

            //Load fu
            HVX_VectorPair *pU0 = (HVX_VectorPair *)(fuAry + startX);
            HVX_Vector vU0uhH = Q6_V_hi_W(pU0[0]);
            HVX_Vector vU0uhL = Q6_V_lo_W(pU0[0]);
            HVX_Vector vU1uhH = Q6_Vuh_vsub_VuhVuh_sat(vVScaleh, vU0uhH);
            HVX_Vector vU1uhL = Q6_Vuh_vsub_VuhVuh_sat(vVScaleh, vU0uhL);

            //load Y to buf;
            int *startXAry = sxAry + startX;
            for (int dx = 0; dx < VLEN; ++dx) {
                int sx = startXAry[dx];
                int sx_ = MIN(sx + 1, srcWidth - 1);
                pX0Y0[dx] = pSrcY[sy * srcWidth + sx];
                pX1Y0[dx] = pSrcY[sy * srcWidth + sx_];
                pX0Y1[dx] = pSrcY[sy_ * srcWidth + sx];
                pX1Y1[dx] = pSrcY[sy_ * srcWidth + sx_];

                sx -= sx % 2;
                sx_ -= sx_ % 2;
                sy = sy / 2;
                sy_ = sy_ / 2;

                ///load U
                if (dx % 2 == 0) {
                    pX0Y0uv[dx] = pSrcU[sy * srcWidth + sx];
                    pX0Y0uv[dx] = pSrcU[sy * srcWidth + sx_];
                    pX0Y0uv[dx] = pSrcU[sy_ * srcWidth + sx];
                    pX0Y0uv[dx] = pSrcU[sy_ * srcWidth + sx_];
                } else { //load V
                    pX0Y0uv[dx] = pSrcV[sy * srcWidth + sx];
                    pX0Y0uv[dx] = pSrcV[sy * srcWidth + sx_];
                    pX0Y0uv[dx] = pSrcV[sy_ * srcWidth + sx];
                    pX0Y0uv[dx] = pSrcV[sy_ * srcWidth + sx_];
                }
            }

            HVX_VectorPair wX0Y0h = Q6_Wuh_vunpack_Vub(vX0Y0[0]);
            HVX_VectorPair wX1Y0h = Q6_Wuh_vunpack_Vub(vX1Y0[0]);
            HVX_VectorPair wX0Y1h = Q6_Wuh_vunpack_Vub(vX0Y1[0]);
            HVX_VectorPair wX1Y1h = Q6_Wuh_vunpack_Vub(vX1Y1[0]);

            HVX_VectorPair s00H = Q6_Wuw_vadd_WuwWuw_sat(Q6_Wuw_vmpy_VuhVuh(vU1uhH, Q6_V_hi_W(wX0Y0h)), Q6_Wuw_vmpy_VuhVuh(vU0uhH, Q6_V_hi_W(wX1Y0h)));
            HVX_VectorPair s00L = Q6_Wuw_vadd_WuwWuw_sat(Q6_Wuw_vmpy_VuhVuh(vU1uhH, Q6_V_lo_W(wX0Y0h)), Q6_Wuw_vmpy_VuhVuh(vU0uhH, Q6_V_lo_W(wX1Y0h)));
            HVX_VectorPair s01H = Q6_Wuw_vadd_WuwWuw_sat(Q6_Wuw_vmpy_VuhVuh(vU1uhH, Q6_V_hi_W(wX0Y1h)), Q6_Wuw_vmpy_VuhVuh(vU0uhH, Q6_V_hi_W(wX1Y1h)));
            HVX_VectorPair s01L = Q6_Wuw_vadd_WuwWuw_sat(Q6_Wuw_vmpy_VuhVuh(vU1uhH, Q6_V_lo_W(wX0Y1h)), Q6_Wuw_vmpy_VuhVuh(vU0uhH, Q6_V_lo_W(wX1Y1h)));
            HVX_Vector s0H = Q6_Vuh_vasr_VuwVuwR_sat(Q6_V_hi_W(s00H), Q6_V_lo_W(s00H), 8);
            HVX_Vector s0L = Q6_Vuh_vasr_VuwVuwR_sat(Q6_V_hi_W(s00L), Q6_V_lo_W(s00L), 8);
            HVX_Vector s1H = Q6_Vuh_vasr_VuwVuwR_sat(Q6_V_hi_W(s01H), Q6_V_lo_W(s01H), 8);
            HVX_Vector s1L = Q6_Vuh_vasr_VuwVuwR_sat(Q6_V_hi_W(s01L), Q6_V_lo_W(s01L), 8);

            s00H = Q6_Wuw_vadd_WuwWuw_sat(Q6_Wuw_vmpy_VuhVuh(vV1uh, s0H), Q6_Wuw_vmpy_VuhVuh(vV0uh, s1H));
            s00L = Q6_Wuw_vadd_WuwWuw_sat(Q6_Wuw_vmpy_VuhVuh(vV1uh, s0L), Q6_Wuw_vmpy_VuhVuh(vV0uh, s1L));
            HVX_Vector s0 = Q6_Vuh_vasr_VuwVuwR_sat(Q6_V_hi_W(s00H), Q6_V_lo_W(s00H), 8);
            HVX_Vector s1 = Q6_Vuh_vasr_VuwVuwR_sat(Q6_V_hi_W(s00L), Q6_V_lo_W(s00L), 8);

            HVX_Vector resY0 = Q6_Vub_vsat_VhVh(s0, s1);
            qprintf_V("resY=%uuud\n", resY0);

            //load UV to buf;
            HVX_VectorPair wX0Y0UVh = Q6_Wuh_vunpack_Vub(vX0Y0uv[0]);
            HVX_VectorPair wX1Y0UVh = Q6_Wuh_vunpack_Vub(vX1Y0uv[0]);
            HVX_VectorPair wX0Y1UVh = Q6_Wuh_vunpack_Vub(vX0Y1uv[0]);
            HVX_VectorPair wX1Y1UVh = Q6_Wuh_vunpack_Vub(vX1Y1uv[0]);

            s00H = Q6_Wuw_vadd_WuwWuw_sat(Q6_Wuw_vmpy_VuhVuh(vU1uhH, Q6_V_hi_W(wX0Y0UVh)), Q6_Wuw_vmpy_VuhVuh(vU0uhH, Q6_V_hi_W(wX1Y0UVh)));
            s00L = Q6_Wuw_vadd_WuwWuw_sat(Q6_Wuw_vmpy_VuhVuh(vU1uhH, Q6_V_lo_W(wX0Y0UVh)), Q6_Wuw_vmpy_VuhVuh(vU0uhH, Q6_V_lo_W(wX1Y0UVh)));
            s01H = Q6_Wuw_vadd_WuwWuw_sat(Q6_Wuw_vmpy_VuhVuh(vU1uhH, Q6_V_hi_W(wX0Y1UVh)), Q6_Wuw_vmpy_VuhVuh(vU0uhH, Q6_V_hi_W(wX1Y1UVh)));
            s01L = Q6_Wuw_vadd_WuwWuw_sat(Q6_Wuw_vmpy_VuhVuh(vU1uhH, Q6_V_lo_W(wX0Y1UVh)), Q6_Wuw_vmpy_VuhVuh(vU0uhH, Q6_V_lo_W(wX1Y1UVh)));
            s0H = Q6_Vuh_vasr_VuwVuwR_sat(Q6_V_hi_W(s00H), Q6_V_lo_W(s00H), 8);
            s0L = Q6_Vuh_vasr_VuwVuwR_sat(Q6_V_hi_W(s00L), Q6_V_lo_W(s00L), 8);
            s1H = Q6_Vuh_vasr_VuwVuwR_sat(Q6_V_hi_W(s01H), Q6_V_lo_W(s01H), 8);
            s1L = Q6_Vuh_vasr_VuwVuwR_sat(Q6_V_hi_W(s01L), Q6_V_lo_W(s01L), 8);

            s00H = Q6_Wuw_vadd_WuwWuw_sat(Q6_Wuw_vmpy_VuhVuh(vV1uh, s0H), Q6_Wuw_vmpy_VuhVuh(vV0uh, s1H));
            s00L = Q6_Wuw_vadd_WuwWuw_sat(Q6_Wuw_vmpy_VuhVuh(vV1uh, s0L), Q6_Wuw_vmpy_VuhVuh(vV0uh, s1L));
            s0 = Q6_Vuh_vasr_VuwVuwR_sat(Q6_V_hi_W(s00H), Q6_V_lo_W(s00H), 8);
            s1 = Q6_Vuh_vasr_VuwVuwR_sat(Q6_V_hi_W(s00L), Q6_V_lo_W(s00L), 8);

            HVX_Vector resUV = Q6_Vub_vsat_VhVh(s0, s1);
            qprintf_V("resUV=%uuud\n", resUV);

            /*
            HVX_VectorPair pt00uh = Q6_Wuh_vmpy_VubVub(vX0Y0ub, vU1ub);
            HVX_VectorPair pt01uh = Q6_Wuh_vmpy_VubVub(vX1Y0ub, vU0ub);
            HVX_Vector pt0uhH = Q6_Vuh_vadd_VuhVuh_sat(Q6_V_hi_W(pt00uh), Q6_V_hi_W(pt01uh));
            HVX_Vector pt0uhL = Q6_Vuh_vadd_VuhVuh_sat(Q6_V_lo_W(pt00uh), Q6_V_lo_W(pt01uh));
            HVX_VectorPair ptmp0 = Q6_Wuw_vmpy_VuhVuh(pt0uhH, vV1uh);
            HVX_VectorPair ptmp1 = Q6_Wuw_vmpy_VuhVuh(pt0uhL, vV1uh);

            HVX_VectorPair pt10uh = Q6_Wuh_vmpy_VubVub(vX0Y1ub, vU1ub);
            HVX_VectorPair pt11uh = Q6_Wuh_vmpy_VubVub(vX1Y1ub, vU0ub);
            HVX_Vector pt1uhH = Q6_Vuh_vadd_VuhVuh_sat(Q6_V_hi_W(pt10uh), Q6_V_hi_W(pt11uh));
            HVX_Vector pt1uhL = Q6_Vuh_vadd_VuhVuh_sat(Q6_V_lo_W(pt10uh), Q6_V_lo_W(pt11uh));
            HVX_VectorPair ptmp2 = Q6_Wuw_vmpy_VuhVuh(pt1uhH, vV0uh);
            HVX_VectorPair ptmp3 = Q6_Wuw_vmpy_VuhVuh(pt1uhL, vV0uh);

            HVX_Vector r0uw = Q6_Vuw_vadd_VuwVuw_sat(Q6_V_hi_W(ptmp0), Q6_V_hi_W(ptmp2));
            HVX_Vector r1uw = Q6_Vuw_vadd_VuwVuw_sat(Q6_V_lo_W(ptmp0), Q6_V_lo_W(ptmp2));
            HVX_Vector r2uw = Q6_Vuw_vadd_VuwVuw_sat(Q6_V_hi_W(ptmp1), Q6_V_hi_W(ptmp3));
            HVX_Vector r3uw = Q6_Vuw_vadd_VuwVuw_sat(Q6_V_lo_W(ptmp1), Q6_V_lo_W(ptmp3));

            HVX_Vector r0uh = Q6_Vuh_vasr_VuwVuwR_sat(r0uw, r1uw, 8);
            HVX_Vector r1uh = Q6_Vuh_vasr_VuwVuwR_sat(r2uw, r3uw, 8);

            HVX_Vector resY = Q6_Vub_vasr_VuhVuhR_sat(r0uh, r1uh, 8);

            HVX_Vector *p = (HVX_Vector *)(pDst + dy * dstWidth + startX);
            *p = r0uw;
            */
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

    if (fuAry) {
        free(fuAry);
        fuAry = NULL;
    }

    if (fvAry) {
        free(fvAry);
        fvAry = NULL;
    }

    return 0;
}

void nv12_pre_process_hvx_C(const unsigned char *pSrc, int srcWidth, int srcHeight,
                            unsigned char *pDst, int dstWidth, int dstHeight, int rotate)
{
    const int scale = 1 << 8;
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

    //计算xy在原图上的坐标
    int *sxAry = (int *)malloc(srcWidth * sizeof(int));
    int *syAry = (int *)malloc(srcHeight * sizeof(int));

    unsigned short *fu0Ary = (unsigned short *)malloc(dstWidth * sizeof(unsigned short));
    unsigned short *fu1Ary = (unsigned short *)malloc(dstWidth * sizeof(unsigned short));
    unsigned short *fv0Ary = (unsigned short *)malloc(dstHeight * sizeof(unsigned short));
    unsigned short *fv1Ary = (unsigned short *)malloc(dstHeight * sizeof(unsigned short));

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
        fu0Ary[dx] = (unsigned short)(u * scale);
        fu1Ary[dx] = (unsigned short)(scale - fu0Ary[dx]);
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
        fv0Ary[dy] = (unsigned short)(v * scale);
        fv1Ary[dy] = (unsigned short)(scale - fv0Ary[dy]);
    }
    unsigned char *pSrcY = (unsigned char *)(pSrc);
    unsigned char *pSrcU = (unsigned char *)(pSrcY + srcHeight * srcWidth);
    unsigned char *pSrcV = pSrcU + 1;

    unsigned char *buf = (unsigned char *)malloc(sizeof(unsigned char) * VLEN * 4);
    unsigned char *pX0Y0 = buf;
    unsigned char *pX1Y0 = buf + VLEN;
    unsigned char *pX0Y1 = buf + VLEN * 2;
    unsigned char *pX1Y1 = buf + VLEN * 3;

    unsigned char *bufU = (unsigned char *)malloc(sizeof(unsigned char) * VLEN * 4);
    unsigned char *pX0Y0u = bufU;
    unsigned char *pX1Y0u = bufU + VLEN;
    unsigned char *pX0Y1u = bufU + VLEN * 2;
    unsigned char *pX1Y1u = bufU + VLEN * 3;

    unsigned char *bufV = (unsigned char *)malloc(sizeof(unsigned char) * VLEN * 4);
    unsigned char *pX0Y0v = bufV;
    unsigned char *pX1Y0v = bufV + VLEN;
    unsigned char *pX0Y1v = bufV + VLEN * 2;
    unsigned char *pX1Y1v = bufV + VLEN * 3;

    int *sum0 = (int *)malloc(sizeof(int) * VLEN);
    int *sum1 = (int *)malloc(sizeof(int) * VLEN);
    unsigned char *resY = (unsigned char *)malloc(sizeof(unsigned char) * VLEN);
    unsigned char *resU = (unsigned char *)malloc(sizeof(unsigned char) * VLEN);
    unsigned char *resV = (unsigned char *)malloc(sizeof(unsigned char) * VLEN);
    for (int dy = 0; dy < dstHeight; ++dy)
    {
        unsigned short v0 = fv0Ary[dy];
        unsigned short v1 = fv1Ary[dy];

        int ii = dstWidth / VLEN;
        int remain = dstWidth & VLEN;
        for (int i = 0; i < ii; i++)
        {
            int startX = i * VLEN;
            int *startXAry = sxAry + startX;
            for (int dx = 0; dx < VLEN; ++dx)
            {
                int sy = syAry[dy];
                int sy_ = MIN(sy + 1, srcHeight - 1);
                int sx = startXAry[dx];
                int sx_ = MIN(sx + 1, srcWidth - 1);
                pX0Y0[dx] = pSrc[sy * srcWidth + sx];
                pX1Y0[dx] = pSrc[sy * srcWidth + sx_];
                pX0Y1[dx] = pSrc[sy_ * srcWidth + sx];
                pX1Y1[dx] = pSrc[sy_ * srcWidth + sx_];

                sx -= sx % 2;
                sx_ -= sx_ % 2;
                sy = sy / 2;
                sy_ = sy_ / 2;

                pX0Y0u[dx] = pSrcU[sy * srcWidth + sx];
                pX1Y0u[dx] = pSrcU[sy * srcWidth + sx_];
                pX0Y1u[dx] = pSrcU[sy_ * srcWidth + sx];
                pX1Y1u[dx] = pSrcU[sy_ * srcWidth + sx_];

                pX0Y0v[dx] = pSrcV[sy * srcWidth + sx];
                pX1Y0v[dx] = pSrcV[sy * srcWidth + sx_];
                pX0Y1v[dx] = pSrcV[sy_ * srcWidth + sx];
                pX1Y1v[dx] = pSrcV[sy_ * srcWidth + sx_];
            }

            unsigned short *pU0 = fu0Ary + startX;
            unsigned short *pU1 = fu1Ary + startX;

            for (int n = 0; n < VLEN; ++n)
            {
                sum0[n] = pX0Y0[n] * pU1[n];
            }

            for (int n = 0; n < VLEN; ++n)
            {
                sum0[n] += pX1Y0[n] * pU0[n];
            }

            for (int n = 0; n < VLEN; ++n)
            {
                sum1[n] = pX0Y1[n] * pU1[n];
            }

            for (int n = 0; n < VLEN; ++n)
            {
                sum1[n] += pX1Y1[n] * pU0[n];
            }

            for (int n = 0; n < VLEN; ++n)
            {
                sum0[n] += sum0[n] * v1;
            }

            for (int n = 0; n < VLEN; ++n)
            {
                sum1[n] += sum1[n] * v0;
            }

            for (int n = 0; n < VLEN; ++n)
            {
                sum0[n] += sum1[n];
            }

            for (int n = 0; n < VLEN; ++n)
            {
                int t = sum0[n] >> 16;
                if (t < 0)
                    t = 0;
                if (t > 255)
                    t = 255;
                resY[n] = t;
            }

            //计算uv
            for (int n = 0; n < VLEN; ++n)
            {
                sum0[n] = pX0Y0u[n] * pU1[n];
            }

            for (int n = 0; n < VLEN; ++n)
            {
                sum0[n] += pX1Y0u[n] * pU0[n];
            }

            for (int n = 0; n < VLEN; ++n)
            {
                sum1[n] = pX0Y1u[n] * pU1[n];
            }

            for (int n = 0; n < VLEN; ++n)
            {
                sum1[n] += pX1Y1u[n] * pU0[n];
            }

            for (int n = 0; n < VLEN; ++n)
            {
                sum0[n] += sum0[n] * v1;
            }

            for (int n = 0; n < VLEN; ++n)
            {
                sum1[n] += sum1[n] * v0;
            }

            for (int n = 0; n < VLEN; ++n)
            {
                sum0[n] += sum1[n];
            }

            for (int n = 0; n < VLEN; ++n)
            {
                int t = sum0[n] >> 16;
                if (t < 0)
                    t = 0;
                if (t > 255)
                    t = 255;
                resU[n] = t;
            }

            for (int n = 0; n < VLEN; ++n)
            {
                sum0[n] = pX0Y0v[n] * pU1[n];
            }

            for (int n = 0; n < VLEN; ++n)
            {
                sum0[n] += pX0Y0v[n] * pU0[n];
            }

            for (int n = 0; n < VLEN; ++n)
            {
                sum1[n] = pX0Y1u[n] * pU1[n];
            }

            for (int n = 0; n < VLEN; ++n)
            {
                sum1[n] += pX1Y1u[n] * pU0[n];
            }

            for (int n = 0; n < VLEN; ++n)
            {
                sum0[n] += sum0[n] * v1;
            }

            for (int n = 0; n < VLEN; ++n)
            {
                sum1[n] += sum1[n] * v0;
            }

            for (int n = 0; n < VLEN; ++n)
            {
                sum0[n] += sum1[n];
            }

            for (int n = 0; n < VLEN; ++n)
            {
                int t = sum0[n] >> 16;
                if (t < 0)
                    t = 0;
                if (t > 255)
                    t = 255;
                resV[n] = t;
            }

            //计算bgr
            for (int n = 0; n < VLEN; ++n)
            {
                unsigned char y, u, v;
                int r, g, b;
                y = resY[n];
                u = resU[n];
                v = resV[n];

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

                int dst_idx = dy * dstWidth * 3 + (startX + n) * 3;
                pDst[dst_idx] = (unsigned char)b;
                pDst[dst_idx + 1] = (unsigned char)g;
                pDst[dst_idx + 2] = (unsigned char)r;
            }
        }
    }

    free(sxAry);
    free(syAry);
    free(fu0Ary);
    free(fu1Ary);
    free(fv0Ary);
    free(fv1Ary);

    free(buf);
    free(bufU);
    free(bufV);

    free(sum0);
    free(sum1);
    free(resY);
    free(resU);
    free(resV);
}