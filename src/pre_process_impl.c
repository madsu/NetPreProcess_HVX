#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include <hexagon_types.h>
#include <hvx_hexagon_protos.h>

#include "HAP_farf.h"
#include "HAP_power.h"
#include "HAP_compute_res.h"
#include "HAP_vtcm_mgr.h"

#include "q6cache.h"
#include "dspCV_hvx.h"
#include "dspCV_worker.h"

#include "hvx.cfg.h"
#include "qprintf.h"
#include "pre_process.h"

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))
int pre_process_nv12_ori(const uint8* pSrc, int pSrcLen, int srcWidth, int srcHeight, uint8* pDst, int pDstLen, int dstWidth, int dstHeight, int rotate)
{
    for (int j = 0; j < dstHeight; ++j)
    {
        for (int i = 0; i < dstWidth; ++i)
        {
            int dx, dy;

            const uint8_t *ptrList[3];
            int stepList[3];
            uint8_t ret[3];

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
                const uint8_t *ptr = ptrList[chn];
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

                uint8_t data00 = ptr[y * step + x];
                uint8_t data10 = ptr[y * step + x_];
                uint8_t data01 = ptr[y_ * step + x];
                uint8_t data11 = ptr[y_ * step + x_];

                float var = v1 * (u1 * data00 + u * data10) + v * (u1 * data01 + u * data11);
                if (var < 0)
                    var = 0;
                if (var > 255)
                    var = 255;

                ret[chn] = (uint8_t)var;
            }

            //nv21 -> bgr
            uint8_t y, u, v;
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
            pDst[dst_idx] = (uint8_t)b;
            pDst[dst_idx + 1] = (uint8_t)g;
            pDst[dst_idx + 2] = (uint8_t)r;
        }
    }
    return 0;
}

static int setClocks()
{
#if (__HEXAGON_ARCH__ < 62)
    return 0;
#endif
    HAP_power_request_t request;
    memset(&request, 0, sizeof(HAP_power_request_t)); //Important to clear the structure if only selected fields are updated.
    request.type = HAP_power_set_apptype;
    request.apptype = HAP_POWER_COMPUTE_CLIENT_CLASS;
    int retval = HAP_power_set(NULL, &request);
    if (retval)
        return AEE_EFAILED;

    // Configure clocks & DCVS mode
    memset(&request, 0, sizeof(HAP_power_request_t)); //Important to clear the structure if only selected fields are updated.
    request.type = HAP_power_set_DCVS_v2;
    request.dcvs_v2.dcvs_enable = 0; // enable dcvs if desired, else it locks to target corner
    request.dcvs_v2.set_dcvs_params = TRUE;
    request.dcvs_v2.dcvs_params.target_corner = HAP_DCVS_VCORNER_NOM; // nominal voltage corner.
    request.dcvs_v2.set_latency = TRUE;
    request.dcvs_v2.latency = 100;
    retval = HAP_power_set(NULL, &request);
    if (retval)
        return AEE_EFAILED;

    return 0;
}

typedef struct
{
    dspCV_synctoken_t *token;
    int32_t retval;
    int32_t threadIdx;
    int32_t threadCount;

    const uint8_t *pSrcImg;
    int32_t srcWidth;
    int32_t srcHeight;
    uint8_t *pDstImg;
    int32_t dstWidth;
    int32_t dstHeight;
    int32_t rotate;

    int32_t *sxAry;
    int32_t *syAry;
    uint16_t *fuAry;
    uint16_t *fvAry;
} nv12hvx_callback_t;

#define   roundup_t(a, m)   (((a)+(m)-1)&(-m))
static void pre_process_nv12_callback(void *data)
{
    nv12hvx_callback_t *dptr = (nv12hvx_callback_t *)data;
    int32_t tid = dspCV_atomic_inc_return(&(dptr->threadIdx)) - 1;
    int32_t NUM_THREADS = dptr->threadCount;

    uint8_t *pSrcImg = dptr->pSrcImg;
    int32_t srcWidth = dptr->srcWidth;
    int32_t srcHeight = dptr->srcHeight;
    uint8_t *pDstImg = dptr->pDstImg;
    int32_t dstWidth = dptr->dstWidth;
    int32_t dstHeight = dptr->dstHeight;
    int32_t *sxAry = dptr->sxAry;
    int32_t *syAry = dptr->syAry;
    uint16_t *fuAry = dptr->fuAry;
    uint16_t *fvAry = dptr->fvAry;
    int32_t rotate = dptr->rotate;

    const uint8_t *pSrcY = pSrcImg;
    const uint8_t *pSrcU = pSrcY + srcHeight * srcWidth;
    const uint8_t *pSrcV = pSrcU + 1;
    uint8_t *pDst = pDstImg;
    int32_t dstStride = roundup_t(dstWidth, VECLEN) * 4;

    //compute dst rows for every thread
    int32_t srcRows = srcHeight / NUM_THREADS;
    int32_t dstRows = dstHeight / NUM_THREADS;
    int32_t startY = tid * dstRows;
    if (tid == (NUM_THREADS - 1)) {
        dstRows = dstHeight - dstRows * (NUM_THREADS - 1);
    }

    //compute cycle num
    int32_t nnY = dstRows >> 1;
    int32_t remainY = dstRows & 1;
    int32_t nnX = dstWidth / VECLEN;
    int32_t remainX = dstWidth - (nnX * VECLEN);
    int32_t l2fsize = roundup_t(srcWidth * 2, VECLEN) * sizeof(uint8_t);
    uint64_t L2FETCH_PARA = CreateL2pfParam(srcWidth * 2 * sizeof(uint8_t), l2fsize, 1, 0);

    //alloc buf
    uint8_t *buf = (uint8_t *)memalign(VECLEN, sizeof(uint8_t) * VECLEN * 12);
    uint8_t *pX0Y0y0 = buf;
    uint8_t *pX1Y0y0 = buf + VECLEN * 1;
    uint8_t *pX0Y1y0 = buf + VECLEN * 2;
    uint8_t *pX1Y1y0 = buf + VECLEN * 3;
    uint8_t *pX0Y0y1 = buf + VECLEN * 4;
    uint8_t *pX1Y0y1 = buf + VECLEN * 5;
    uint8_t *pX0Y1y1 = buf + VECLEN * 6;
    uint8_t *pX1Y1y1 = buf + VECLEN * 7;
    uint8_t *pX0Y0uv = buf + VECLEN * 8;
    uint8_t *pX0Y1uv = buf + VECLEN * 9;
    uint8_t *pX1Y0uv = buf + VECLEN * 10;
    uint8_t *pX1Y1uv = buf + VECLEN * 11;
    HVX_Vector *vX0Y0y0 = (HVX_Vector *)(pX0Y0y0);
    HVX_Vector *vX1Y0y0 = (HVX_Vector *)(pX1Y0y0);
    HVX_Vector *vX0Y1y0 = (HVX_Vector *)(pX0Y1y0);
    HVX_Vector *vX1Y1y0 = (HVX_Vector *)(pX1Y1y0);
    HVX_Vector *vX0Y0y1 = (HVX_Vector *)(pX0Y0y1);
    HVX_Vector *vX1Y0y1 = (HVX_Vector *)(pX1Y0y1);
    HVX_Vector *vX0Y1y1 = (HVX_Vector *)(pX0Y1y1);
    HVX_Vector *vX1Y1y1 = (HVX_Vector *)(pX1Y1y1);
    HVX_Vector *vX0Y0uv = (HVX_Vector *)(pX0Y0uv);
    HVX_Vector *vX1Y0uv = (HVX_Vector *)(pX1Y0uv);
    HVX_Vector *vX0Y1uv = (HVX_Vector *)(pX0Y1uv);
    HVX_Vector *vX1Y1uv = (HVX_Vector *)(pX1Y1uv);

    HVX_Vector sUv, sY0, sY1, sV_833u_400, sR, sG, sB, sGE, sGO, sRE, sRO, sBE, sBO, sIffG, sIBR, sIBR2;
    HVX_VectorPair dUvx2, dY0x2, dY1x2, dY1192a, dY1192b, dY1192c, dU2066v1634, dIffBGR, dIffBGR2;
    const int scale = 1 << 8;
    int const_400_833 = ((-400 << 16) + ((-833) & 0xffff));
    int const1192 = (1192 << 16) + 1192;
    int const2066n1634 = ((2066 << 16) + 1634);
    int const10 = 10;
    int const_1 = -1;
    HVX_Vector sConst0xff = Q6_V_vsplat_R(0x00ff00ff);
    HVX_Vector sConst128 = Q6_V_vsplat_R(0x80808080);
    HVX_Vector sConst16 = Q6_V_vsplat_R(0x10101010);
    HVX_Vector vVScaleh = Q6_Vh_vsplat_R(scale);

    int32_t dy = startY;
    for (; nnY > 0; nnY--, dy += 2) {
        HVX_Vector vV0uh0 = Q6_Vh_vsplat_R(fvAry[dy]);
        HVX_Vector vV1uh0 = Q6_Vh_vsub_VhVh(vVScaleh, vV0uh0);
        HVX_Vector vV0uh1 = Q6_Vh_vsplat_R(fvAry[dy + 1]);
        HVX_Vector vV1uh1 = Q6_Vh_vsub_VhVh(vVScaleh, vV0uh1);

        HVX_Vector *prgb0 = (HVX_Vector *)(pDst + dy * dstStride);
        HVX_Vector *prgb1 = (HVX_Vector *)(pDst + (dy + 1) * dstStride);

        int32_t sx, sx_, sy0, sy0_, sy1, sy1_, su0, su0_;
        if (rotate == 0) {
            sy0 = syAry[dy];
            sy0_ = MIN(sy0 + 1, srcHeight - 1);
            sy1 = syAry[dy + 1];
            sy1_ = MIN(sy1 + 1, srcHeight - 1);
            su0 = sy0 >> 1;
            su0_ = sy0_ >> 1;
            L2fetch((unsigned int)(pSrcY + sy0 * srcWidth), L2FETCH_PARA);
            L2fetch((unsigned int)(pSrcY + sy1 * srcWidth), L2FETCH_PARA);
            L2fetch((unsigned int)(pSrcU + su0 * srcWidth), L2FETCH_PARA);
        } else if (rotate == 90) {
            sx = MIN(srcWidth - syAry[dy], srcWidth - 1);
            sx_ = MAX(sx - 1, 0);
        } else if(rotate == 180) {
            sy0 = MIN(srcHeight - syAry[dy], srcHeight - 1);
            sy0_ = MAX(sy0 - 1, 0);
            sy1 = MIN(srcHeight - syAry[dy + 1], srcHeight - 1);
            sy1_ = MAX(sy1 - 1, 0);
            su0 = sy0 >> 1;
            su0_ = sy0_ >> 1;
            L2fetch((unsigned int)(pSrcY + sy0 * srcWidth), L2FETCH_PARA);
            L2fetch((unsigned int)(pSrcY + sy1 * srcWidth), L2FETCH_PARA);
            L2fetch((unsigned int)(pSrcU + su0 * srcWidth), L2FETCH_PARA);
        } else {
            sx = syAry[dy];
            sx_ = MIN(sx + 1, srcWidth - 1);
        }

        int32_t dx = 0;
        for (int32_t n = 0; n < nnX; n++, dx += VECLEN) {
            //load YUV to buf;
            for (int32_t idx = 0; idx < VECLEN; ++idx) {
                if (rotate == 0) {
                    sx = sxAry[dx + idx];
                    sx_ = MIN(sx + 1, srcWidth - 1);
                } else if(rotate == 90) {
                    sy0 = sxAry[dx + idx];
                    sy0_ = MIN(sy0 + 1, srcHeight - 1);
                    int32_t iy1 = MIN(dx + 1 + idx, dstWidth - 1);
                    sy1 = sxAry[iy1];
                    sy1_ = MIN(sy1 + 1, srcHeight - 1);
                    su0 = sy0 >> 1;
                    su0_ = sy0_ >> 1;
                } else if(rotate == 180) {
                    sx = MIN(srcWidth - sxAry[dx + idx], srcWidth - 1);
                    sx_ = MAX(sx - 1, 0);
                } else {
                    sy0 = MIN(srcHeight - sxAry[dx + idx], srcHeight - 1);
                    sy0_ = MAX(sy0 - 1, 0);
                    int32_t iy1 = MIN(dx + 1 + idx, dstWidth - 1);
                    sy1 = MIN(srcHeight - sxAry[iy1], srcHeight - 1);
                    sy1_ = MAX(sy1 - 1, 0);
                    su0 = sy0 >> 1;
                    su0_ = sy0_ >> 1;
                }

                pX0Y0y0[idx] = pSrcY[sy0 * srcWidth + sx];
                pX1Y0y0[idx] = pSrcY[sy0 * srcWidth + sx_];
                pX0Y1y0[idx] = pSrcY[sy0_ * srcWidth + sx];
                pX1Y1y0[idx] = pSrcY[sy0_ * srcWidth + sx_];
                pX0Y0y1[idx] = pSrcY[sy1 * srcWidth + sx];
                pX1Y0y1[idx] = pSrcY[sy1 * srcWidth + sx_];
                pX0Y1y1[idx] = pSrcY[sy1_ * srcWidth + sx];
                pX1Y1y1[idx] = pSrcY[sy1_ * srcWidth + sx_];

                int32_t sux = sx - sx % 2;
                int32_t sux_ = sx_ - sx_ % 2;
                if (idx % 2 == 0) {
                    pX0Y0uv[idx] = pSrcU[su0 * srcWidth + sux];
                    pX1Y0uv[idx] = pSrcU[su0 * srcWidth + sux_];
                    pX0Y1uv[idx] = pSrcU[su0_ * srcWidth + sux];
                    pX1Y1uv[idx] = pSrcU[su0_ * srcWidth + sux_];
                } else {
                    pX0Y0uv[idx] = pSrcV[su0 * srcWidth + sux];
                    pX1Y0uv[idx] = pSrcV[su0 * srcWidth + sux_];
                    pX0Y1uv[idx] = pSrcV[su0_ * srcWidth + sux];
                    pX1Y1uv[idx] = pSrcV[su0_ * srcWidth + sux_];
                }
            }

            //Load fu
            HVX_VectorPair *pU0 = (HVX_VectorPair *)(fuAry + dx);
            HVX_Vector vU0uhH = Q6_V_hi_W(pU0[0]);
            HVX_Vector vU0uhL = Q6_V_lo_W(pU0[0]);
            HVX_Vector vU1uhH = Q6_Vuh_vsub_VuhVuh_sat(vVScaleh, vU0uhH);
            HVX_Vector vU1uhL = Q6_Vuh_vsub_VuhVuh_sat(vVScaleh, vU0uhL);

            //compute Y0
            HVX_VectorPair wX0Y0h = Q6_Wuh_vzxt_Vub(vX0Y0y0[0]);
            HVX_VectorPair wX1Y0h = Q6_Wuh_vzxt_Vub(vX1Y0y0[0]);
            HVX_VectorPair wX0Y1h = Q6_Wuh_vzxt_Vub(vX0Y1y0[0]);
            HVX_VectorPair wX1Y1h = Q6_Wuh_vzxt_Vub(vX1Y1y0[0]);

            HVX_VectorPair s00H = Q6_Wuw_vadd_WuwWuw_sat(Q6_Wuw_vmpy_VuhVuh(vU1uhH, Q6_V_hi_W(wX0Y0h)), Q6_Wuw_vmpy_VuhVuh(vU0uhH, Q6_V_hi_W(wX1Y0h)));
            HVX_VectorPair s00L = Q6_Wuw_vadd_WuwWuw_sat(Q6_Wuw_vmpy_VuhVuh(vU1uhH, Q6_V_lo_W(wX0Y0h)), Q6_Wuw_vmpy_VuhVuh(vU0uhH, Q6_V_lo_W(wX1Y0h)));
            HVX_VectorPair s01H = Q6_Wuw_vadd_WuwWuw_sat(Q6_Wuw_vmpy_VuhVuh(vU1uhH, Q6_V_hi_W(wX0Y1h)), Q6_Wuw_vmpy_VuhVuh(vU0uhH, Q6_V_hi_W(wX1Y1h)));
            HVX_VectorPair s01L = Q6_Wuw_vadd_WuwWuw_sat(Q6_Wuw_vmpy_VuhVuh(vU1uhH, Q6_V_lo_W(wX0Y1h)), Q6_Wuw_vmpy_VuhVuh(vU0uhH, Q6_V_lo_W(wX1Y1h)));

            HVX_Vector s0H = Q6_Vuh_vasr_VuwVuwR_sat(Q6_V_hi_W(s00H), Q6_V_lo_W(s00H), 8);
            HVX_Vector s0L = Q6_Vuh_vasr_VuwVuwR_sat(Q6_V_hi_W(s00L), Q6_V_lo_W(s00L), 8);
            HVX_Vector s1H = Q6_Vuh_vasr_VuwVuwR_sat(Q6_V_hi_W(s01H), Q6_V_lo_W(s01H), 8);
            HVX_Vector s1L = Q6_Vuh_vasr_VuwVuwR_sat(Q6_V_hi_W(s01L), Q6_V_lo_W(s01L), 8);

            s00H = Q6_Wuw_vadd_WuwWuw_sat(Q6_Wuw_vmpy_VuhVuh(vV1uh0, s0H), Q6_Wuw_vmpy_VuhVuh(vV0uh0, s1H));
            s00L = Q6_Wuw_vadd_WuwWuw_sat(Q6_Wuw_vmpy_VuhVuh(vV1uh0, s0L), Q6_Wuw_vmpy_VuhVuh(vV0uh0, s1L));
            HVX_Vector s0 = Q6_Vuh_vasr_VuwVuwR_sat(Q6_V_hi_W(s00H), Q6_V_lo_W(s00H), 8);
            HVX_Vector s1 = Q6_Vuh_vasr_VuwVuwR_sat(Q6_V_hi_W(s00L), Q6_V_lo_W(s00L), 8);
            HVX_Vector resY0 = Q6_Vub_vsat_VhVh(s0, s1);

            //compute Y1
            wX0Y0h = Q6_Wuh_vzxt_Vub(vX0Y0y1[0]);
            wX1Y0h = Q6_Wuh_vzxt_Vub(vX1Y0y1[0]);
            wX0Y1h = Q6_Wuh_vzxt_Vub(vX0Y1y1[0]);
            wX1Y1h = Q6_Wuh_vzxt_Vub(vX1Y1y1[0]);

            s00H = Q6_Wuw_vadd_WuwWuw_sat(Q6_Wuw_vmpy_VuhVuh(vU1uhH, Q6_V_hi_W(wX0Y0h)), Q6_Wuw_vmpy_VuhVuh(vU0uhH, Q6_V_hi_W(wX1Y0h)));
            s00L = Q6_Wuw_vadd_WuwWuw_sat(Q6_Wuw_vmpy_VuhVuh(vU1uhH, Q6_V_lo_W(wX0Y0h)), Q6_Wuw_vmpy_VuhVuh(vU0uhH, Q6_V_lo_W(wX1Y0h)));
            s01H = Q6_Wuw_vadd_WuwWuw_sat(Q6_Wuw_vmpy_VuhVuh(vU1uhH, Q6_V_hi_W(wX0Y1h)), Q6_Wuw_vmpy_VuhVuh(vU0uhH, Q6_V_hi_W(wX1Y1h)));
            s01L = Q6_Wuw_vadd_WuwWuw_sat(Q6_Wuw_vmpy_VuhVuh(vU1uhH, Q6_V_lo_W(wX0Y1h)), Q6_Wuw_vmpy_VuhVuh(vU0uhH, Q6_V_lo_W(wX1Y1h)));

            s0H = Q6_Vuh_vasr_VuwVuwR_sat(Q6_V_hi_W(s00H), Q6_V_lo_W(s00H), 8);
            s0L = Q6_Vuh_vasr_VuwVuwR_sat(Q6_V_hi_W(s00L), Q6_V_lo_W(s00L), 8);
            s1H = Q6_Vuh_vasr_VuwVuwR_sat(Q6_V_hi_W(s01H), Q6_V_lo_W(s01H), 8);
            s1L = Q6_Vuh_vasr_VuwVuwR_sat(Q6_V_hi_W(s01L), Q6_V_lo_W(s01L), 8);

            s00H = Q6_Wuw_vadd_WuwWuw_sat(Q6_Wuw_vmpy_VuhVuh(vV1uh1, s0H), Q6_Wuw_vmpy_VuhVuh(vV0uh1, s1H));
            s00L = Q6_Wuw_vadd_WuwWuw_sat(Q6_Wuw_vmpy_VuhVuh(vV1uh1, s0L), Q6_Wuw_vmpy_VuhVuh(vV0uh1, s1L));
            s0 = Q6_Vuh_vasr_VuwVuwR_sat(Q6_V_hi_W(s00H), Q6_V_lo_W(s00H), 8);
            s1 = Q6_Vuh_vasr_VuwVuwR_sat(Q6_V_hi_W(s00L), Q6_V_lo_W(s00L), 8);
            HVX_Vector resY1 = Q6_Vub_vsat_VhVh(s0, s1);

            //compute UV
            wX0Y0h = Q6_Wuh_vzxt_Vub(vX0Y0uv[0]);
            wX1Y0h = Q6_Wuh_vzxt_Vub(vX1Y0uv[0]);
            wX0Y1h = Q6_Wuh_vzxt_Vub(vX0Y1uv[0]);
            wX1Y1h = Q6_Wuh_vzxt_Vub(vX1Y1uv[0]);

            s00H = Q6_Wuw_vadd_WuwWuw_sat(Q6_Wuw_vmpy_VuhVuh(vU1uhH, Q6_V_hi_W(wX0Y0h)), Q6_Wuw_vmpy_VuhVuh(vU0uhH, Q6_V_hi_W(wX1Y0h)));
            s00L = Q6_Wuw_vadd_WuwWuw_sat(Q6_Wuw_vmpy_VuhVuh(vU1uhH, Q6_V_lo_W(wX0Y0h)), Q6_Wuw_vmpy_VuhVuh(vU0uhH, Q6_V_lo_W(wX1Y0h)));
            s01H = Q6_Wuw_vadd_WuwWuw_sat(Q6_Wuw_vmpy_VuhVuh(vU1uhH, Q6_V_hi_W(wX0Y1h)), Q6_Wuw_vmpy_VuhVuh(vU0uhH, Q6_V_hi_W(wX1Y1h)));
            s01L = Q6_Wuw_vadd_WuwWuw_sat(Q6_Wuw_vmpy_VuhVuh(vU1uhH, Q6_V_lo_W(wX0Y1h)), Q6_Wuw_vmpy_VuhVuh(vU0uhH, Q6_V_lo_W(wX1Y1h)));
            s0H = Q6_Vuh_vasr_VuwVuwR_sat(Q6_V_hi_W(s00H), Q6_V_lo_W(s00H), 8);
            s0L = Q6_Vuh_vasr_VuwVuwR_sat(Q6_V_hi_W(s00L), Q6_V_lo_W(s00L), 8);
            s1H = Q6_Vuh_vasr_VuwVuwR_sat(Q6_V_hi_W(s01H), Q6_V_lo_W(s01H), 8);
            s1L = Q6_Vuh_vasr_VuwVuwR_sat(Q6_V_hi_W(s01L), Q6_V_lo_W(s01L), 8);

            s00H = Q6_Wuw_vadd_WuwWuw_sat(Q6_Wuw_vmpy_VuhVuh(vV1uh0, s0H), Q6_Wuw_vmpy_VuhVuh(vV0uh0, s1H));
            s00L = Q6_Wuw_vadd_WuwWuw_sat(Q6_Wuw_vmpy_VuhVuh(vV1uh0, s0L), Q6_Wuw_vmpy_VuhVuh(vV0uh0, s1L));
            s0 = Q6_Vuh_vasr_VuwVuwR_sat(Q6_V_hi_W(s00H), Q6_V_lo_W(s00H), 8);
            s1 = Q6_Vuh_vasr_VuwVuwR_sat(Q6_V_hi_W(s00L), Q6_V_lo_W(s00L), 8);
            HVX_Vector resUV = Q6_Vub_vsat_VhVh(s0, s1);

            sUv = Q6_Vb_vshuff_Vb(resUV);
            dUvx2 = Q6_Wh_vsub_VubVub(sUv, sConst128);
            sV_833u_400 = Q6_Vw_vdmpy_VhRh_sat(Q6_V_lo_W(dUvx2), const_400_833);
            dU2066v1634 = Q6_Ww_vmpy_VhRh(Q6_V_lo_W(dUvx2), const2066n1634);

            sY0 = Q6_Vub_vsub_VubVub_sat(resY0, sConst16);
            dY0x2 = Q6_Wuh_vunpack_Vub(sY0);

            sY1 = Q6_Vub_vsub_VubVub_sat(resY1, sConst16);
            dY1x2 = Q6_Wuh_vunpack_Vub(sY1);

            dY1192a = Q6_Wuw_vmpy_VuhRuh(Q6_V_lo_W(dY0x2), const1192);
            sGE = Q6_Vw_vadd_VwVw(Q6_V_lo_W(dY1192a), sV_833u_400);
            sGO = Q6_Vw_vadd_VwVw(Q6_V_hi_W(dY1192a), sV_833u_400);
            sRE = Q6_Vw_vadd_VwVw(Q6_V_lo_W(dY1192a), Q6_V_lo_W(dU2066v1634));
            sRO = Q6_Vw_vadd_VwVw(Q6_V_hi_W(dY1192a), Q6_V_lo_W(dU2066v1634));
            sBE = Q6_Vw_vadd_VwVw(Q6_V_lo_W(dY1192a), Q6_V_hi_W(dU2066v1634));
            sBO = Q6_Vw_vadd_VwVw(Q6_V_hi_W(dY1192a), Q6_V_hi_W(dU2066v1634));
            sG = Q6_Vuh_vasr_VwVwR_sat(sGO, sGE, const10);
            sR = Q6_Vuh_vasr_VwVwR_sat(sRO, sRE, const10);
            sB = Q6_Vuh_vasr_VwVwR_sat(sBO, sBE, const10);
            sIffG = Q6_Vub_vsat_VhVh(sConst0xff, sG);
            sIBR = Q6_Vub_vsat_VhVh(sB, sR);
            dIffBGR = Q6_W_vshuff_VVR(sIffG, sIBR, const_1);
            *prgb0++ = Q6_V_lo_W(dIffBGR);
            *prgb0++ = Q6_V_hi_W(dIffBGR);

            dY1192c = Q6_Wuw_vmpy_VuhRuh(Q6_V_lo_W(dY1x2), const1192);
            sGE = Q6_Vw_vadd_VwVw(Q6_V_lo_W(dY1192c), sV_833u_400);
            sGO = Q6_Vw_vadd_VwVw(Q6_V_hi_W(dY1192c), sV_833u_400);
            sRE = Q6_Vw_vadd_VwVw(Q6_V_lo_W(dY1192c), Q6_V_lo_W(dU2066v1634));
            sRO = Q6_Vw_vadd_VwVw(Q6_V_hi_W(dY1192c), Q6_V_lo_W(dU2066v1634));
            sBE = Q6_Vw_vadd_VwVw(Q6_V_lo_W(dY1192c), Q6_V_hi_W(dU2066v1634));
            sBO = Q6_Vw_vadd_VwVw(Q6_V_hi_W(dY1192c), Q6_V_hi_W(dU2066v1634));
            sG = Q6_Vuh_vasr_VwVwR_sat(sGO, sGE, const10);
            sR = Q6_Vuh_vasr_VwVwR_sat(sRO, sRE, const10);
            sB = Q6_Vuh_vasr_VwVwR_sat(sBO, sBE, const10);
            sG = Q6_Vh_vmin_VhVh(sG, sConst0xff);
            sR = Q6_Vh_vmin_VhVh(sR, sConst0xff);
            sB = Q6_Vh_vmin_VhVh(sB, sConst0xff);
            sIffG = Q6_Vb_vshuffe_VbVb(sConst0xff, sG);
            sIBR2 = Q6_Vb_vshuffe_VbVb(sB, sR);
            dIffBGR2 = Q6_W_vshuff_VVR(sIffG, sIBR2, const_1);
            *prgb1++ = Q6_V_lo_W(dIffBGR2);
            *prgb1++ = Q6_V_hi_W(dIffBGR2);

            sV_833u_400 = Q6_Vw_vdmpy_VhRh_sat(Q6_V_hi_W(dUvx2), const_400_833);
            dU2066v1634 = Q6_Ww_vmpy_VhRh(Q6_V_hi_W(dUvx2), const2066n1634);

            dY1192b = Q6_Wuw_vmpy_VuhRuh(Q6_V_hi_W(dY0x2), const1192);
            sGE = Q6_Vw_vadd_VwVw(Q6_V_lo_W(dY1192b), sV_833u_400);
            sGO = Q6_Vw_vadd_VwVw(Q6_V_hi_W(dY1192b), sV_833u_400);
            sRE = Q6_Vw_vadd_VwVw(Q6_V_lo_W(dY1192b), Q6_V_lo_W(dU2066v1634));
            sRO = Q6_Vw_vadd_VwVw(Q6_V_hi_W(dY1192b), Q6_V_lo_W(dU2066v1634));
            sBE = Q6_Vw_vadd_VwVw(Q6_V_lo_W(dY1192b), Q6_V_hi_W(dU2066v1634));
            sBO = Q6_Vw_vadd_VwVw(Q6_V_hi_W(dY1192b), Q6_V_hi_W(dU2066v1634));
            sG = Q6_Vuh_vasr_VwVwR_sat(sGO, sGE, const10);
            sR = Q6_Vuh_vasr_VwVwR_sat(sRO, sRE, const10);
            sB = Q6_Vuh_vasr_VwVwR_sat(sBO, sBE, const10);
            sG = Q6_Vh_vmin_VhVh(sG, sConst0xff);
            sR = Q6_Vh_vmin_VhVh(sR, sConst0xff);
            sB = Q6_Vh_vmin_VhVh(sB, sConst0xff);
            sIffG = Q6_Vb_vshuffe_VbVb(sConst0xff, sG);
            sIBR = Q6_Vb_vshuffe_VbVb(sB, sR);
            dIffBGR = Q6_W_vshuff_VVR(sIffG, sIBR, const_1);
            *prgb0++ = Q6_V_lo_W(dIffBGR);
            *prgb0++ = Q6_V_hi_W(dIffBGR);

            dY1192a = Q6_Wuw_vmpy_VuhRuh(Q6_V_hi_W(dY1x2), const1192);
            sGE = Q6_Vw_vadd_VwVw(Q6_V_lo_W(dY1192a), sV_833u_400);
            sGO = Q6_Vw_vadd_VwVw(Q6_V_hi_W(dY1192a), sV_833u_400);
            sRE = Q6_Vw_vadd_VwVw(Q6_V_lo_W(dY1192a), Q6_V_lo_W(dU2066v1634));
            sRO = Q6_Vw_vadd_VwVw(Q6_V_hi_W(dY1192a), Q6_V_lo_W(dU2066v1634));
            sBE = Q6_Vw_vadd_VwVw(Q6_V_lo_W(dY1192a), Q6_V_hi_W(dU2066v1634));
            sBO = Q6_Vw_vadd_VwVw(Q6_V_hi_W(dY1192a), Q6_V_hi_W(dU2066v1634));
            sG = Q6_Vuh_vasr_VwVwR_sat(sGO, sGE, const10);
            sR = Q6_Vuh_vasr_VwVwR_sat(sRO, sRE, const10);
            sB = Q6_Vuh_vasr_VwVwR_sat(sBO, sBE, const10);
            sIffG = Q6_Vub_vsat_VhVh(sConst0xff, sG);
            sIBR = Q6_Vub_vsat_VhVh(sB, sR);
            dIffBGR = Q6_W_vshuff_VVR(sIffG, sIBR, const_1);
            *prgb1++ = Q6_V_lo_W(dIffBGR);
            *prgb1++ = Q6_V_hi_W(dIffBGR);
        }

        for (int32_t idx = 0; idx < remainX; ++idx) {
            if (rotate == 0) {
                sx = sxAry[dx + idx];
                sx_ = MIN(sx + 1, srcWidth - 1);
            } else if (rotate == 90) {
                sy0 = sxAry[dx + idx];
                sy0_ = MIN(sy0 + 1, srcHeight - 1);
                int32_t iy1 = MIN(dx + 1 + idx, dstWidth - 1);
                sy1 = sxAry[iy1];
                sy1_ = MIN(sy1 + 1, srcHeight - 1);
                su0 = sy0 >> 1;
                su0_ = sy0_ >> 1;
            } else if (rotate == 180) {
                sx = MIN(srcWidth - sxAry[dx + idx], srcWidth - 1);
                sx_ = MAX(sx - 1, 0);
            } else {
                sy0 = MIN(srcHeight - sxAry[dx + idx], srcHeight - 1);
                sy0_ = MAX(sy0 - 1, 0);
                int32_t iy1 = MIN(dx + 1 + idx, dstWidth - 1);
                sy1 = MIN(srcHeight - sxAry[iy1], srcHeight - 1);
                sy1_ = MAX(sy1 - 1, 0);
                su0 = sy0 >> 1;
                su0_ = sy0_ >> 1;
            }

            pX0Y0y0[idx] = pSrcY[sy0 * srcWidth + sx];
            pX1Y0y0[idx] = pSrcY[sy0 * srcWidth + sx_];
            pX0Y1y0[idx] = pSrcY[sy0_ * srcWidth + sx];
            pX1Y1y0[idx] = pSrcY[sy0_ * srcWidth + sx_];
            pX0Y0y1[idx] = pSrcY[sy1 * srcWidth + sx];
            pX1Y0y1[idx] = pSrcY[sy1 * srcWidth + sx_];
            pX0Y1y1[idx] = pSrcY[sy1_ * srcWidth + sx];
            pX1Y1y1[idx] = pSrcY[sy1_ * srcWidth + sx_];

            int32_t sux = sx - sx % 2;
            int32_t sux_ = sx_ - sx_ % 2;
            if (idx % 2 == 0) {
                pX0Y0uv[idx] = pSrcU[su0 * srcWidth + sux];
                pX1Y0uv[idx] = pSrcU[su0 * srcWidth + sux_];
                pX0Y1uv[idx] = pSrcU[su0_ * srcWidth + sux];
                pX1Y1uv[idx] = pSrcU[su0_ * srcWidth + sux_];
            } else {
                pX0Y0uv[idx] = pSrcV[su0 * srcWidth + sux];
                pX1Y0uv[idx] = pSrcV[su0 * srcWidth + sux_];
                pX0Y1uv[idx] = pSrcV[su0_ * srcWidth + sux];
                pX1Y1uv[idx] = pSrcV[su0_ * srcWidth + sux_];
            }
        }

        //Load fu
        HVX_VectorPair *pU0 = (HVX_VectorPair *)(fuAry + dx);
        HVX_Vector vU0uhH = Q6_V_hi_W(pU0[0]);
        HVX_Vector vU0uhL = Q6_V_lo_W(pU0[0]);
        HVX_Vector vU1uhH = Q6_Vuh_vsub_VuhVuh_sat(vVScaleh, vU0uhH);
        HVX_Vector vU1uhL = Q6_Vuh_vsub_VuhVuh_sat(vVScaleh, vU0uhL);

        //compute Y0
        HVX_VectorPair wX0Y0h = Q6_Wuh_vzxt_Vub(vX0Y0y0[0]);
        HVX_VectorPair wX1Y0h = Q6_Wuh_vzxt_Vub(vX1Y0y0[0]);
        HVX_VectorPair wX0Y1h = Q6_Wuh_vzxt_Vub(vX0Y1y0[0]);
        HVX_VectorPair wX1Y1h = Q6_Wuh_vzxt_Vub(vX1Y1y0[0]);

        HVX_VectorPair s00H = Q6_Wuw_vadd_WuwWuw_sat(Q6_Wuw_vmpy_VuhVuh(vU1uhH, Q6_V_hi_W(wX0Y0h)), Q6_Wuw_vmpy_VuhVuh(vU0uhH, Q6_V_hi_W(wX1Y0h)));
        HVX_VectorPair s00L = Q6_Wuw_vadd_WuwWuw_sat(Q6_Wuw_vmpy_VuhVuh(vU1uhH, Q6_V_lo_W(wX0Y0h)), Q6_Wuw_vmpy_VuhVuh(vU0uhH, Q6_V_lo_W(wX1Y0h)));
        HVX_VectorPair s01H = Q6_Wuw_vadd_WuwWuw_sat(Q6_Wuw_vmpy_VuhVuh(vU1uhH, Q6_V_hi_W(wX0Y1h)), Q6_Wuw_vmpy_VuhVuh(vU0uhH, Q6_V_hi_W(wX1Y1h)));
        HVX_VectorPair s01L = Q6_Wuw_vadd_WuwWuw_sat(Q6_Wuw_vmpy_VuhVuh(vU1uhH, Q6_V_lo_W(wX0Y1h)), Q6_Wuw_vmpy_VuhVuh(vU0uhH, Q6_V_lo_W(wX1Y1h)));

        HVX_Vector s0H = Q6_Vuh_vasr_VuwVuwR_sat(Q6_V_hi_W(s00H), Q6_V_lo_W(s00H), 8);
        HVX_Vector s0L = Q6_Vuh_vasr_VuwVuwR_sat(Q6_V_hi_W(s00L), Q6_V_lo_W(s00L), 8);
        HVX_Vector s1H = Q6_Vuh_vasr_VuwVuwR_sat(Q6_V_hi_W(s01H), Q6_V_lo_W(s01H), 8);
        HVX_Vector s1L = Q6_Vuh_vasr_VuwVuwR_sat(Q6_V_hi_W(s01L), Q6_V_lo_W(s01L), 8);

        s00H = Q6_Wuw_vadd_WuwWuw_sat(Q6_Wuw_vmpy_VuhVuh(vV1uh0, s0H), Q6_Wuw_vmpy_VuhVuh(vV0uh0, s1H));
        s00L = Q6_Wuw_vadd_WuwWuw_sat(Q6_Wuw_vmpy_VuhVuh(vV1uh0, s0L), Q6_Wuw_vmpy_VuhVuh(vV0uh0, s1L));
        HVX_Vector s0 = Q6_Vuh_vasr_VuwVuwR_sat(Q6_V_hi_W(s00H), Q6_V_lo_W(s00H), 8);
        HVX_Vector s1 = Q6_Vuh_vasr_VuwVuwR_sat(Q6_V_hi_W(s00L), Q6_V_lo_W(s00L), 8);
        HVX_Vector resY0 = Q6_Vub_vsat_VhVh(s0, s1);

        //compute Y1
        wX0Y0h = Q6_Wuh_vzxt_Vub(vX0Y0y1[0]);
        wX1Y0h = Q6_Wuh_vzxt_Vub(vX1Y0y1[0]);
        wX0Y1h = Q6_Wuh_vzxt_Vub(vX0Y1y1[0]);
        wX1Y1h = Q6_Wuh_vzxt_Vub(vX1Y1y1[0]);

        s00H = Q6_Wuw_vadd_WuwWuw_sat(Q6_Wuw_vmpy_VuhVuh(vU1uhH, Q6_V_hi_W(wX0Y0h)), Q6_Wuw_vmpy_VuhVuh(vU0uhH, Q6_V_hi_W(wX1Y0h)));
        s00L = Q6_Wuw_vadd_WuwWuw_sat(Q6_Wuw_vmpy_VuhVuh(vU1uhH, Q6_V_lo_W(wX0Y0h)), Q6_Wuw_vmpy_VuhVuh(vU0uhH, Q6_V_lo_W(wX1Y0h)));
        s01H = Q6_Wuw_vadd_WuwWuw_sat(Q6_Wuw_vmpy_VuhVuh(vU1uhH, Q6_V_hi_W(wX0Y1h)), Q6_Wuw_vmpy_VuhVuh(vU0uhH, Q6_V_hi_W(wX1Y1h)));
        s01L = Q6_Wuw_vadd_WuwWuw_sat(Q6_Wuw_vmpy_VuhVuh(vU1uhH, Q6_V_lo_W(wX0Y1h)), Q6_Wuw_vmpy_VuhVuh(vU0uhH, Q6_V_lo_W(wX1Y1h)));

        s0H = Q6_Vuh_vasr_VuwVuwR_sat(Q6_V_hi_W(s00H), Q6_V_lo_W(s00H), 8);
        s0L = Q6_Vuh_vasr_VuwVuwR_sat(Q6_V_hi_W(s00L), Q6_V_lo_W(s00L), 8);
        s1H = Q6_Vuh_vasr_VuwVuwR_sat(Q6_V_hi_W(s01H), Q6_V_lo_W(s01H), 8);
        s1L = Q6_Vuh_vasr_VuwVuwR_sat(Q6_V_hi_W(s01L), Q6_V_lo_W(s01L), 8);

        s00H = Q6_Wuw_vadd_WuwWuw_sat(Q6_Wuw_vmpy_VuhVuh(vV1uh1, s0H), Q6_Wuw_vmpy_VuhVuh(vV0uh1, s1H));
        s00L = Q6_Wuw_vadd_WuwWuw_sat(Q6_Wuw_vmpy_VuhVuh(vV1uh1, s0L), Q6_Wuw_vmpy_VuhVuh(vV0uh1, s1L));
        s0 = Q6_Vuh_vasr_VuwVuwR_sat(Q6_V_hi_W(s00H), Q6_V_lo_W(s00H), 8);
        s1 = Q6_Vuh_vasr_VuwVuwR_sat(Q6_V_hi_W(s00L), Q6_V_lo_W(s00L), 8);
        HVX_Vector resY1 = Q6_Vub_vsat_VhVh(s0, s1);

        //compute UV
        wX0Y0h = Q6_Wuh_vzxt_Vub(vX0Y0uv[0]);
        wX1Y0h = Q6_Wuh_vzxt_Vub(vX1Y0uv[0]);
        wX0Y1h = Q6_Wuh_vzxt_Vub(vX0Y1uv[0]);
        wX1Y1h = Q6_Wuh_vzxt_Vub(vX1Y1uv[0]);

        s00H = Q6_Wuw_vadd_WuwWuw_sat(Q6_Wuw_vmpy_VuhVuh(vU1uhH, Q6_V_hi_W(wX0Y0h)), Q6_Wuw_vmpy_VuhVuh(vU0uhH, Q6_V_hi_W(wX1Y0h)));
        s00L = Q6_Wuw_vadd_WuwWuw_sat(Q6_Wuw_vmpy_VuhVuh(vU1uhH, Q6_V_lo_W(wX0Y0h)), Q6_Wuw_vmpy_VuhVuh(vU0uhH, Q6_V_lo_W(wX1Y0h)));
        s01H = Q6_Wuw_vadd_WuwWuw_sat(Q6_Wuw_vmpy_VuhVuh(vU1uhH, Q6_V_hi_W(wX0Y1h)), Q6_Wuw_vmpy_VuhVuh(vU0uhH, Q6_V_hi_W(wX1Y1h)));
        s01L = Q6_Wuw_vadd_WuwWuw_sat(Q6_Wuw_vmpy_VuhVuh(vU1uhH, Q6_V_lo_W(wX0Y1h)), Q6_Wuw_vmpy_VuhVuh(vU0uhH, Q6_V_lo_W(wX1Y1h)));
        s0H = Q6_Vuh_vasr_VuwVuwR_sat(Q6_V_hi_W(s00H), Q6_V_lo_W(s00H), 8);
        s0L = Q6_Vuh_vasr_VuwVuwR_sat(Q6_V_hi_W(s00L), Q6_V_lo_W(s00L), 8);
        s1H = Q6_Vuh_vasr_VuwVuwR_sat(Q6_V_hi_W(s01H), Q6_V_lo_W(s01H), 8);
        s1L = Q6_Vuh_vasr_VuwVuwR_sat(Q6_V_hi_W(s01L), Q6_V_lo_W(s01L), 8);

        s00H = Q6_Wuw_vadd_WuwWuw_sat(Q6_Wuw_vmpy_VuhVuh(vV1uh0, s0H), Q6_Wuw_vmpy_VuhVuh(vV0uh0, s1H));
        s00L = Q6_Wuw_vadd_WuwWuw_sat(Q6_Wuw_vmpy_VuhVuh(vV1uh0, s0L), Q6_Wuw_vmpy_VuhVuh(vV0uh0, s1L));
        s0 = Q6_Vuh_vasr_VuwVuwR_sat(Q6_V_hi_W(s00H), Q6_V_lo_W(s00H), 8);
        s1 = Q6_Vuh_vasr_VuwVuwR_sat(Q6_V_hi_W(s00L), Q6_V_lo_W(s00L), 8);
        HVX_Vector resUV = Q6_Vub_vsat_VhVh(s0, s1);

        sUv = Q6_Vb_vshuff_Vb(resUV);
        dUvx2 = Q6_Wh_vsub_VubVub(sUv, sConst128);
        sV_833u_400 = Q6_Vw_vdmpy_VhRh_sat(Q6_V_lo_W(dUvx2), const_400_833);
        dU2066v1634 = Q6_Ww_vmpy_VhRh(Q6_V_lo_W(dUvx2), const2066n1634);

        sY0 = Q6_Vub_vsub_VubVub_sat(resY0, sConst16);
        dY0x2 = Q6_Wuh_vunpack_Vub(sY0);

        sY1 = Q6_Vub_vsub_VubVub_sat(resY1, sConst16);
        dY1x2 = Q6_Wuh_vunpack_Vub(sY1);

        dY1192a = Q6_Wuw_vmpy_VuhRuh(Q6_V_lo_W(dY0x2), const1192);
        sGE = Q6_Vw_vadd_VwVw(Q6_V_lo_W(dY1192a), sV_833u_400);
        sGO = Q6_Vw_vadd_VwVw(Q6_V_hi_W(dY1192a), sV_833u_400);
        sRE = Q6_Vw_vadd_VwVw(Q6_V_lo_W(dY1192a), Q6_V_lo_W(dU2066v1634));
        sRO = Q6_Vw_vadd_VwVw(Q6_V_hi_W(dY1192a), Q6_V_lo_W(dU2066v1634));
        sBE = Q6_Vw_vadd_VwVw(Q6_V_lo_W(dY1192a), Q6_V_hi_W(dU2066v1634));
        sBO = Q6_Vw_vadd_VwVw(Q6_V_hi_W(dY1192a), Q6_V_hi_W(dU2066v1634));
        sG = Q6_Vuh_vasr_VwVwR_sat(sGO, sGE, const10);
        sR = Q6_Vuh_vasr_VwVwR_sat(sRO, sRE, const10);
        sB = Q6_Vuh_vasr_VwVwR_sat(sBO, sBE, const10);
        sIffG = Q6_Vub_vsat_VhVh(sConst0xff, sG);
        sIBR = Q6_Vub_vsat_VhVh(sB, sR);
        dIffBGR = Q6_W_vshuff_VVR(sIffG, sIBR, const_1);
        *prgb0++ = Q6_V_lo_W(dIffBGR);
        *prgb0++ = Q6_V_hi_W(dIffBGR);

        dY1192c = Q6_Wuw_vmpy_VuhRuh(Q6_V_lo_W(dY1x2), const1192);
        sGE = Q6_Vw_vadd_VwVw(Q6_V_lo_W(dY1192c), sV_833u_400);
        sGO = Q6_Vw_vadd_VwVw(Q6_V_hi_W(dY1192c), sV_833u_400);
        sRE = Q6_Vw_vadd_VwVw(Q6_V_lo_W(dY1192c), Q6_V_lo_W(dU2066v1634));
        sRO = Q6_Vw_vadd_VwVw(Q6_V_hi_W(dY1192c), Q6_V_lo_W(dU2066v1634));
        sBE = Q6_Vw_vadd_VwVw(Q6_V_lo_W(dY1192c), Q6_V_hi_W(dU2066v1634));
        sBO = Q6_Vw_vadd_VwVw(Q6_V_hi_W(dY1192c), Q6_V_hi_W(dU2066v1634));
        sG = Q6_Vuh_vasr_VwVwR_sat(sGO, sGE, const10);
        sR = Q6_Vuh_vasr_VwVwR_sat(sRO, sRE, const10);
        sB = Q6_Vuh_vasr_VwVwR_sat(sBO, sBE, const10);
        sG = Q6_Vh_vmin_VhVh(sG, sConst0xff);
        sR = Q6_Vh_vmin_VhVh(sR, sConst0xff);
        sB = Q6_Vh_vmin_VhVh(sB, sConst0xff);
        sIffG = Q6_Vb_vshuffe_VbVb(sConst0xff, sG);
        sIBR2 = Q6_Vb_vshuffe_VbVb(sB, sR);
        dIffBGR2 = Q6_W_vshuff_VVR(sIffG, sIBR2, const_1);
        *prgb1++ = Q6_V_lo_W(dIffBGR2);
        *prgb1++ = Q6_V_hi_W(dIffBGR2);

        sV_833u_400 = Q6_Vw_vdmpy_VhRh_sat(Q6_V_hi_W(dUvx2), const_400_833);
        dU2066v1634 = Q6_Ww_vmpy_VhRh(Q6_V_hi_W(dUvx2), const2066n1634);

        dY1192b = Q6_Wuw_vmpy_VuhRuh(Q6_V_hi_W(dY0x2), const1192);
        sGE = Q6_Vw_vadd_VwVw(Q6_V_lo_W(dY1192b), sV_833u_400);
        sGO = Q6_Vw_vadd_VwVw(Q6_V_hi_W(dY1192b), sV_833u_400);
        sRE = Q6_Vw_vadd_VwVw(Q6_V_lo_W(dY1192b), Q6_V_lo_W(dU2066v1634));
        sRO = Q6_Vw_vadd_VwVw(Q6_V_hi_W(dY1192b), Q6_V_lo_W(dU2066v1634));
        sBE = Q6_Vw_vadd_VwVw(Q6_V_lo_W(dY1192b), Q6_V_hi_W(dU2066v1634));
        sBO = Q6_Vw_vadd_VwVw(Q6_V_hi_W(dY1192b), Q6_V_hi_W(dU2066v1634));
        sG = Q6_Vuh_vasr_VwVwR_sat(sGO, sGE, const10);
        sR = Q6_Vuh_vasr_VwVwR_sat(sRO, sRE, const10);
        sB = Q6_Vuh_vasr_VwVwR_sat(sBO, sBE, const10);
        sG = Q6_Vh_vmin_VhVh(sG, sConst0xff);
        sR = Q6_Vh_vmin_VhVh(sR, sConst0xff);
        sB = Q6_Vh_vmin_VhVh(sB, sConst0xff);
        sIffG = Q6_Vb_vshuffe_VbVb(sConst0xff, sG);
        sIBR = Q6_Vb_vshuffe_VbVb(sB, sR);
        dIffBGR = Q6_W_vshuff_VVR(sIffG, sIBR, const_1);
        *prgb0++ = Q6_V_lo_W(dIffBGR);
        *prgb0++ = Q6_V_hi_W(dIffBGR);

        dY1192a = Q6_Wuw_vmpy_VuhRuh(Q6_V_hi_W(dY1x2), const1192);
        sGE = Q6_Vw_vadd_VwVw(Q6_V_lo_W(dY1192a), sV_833u_400);
        sGO = Q6_Vw_vadd_VwVw(Q6_V_hi_W(dY1192a), sV_833u_400);
        sRE = Q6_Vw_vadd_VwVw(Q6_V_lo_W(dY1192a), Q6_V_lo_W(dU2066v1634));
        sRO = Q6_Vw_vadd_VwVw(Q6_V_hi_W(dY1192a), Q6_V_lo_W(dU2066v1634));
        sBE = Q6_Vw_vadd_VwVw(Q6_V_lo_W(dY1192a), Q6_V_hi_W(dU2066v1634));
        sBO = Q6_Vw_vadd_VwVw(Q6_V_hi_W(dY1192a), Q6_V_hi_W(dU2066v1634));
        sG = Q6_Vuh_vasr_VwVwR_sat(sGO, sGE, const10);
        sR = Q6_Vuh_vasr_VwVwR_sat(sRO, sRE, const10);
        sB = Q6_Vuh_vasr_VwVwR_sat(sBO, sBE, const10);
        sIffG = Q6_Vub_vsat_VhVh(sConst0xff, sG);
        sIBR = Q6_Vub_vsat_VhVh(sB, sR);
        dIffBGR = Q6_W_vshuff_VVR(sIffG, sIBR, const_1);
        *prgb1++ = Q6_V_lo_W(dIffBGR);
        *prgb1++ = Q6_V_hi_W(dIffBGR);
    }

    for (; remainY > 0; remainY--, dy++) {
        HVX_Vector vV0uh0 = Q6_Vh_vsplat_R(fvAry[dy]);
        HVX_Vector vV1uh0 = Q6_Vh_vsub_VhVh(vVScaleh, vV0uh0);

        HVX_Vector *prgb0 = (HVX_Vector *)(pDst + dy * dstStride);

        int32_t sx, sx_, sy0, sy0_, su0, su0_;
        if (rotate == 0) {
            sy0 = syAry[dy];
            sy0_ = MIN(sy0 + 1, srcHeight - 1);
            su0 = sy0 >> 1;
            su0_ = sy0_ >> 1;
            L2fetch((unsigned int)(pSrcY + sy0 * srcWidth), L2FETCH_PARA);
            L2fetch((unsigned int)(pSrcU + su0 * srcWidth), L2FETCH_PARA);
        } else if (rotate == 90) {
            sx = MIN(srcWidth - syAry[dy], srcWidth - 1);
            sx_ = MAX(sx - 1, 0);
        } else if(rotate == 180) {
            sy0 = MIN(srcHeight - syAry[dy], srcHeight - 1);
            sy0_ = MAX(sy0 - 1, 0);
            su0 = sy0 >> 1;
            su0_ = sy0_ >> 1;
            L2fetch((unsigned int)(pSrcY + sy0 * srcWidth), L2FETCH_PARA);
            L2fetch((unsigned int)(pSrcU + su0 * srcWidth), L2FETCH_PARA);
        } else {
            sx = syAry[dy];
            sx_ = MIN(sx + 1, srcWidth - 1);
        }

        int32_t dx = 0;
        for (int32_t n = 0; n < nnX; n++, dx += VECLEN) {
            //load YUV to buf;
            for (int32_t idx = 0; idx < VECLEN; ++idx) {
                if (rotate == 0) {
                    sx = sxAry[dx + idx];
                    sx_ = MIN(sx + 1, srcWidth - 1);
                } else if (rotate == 90) {
                    sy0 = sxAry[dx + idx];
                    sy0_ = MIN(sy0 + 1, srcHeight - 1);
                    su0 = sy0 >> 1;
                    su0_ = sy0_ >> 1;
                } else if (rotate == 180) {
                    sx = MIN(srcWidth - sxAry[dx + idx], srcWidth - 1);
                    sx_ = MAX(sx - 1, 0);
                } else {
                    sy0 = MIN(srcHeight - sxAry[dx + idx], srcHeight - 1);
                    sy0_ = MAX(sy0 - 1, 0);
                    su0 = sy0 >> 1;
                    su0_ = sy0_ >> 1;
                }

                pX0Y0y0[idx] = pSrcY[sy0 * srcWidth + sx];
                pX1Y0y0[idx] = pSrcY[sy0 * srcWidth + sx_];
                pX0Y1y0[idx] = pSrcY[sy0_ * srcWidth + sx];
                pX1Y1y0[idx] = pSrcY[sy0_ * srcWidth + sx_];

                int32_t sux = sx - sx % 2;
                int32_t sux_ = sx_ - sx_ % 2;
                if (idx % 2 == 0) {
                    pX0Y0uv[idx] = pSrcU[su0 * srcWidth + sux];
                    pX1Y0uv[idx] = pSrcU[su0 * srcWidth + sux_];
                    pX0Y1uv[idx] = pSrcU[su0_ * srcWidth + sux];
                    pX1Y1uv[idx] = pSrcU[su0_ * srcWidth + sux_];
                } else {
                    pX0Y0uv[idx] = pSrcV[su0 * srcWidth + sux];
                    pX1Y0uv[idx] = pSrcV[su0 * srcWidth + sux_];
                    pX0Y1uv[idx] = pSrcV[su0_ * srcWidth + sux];
                    pX1Y1uv[idx] = pSrcV[su0_ * srcWidth + sux_];
                }
            }

            //Load fu
            HVX_VectorPair *pU0 = (HVX_VectorPair *)(fuAry + dx);
            HVX_Vector vU0uhH = Q6_V_hi_W(pU0[0]);
            HVX_Vector vU0uhL = Q6_V_lo_W(pU0[0]);
            HVX_Vector vU1uhH = Q6_Vuh_vsub_VuhVuh_sat(vVScaleh, vU0uhH);
            HVX_Vector vU1uhL = Q6_Vuh_vsub_VuhVuh_sat(vVScaleh, vU0uhL);

            //compute Y0
            HVX_VectorPair wX0Y0h = Q6_Wuh_vzxt_Vub(vX0Y0y0[0]);
            HVX_VectorPair wX1Y0h = Q6_Wuh_vzxt_Vub(vX1Y0y0[0]);
            HVX_VectorPair wX0Y1h = Q6_Wuh_vzxt_Vub(vX0Y1y0[0]);
            HVX_VectorPair wX1Y1h = Q6_Wuh_vzxt_Vub(vX1Y1y0[0]);

            HVX_VectorPair s00H = Q6_Wuw_vadd_WuwWuw_sat(Q6_Wuw_vmpy_VuhVuh(vU1uhH, Q6_V_hi_W(wX0Y0h)), Q6_Wuw_vmpy_VuhVuh(vU0uhH, Q6_V_hi_W(wX1Y0h)));
            HVX_VectorPair s00L = Q6_Wuw_vadd_WuwWuw_sat(Q6_Wuw_vmpy_VuhVuh(vU1uhH, Q6_V_lo_W(wX0Y0h)), Q6_Wuw_vmpy_VuhVuh(vU0uhH, Q6_V_lo_W(wX1Y0h)));
            HVX_VectorPair s01H = Q6_Wuw_vadd_WuwWuw_sat(Q6_Wuw_vmpy_VuhVuh(vU1uhH, Q6_V_hi_W(wX0Y1h)), Q6_Wuw_vmpy_VuhVuh(vU0uhH, Q6_V_hi_W(wX1Y1h)));
            HVX_VectorPair s01L = Q6_Wuw_vadd_WuwWuw_sat(Q6_Wuw_vmpy_VuhVuh(vU1uhH, Q6_V_lo_W(wX0Y1h)), Q6_Wuw_vmpy_VuhVuh(vU0uhH, Q6_V_lo_W(wX1Y1h)));

            HVX_Vector s0H = Q6_Vuh_vasr_VuwVuwR_sat(Q6_V_hi_W(s00H), Q6_V_lo_W(s00H), 8);
            HVX_Vector s0L = Q6_Vuh_vasr_VuwVuwR_sat(Q6_V_hi_W(s00L), Q6_V_lo_W(s00L), 8);
            HVX_Vector s1H = Q6_Vuh_vasr_VuwVuwR_sat(Q6_V_hi_W(s01H), Q6_V_lo_W(s01H), 8);
            HVX_Vector s1L = Q6_Vuh_vasr_VuwVuwR_sat(Q6_V_hi_W(s01L), Q6_V_lo_W(s01L), 8);

            s00H = Q6_Wuw_vadd_WuwWuw_sat(Q6_Wuw_vmpy_VuhVuh(vV1uh0, s0H), Q6_Wuw_vmpy_VuhVuh(vV0uh0, s1H));
            s00L = Q6_Wuw_vadd_WuwWuw_sat(Q6_Wuw_vmpy_VuhVuh(vV1uh0, s0L), Q6_Wuw_vmpy_VuhVuh(vV0uh0, s1L));
            HVX_Vector s0 = Q6_Vuh_vasr_VuwVuwR_sat(Q6_V_hi_W(s00H), Q6_V_lo_W(s00H), 8);
            HVX_Vector s1 = Q6_Vuh_vasr_VuwVuwR_sat(Q6_V_hi_W(s00L), Q6_V_lo_W(s00L), 8);
            HVX_Vector resY0 = Q6_Vub_vsat_VhVh(s0, s1);

            //compute UV
            wX0Y0h = Q6_Wuh_vzxt_Vub(vX0Y0uv[0]);
            wX1Y0h = Q6_Wuh_vzxt_Vub(vX1Y0uv[0]);
            wX0Y1h = Q6_Wuh_vzxt_Vub(vX0Y1uv[0]);
            wX1Y1h = Q6_Wuh_vzxt_Vub(vX1Y1uv[0]);

            s00H = Q6_Wuw_vadd_WuwWuw_sat(Q6_Wuw_vmpy_VuhVuh(vU1uhH, Q6_V_hi_W(wX0Y0h)), Q6_Wuw_vmpy_VuhVuh(vU0uhH, Q6_V_hi_W(wX1Y0h)));
            s00L = Q6_Wuw_vadd_WuwWuw_sat(Q6_Wuw_vmpy_VuhVuh(vU1uhH, Q6_V_lo_W(wX0Y0h)), Q6_Wuw_vmpy_VuhVuh(vU0uhH, Q6_V_lo_W(wX1Y0h)));
            s01H = Q6_Wuw_vadd_WuwWuw_sat(Q6_Wuw_vmpy_VuhVuh(vU1uhH, Q6_V_hi_W(wX0Y1h)), Q6_Wuw_vmpy_VuhVuh(vU0uhH, Q6_V_hi_W(wX1Y1h)));
            s01L = Q6_Wuw_vadd_WuwWuw_sat(Q6_Wuw_vmpy_VuhVuh(vU1uhH, Q6_V_lo_W(wX0Y1h)), Q6_Wuw_vmpy_VuhVuh(vU0uhH, Q6_V_lo_W(wX1Y1h)));
            s0H = Q6_Vuh_vasr_VuwVuwR_sat(Q6_V_hi_W(s00H), Q6_V_lo_W(s00H), 8);
            s0L = Q6_Vuh_vasr_VuwVuwR_sat(Q6_V_hi_W(s00L), Q6_V_lo_W(s00L), 8);
            s1H = Q6_Vuh_vasr_VuwVuwR_sat(Q6_V_hi_W(s01H), Q6_V_lo_W(s01H), 8);
            s1L = Q6_Vuh_vasr_VuwVuwR_sat(Q6_V_hi_W(s01L), Q6_V_lo_W(s01L), 8);

            s00H = Q6_Wuw_vadd_WuwWuw_sat(Q6_Wuw_vmpy_VuhVuh(vV1uh0, s0H), Q6_Wuw_vmpy_VuhVuh(vV0uh0, s1H));
            s00L = Q6_Wuw_vadd_WuwWuw_sat(Q6_Wuw_vmpy_VuhVuh(vV1uh0, s0L), Q6_Wuw_vmpy_VuhVuh(vV0uh0, s1L));
            s0 = Q6_Vuh_vasr_VuwVuwR_sat(Q6_V_hi_W(s00H), Q6_V_lo_W(s00H), 8);
            s1 = Q6_Vuh_vasr_VuwVuwR_sat(Q6_V_hi_W(s00L), Q6_V_lo_W(s00L), 8);
            HVX_Vector resUV = Q6_Vub_vsat_VhVh(s0, s1);

            sUv = Q6_Vb_vshuff_Vb(resUV);
            dUvx2 = Q6_Wh_vsub_VubVub(sUv, sConst128);
            sV_833u_400 = Q6_Vw_vdmpy_VhRh_sat(Q6_V_lo_W(dUvx2), const_400_833);
            dU2066v1634 = Q6_Ww_vmpy_VhRh(Q6_V_lo_W(dUvx2), const2066n1634);

            sY0 = Q6_Vub_vsub_VubVub_sat(resY0, sConst16);
            dY0x2 = Q6_Wuh_vunpack_Vub(sY0);

            dY1192a = Q6_Wuw_vmpy_VuhRuh(Q6_V_lo_W(dY0x2), const1192);
            sGE = Q6_Vw_vadd_VwVw(Q6_V_lo_W(dY1192a), sV_833u_400);
            sGO = Q6_Vw_vadd_VwVw(Q6_V_hi_W(dY1192a), sV_833u_400);
            sRE = Q6_Vw_vadd_VwVw(Q6_V_lo_W(dY1192a), Q6_V_lo_W(dU2066v1634));
            sRO = Q6_Vw_vadd_VwVw(Q6_V_hi_W(dY1192a), Q6_V_lo_W(dU2066v1634));
            sBE = Q6_Vw_vadd_VwVw(Q6_V_lo_W(dY1192a), Q6_V_hi_W(dU2066v1634));
            sBO = Q6_Vw_vadd_VwVw(Q6_V_hi_W(dY1192a), Q6_V_hi_W(dU2066v1634));
            sG = Q6_Vuh_vasr_VwVwR_sat(sGO, sGE, const10);
            sR = Q6_Vuh_vasr_VwVwR_sat(sRO, sRE, const10);
            sB = Q6_Vuh_vasr_VwVwR_sat(sBO, sBE, const10);
            sIffG = Q6_Vub_vsat_VhVh(sConst0xff, sG);
            sIBR = Q6_Vub_vsat_VhVh(sB, sR);
            dIffBGR = Q6_W_vshuff_VVR(sIffG, sIBR, const_1);
            *prgb0++ = Q6_V_lo_W(dIffBGR);
            *prgb0++ = Q6_V_hi_W(dIffBGR);

            sV_833u_400 = Q6_Vw_vdmpy_VhRh_sat(Q6_V_hi_W(dUvx2), const_400_833);
            dU2066v1634 = Q6_Ww_vmpy_VhRh(Q6_V_hi_W(dUvx2), const2066n1634);

            dY1192b = Q6_Wuw_vmpy_VuhRuh(Q6_V_hi_W(dY0x2), const1192);
            sGE = Q6_Vw_vadd_VwVw(Q6_V_lo_W(dY1192b), sV_833u_400);
            sGO = Q6_Vw_vadd_VwVw(Q6_V_hi_W(dY1192b), sV_833u_400);
            sRE = Q6_Vw_vadd_VwVw(Q6_V_lo_W(dY1192b), Q6_V_lo_W(dU2066v1634));
            sRO = Q6_Vw_vadd_VwVw(Q6_V_hi_W(dY1192b), Q6_V_lo_W(dU2066v1634));
            sBE = Q6_Vw_vadd_VwVw(Q6_V_lo_W(dY1192b), Q6_V_hi_W(dU2066v1634));
            sBO = Q6_Vw_vadd_VwVw(Q6_V_hi_W(dY1192b), Q6_V_hi_W(dU2066v1634));
            sG = Q6_Vuh_vasr_VwVwR_sat(sGO, sGE, const10);
            sR = Q6_Vuh_vasr_VwVwR_sat(sRO, sRE, const10);
            sB = Q6_Vuh_vasr_VwVwR_sat(sBO, sBE, const10);
            sG = Q6_Vh_vmin_VhVh(sG, sConst0xff);
            sR = Q6_Vh_vmin_VhVh(sR, sConst0xff);
            sB = Q6_Vh_vmin_VhVh(sB, sConst0xff);
            sIffG = Q6_Vb_vshuffe_VbVb(sConst0xff, sG);
            sIBR = Q6_Vb_vshuffe_VbVb(sB, sR);
            dIffBGR = Q6_W_vshuff_VVR(sIffG, sIBR, const_1);
            *prgb0++ = Q6_V_lo_W(dIffBGR);
            *prgb0++ = Q6_V_hi_W(dIffBGR);
        }

        //load YUV to buf;
        for (int32_t idx = 0; idx < remainX; ++idx) {
            if (rotate == 0) {
                sx = sxAry[dx + idx];
                sx_ = MIN(sx + 1, srcWidth - 1);
            } else if (rotate == 90) {
                sy0 = sxAry[dx + idx];
                sy0_ = MIN(sy0 + 1, srcHeight - 1);
                su0 = sy0 >> 1;
                su0_ = sy0_ >> 1;
            } else if (rotate == 180) {
                sx = MIN(srcWidth - sxAry[dx + idx], srcWidth - 1);
                sx_ = MAX(sx - 1, 0);
            } else {
                sy0 = MIN(srcHeight - sxAry[dx + idx], srcHeight - 1);
                sy0_ = MAX(sy0 - 1, 0);
                su0 = sy0 >> 1;
                su0_ = sy0_ >> 1;
            }

            pX0Y0y0[idx] = pSrcY[sy0 * srcWidth + sx];
            pX1Y0y0[idx] = pSrcY[sy0 * srcWidth + sx_];
            pX0Y1y0[idx] = pSrcY[sy0_ * srcWidth + sx];
            pX1Y1y0[idx] = pSrcY[sy0_ * srcWidth + sx_];

            int32_t sux = sx - sx % 2;
            int32_t sux_ = sx_ - sx_ % 2;
            if (idx % 2 == 0) {
                pX0Y0uv[idx] = pSrcU[su0 * srcWidth + sux];
                pX1Y0uv[idx] = pSrcU[su0 * srcWidth + sux_];
                pX0Y1uv[idx] = pSrcU[su0_ * srcWidth + sux];
                pX1Y1uv[idx] = pSrcU[su0_ * srcWidth + sux_];
            } else {
                pX0Y0uv[idx] = pSrcV[su0 * srcWidth + sux];
                pX1Y0uv[idx] = pSrcV[su0 * srcWidth + sux_];
                pX0Y1uv[idx] = pSrcV[su0_ * srcWidth + sux];
                pX1Y1uv[idx] = pSrcV[su0_ * srcWidth + sux_];
            }
        }

        //Load fu
        HVX_VectorPair *pU0 = (HVX_VectorPair *)(fuAry + dx);
        HVX_Vector vU0uhH = Q6_V_hi_W(pU0[0]);
        HVX_Vector vU0uhL = Q6_V_lo_W(pU0[0]);
        HVX_Vector vU1uhH = Q6_Vuh_vsub_VuhVuh_sat(vVScaleh, vU0uhH);
        HVX_Vector vU1uhL = Q6_Vuh_vsub_VuhVuh_sat(vVScaleh, vU0uhL);

        //compute Y0
        HVX_VectorPair wX0Y0h = Q6_Wuh_vzxt_Vub(vX0Y0y0[0]);
        HVX_VectorPair wX1Y0h = Q6_Wuh_vzxt_Vub(vX1Y0y0[0]);
        HVX_VectorPair wX0Y1h = Q6_Wuh_vzxt_Vub(vX0Y1y0[0]);
        HVX_VectorPair wX1Y1h = Q6_Wuh_vzxt_Vub(vX1Y1y0[0]);

        HVX_VectorPair s00H = Q6_Wuw_vadd_WuwWuw_sat(Q6_Wuw_vmpy_VuhVuh(vU1uhH, Q6_V_hi_W(wX0Y0h)), Q6_Wuw_vmpy_VuhVuh(vU0uhH, Q6_V_hi_W(wX1Y0h)));
        HVX_VectorPair s00L = Q6_Wuw_vadd_WuwWuw_sat(Q6_Wuw_vmpy_VuhVuh(vU1uhH, Q6_V_lo_W(wX0Y0h)), Q6_Wuw_vmpy_VuhVuh(vU0uhH, Q6_V_lo_W(wX1Y0h)));
        HVX_VectorPair s01H = Q6_Wuw_vadd_WuwWuw_sat(Q6_Wuw_vmpy_VuhVuh(vU1uhH, Q6_V_hi_W(wX0Y1h)), Q6_Wuw_vmpy_VuhVuh(vU0uhH, Q6_V_hi_W(wX1Y1h)));
        HVX_VectorPair s01L = Q6_Wuw_vadd_WuwWuw_sat(Q6_Wuw_vmpy_VuhVuh(vU1uhH, Q6_V_lo_W(wX0Y1h)), Q6_Wuw_vmpy_VuhVuh(vU0uhH, Q6_V_lo_W(wX1Y1h)));

        HVX_Vector s0H = Q6_Vuh_vasr_VuwVuwR_sat(Q6_V_hi_W(s00H), Q6_V_lo_W(s00H), 8);
        HVX_Vector s0L = Q6_Vuh_vasr_VuwVuwR_sat(Q6_V_hi_W(s00L), Q6_V_lo_W(s00L), 8);
        HVX_Vector s1H = Q6_Vuh_vasr_VuwVuwR_sat(Q6_V_hi_W(s01H), Q6_V_lo_W(s01H), 8);
        HVX_Vector s1L = Q6_Vuh_vasr_VuwVuwR_sat(Q6_V_hi_W(s01L), Q6_V_lo_W(s01L), 8);

        s00H = Q6_Wuw_vadd_WuwWuw_sat(Q6_Wuw_vmpy_VuhVuh(vV1uh0, s0H), Q6_Wuw_vmpy_VuhVuh(vV0uh0, s1H));
        s00L = Q6_Wuw_vadd_WuwWuw_sat(Q6_Wuw_vmpy_VuhVuh(vV1uh0, s0L), Q6_Wuw_vmpy_VuhVuh(vV0uh0, s1L));
        HVX_Vector s0 = Q6_Vuh_vasr_VuwVuwR_sat(Q6_V_hi_W(s00H), Q6_V_lo_W(s00H), 8);
        HVX_Vector s1 = Q6_Vuh_vasr_VuwVuwR_sat(Q6_V_hi_W(s00L), Q6_V_lo_W(s00L), 8);
        HVX_Vector resY0 = Q6_Vub_vsat_VhVh(s0, s1);

        //compute UV
        wX0Y0h = Q6_Wuh_vzxt_Vub(vX0Y0uv[0]);
        wX1Y0h = Q6_Wuh_vzxt_Vub(vX1Y0uv[0]);
        wX0Y1h = Q6_Wuh_vzxt_Vub(vX0Y1uv[0]);
        wX1Y1h = Q6_Wuh_vzxt_Vub(vX1Y1uv[0]);

        s00H = Q6_Wuw_vadd_WuwWuw_sat(Q6_Wuw_vmpy_VuhVuh(vU1uhH, Q6_V_hi_W(wX0Y0h)), Q6_Wuw_vmpy_VuhVuh(vU0uhH, Q6_V_hi_W(wX1Y0h)));
        s00L = Q6_Wuw_vadd_WuwWuw_sat(Q6_Wuw_vmpy_VuhVuh(vU1uhH, Q6_V_lo_W(wX0Y0h)), Q6_Wuw_vmpy_VuhVuh(vU0uhH, Q6_V_lo_W(wX1Y0h)));
        s01H = Q6_Wuw_vadd_WuwWuw_sat(Q6_Wuw_vmpy_VuhVuh(vU1uhH, Q6_V_hi_W(wX0Y1h)), Q6_Wuw_vmpy_VuhVuh(vU0uhH, Q6_V_hi_W(wX1Y1h)));
        s01L = Q6_Wuw_vadd_WuwWuw_sat(Q6_Wuw_vmpy_VuhVuh(vU1uhH, Q6_V_lo_W(wX0Y1h)), Q6_Wuw_vmpy_VuhVuh(vU0uhH, Q6_V_lo_W(wX1Y1h)));
        s0H = Q6_Vuh_vasr_VuwVuwR_sat(Q6_V_hi_W(s00H), Q6_V_lo_W(s00H), 8);
        s0L = Q6_Vuh_vasr_VuwVuwR_sat(Q6_V_hi_W(s00L), Q6_V_lo_W(s00L), 8);
        s1H = Q6_Vuh_vasr_VuwVuwR_sat(Q6_V_hi_W(s01H), Q6_V_lo_W(s01H), 8);
        s1L = Q6_Vuh_vasr_VuwVuwR_sat(Q6_V_hi_W(s01L), Q6_V_lo_W(s01L), 8);

        s00H = Q6_Wuw_vadd_WuwWuw_sat(Q6_Wuw_vmpy_VuhVuh(vV1uh0, s0H), Q6_Wuw_vmpy_VuhVuh(vV0uh0, s1H));
        s00L = Q6_Wuw_vadd_WuwWuw_sat(Q6_Wuw_vmpy_VuhVuh(vV1uh0, s0L), Q6_Wuw_vmpy_VuhVuh(vV0uh0, s1L));
        s0 = Q6_Vuh_vasr_VuwVuwR_sat(Q6_V_hi_W(s00H), Q6_V_lo_W(s00H), 8);
        s1 = Q6_Vuh_vasr_VuwVuwR_sat(Q6_V_hi_W(s00L), Q6_V_lo_W(s00L), 8);
        HVX_Vector resUV = Q6_Vub_vsat_VhVh(s0, s1);

        sUv = Q6_Vb_vshuff_Vb(resUV);
        dUvx2 = Q6_Wh_vsub_VubVub(sUv, sConst128);
        sV_833u_400 = Q6_Vw_vdmpy_VhRh_sat(Q6_V_lo_W(dUvx2), const_400_833);
        dU2066v1634 = Q6_Ww_vmpy_VhRh(Q6_V_lo_W(dUvx2), const2066n1634);

        sY0 = Q6_Vub_vsub_VubVub_sat(resY0, sConst16);
        dY0x2 = Q6_Wuh_vunpack_Vub(sY0);

        dY1192a = Q6_Wuw_vmpy_VuhRuh(Q6_V_lo_W(dY0x2), const1192);
        sGE = Q6_Vw_vadd_VwVw(Q6_V_lo_W(dY1192a), sV_833u_400);
        sGO = Q6_Vw_vadd_VwVw(Q6_V_hi_W(dY1192a), sV_833u_400);
        sRE = Q6_Vw_vadd_VwVw(Q6_V_lo_W(dY1192a), Q6_V_lo_W(dU2066v1634));
        sRO = Q6_Vw_vadd_VwVw(Q6_V_hi_W(dY1192a), Q6_V_lo_W(dU2066v1634));
        sBE = Q6_Vw_vadd_VwVw(Q6_V_lo_W(dY1192a), Q6_V_hi_W(dU2066v1634));
        sBO = Q6_Vw_vadd_VwVw(Q6_V_hi_W(dY1192a), Q6_V_hi_W(dU2066v1634));
        sG = Q6_Vuh_vasr_VwVwR_sat(sGO, sGE, const10);
        sR = Q6_Vuh_vasr_VwVwR_sat(sRO, sRE, const10);
        sB = Q6_Vuh_vasr_VwVwR_sat(sBO, sBE, const10);
        sIffG = Q6_Vub_vsat_VhVh(sConst0xff, sG);
        sIBR = Q6_Vub_vsat_VhVh(sB, sR);
        dIffBGR = Q6_W_vshuff_VVR(sIffG, sIBR, const_1);
        *prgb0++ = Q6_V_lo_W(dIffBGR);
        *prgb0++ = Q6_V_hi_W(dIffBGR);

        sV_833u_400 = Q6_Vw_vdmpy_VhRh_sat(Q6_V_hi_W(dUvx2), const_400_833);
        dU2066v1634 = Q6_Ww_vmpy_VhRh(Q6_V_hi_W(dUvx2), const2066n1634);

        dY1192b = Q6_Wuw_vmpy_VuhRuh(Q6_V_hi_W(dY0x2), const1192);
        sGE = Q6_Vw_vadd_VwVw(Q6_V_lo_W(dY1192b), sV_833u_400);
        sGO = Q6_Vw_vadd_VwVw(Q6_V_hi_W(dY1192b), sV_833u_400);
        sRE = Q6_Vw_vadd_VwVw(Q6_V_lo_W(dY1192b), Q6_V_lo_W(dU2066v1634));
        sRO = Q6_Vw_vadd_VwVw(Q6_V_hi_W(dY1192b), Q6_V_lo_W(dU2066v1634));
        sBE = Q6_Vw_vadd_VwVw(Q6_V_lo_W(dY1192b), Q6_V_hi_W(dU2066v1634));
        sBO = Q6_Vw_vadd_VwVw(Q6_V_hi_W(dY1192b), Q6_V_hi_W(dU2066v1634));
        sG = Q6_Vuh_vasr_VwVwR_sat(sGO, sGE, const10);
        sR = Q6_Vuh_vasr_VwVwR_sat(sRO, sRE, const10);
        sB = Q6_Vuh_vasr_VwVwR_sat(sBO, sBE, const10);
        sG = Q6_Vh_vmin_VhVh(sG, sConst0xff);
        sR = Q6_Vh_vmin_VhVh(sR, sConst0xff);
        sB = Q6_Vh_vmin_VhVh(sB, sConst0xff);
        sIffG = Q6_Vb_vshuffe_VbVb(sConst0xff, sG);
        sIBR = Q6_Vb_vshuffe_VbVb(sB, sR);
        dIffBGR = Q6_W_vshuff_VVR(sIffG, sIBR, const_1);
        *prgb0++ = Q6_V_lo_W(dIffBGR);
        *prgb0++ = Q6_V_hi_W(dIffBGR);
    }

    if (buf != NULL) {
        free(buf);
        buf = NULL;
    }
    dspCV_worker_pool_synctoken_jobdone(dptr->token);
}

int pre_process_nv12_hvx(const uint8 *pSrc, int pSrcLen, int srcWidth, int srcHeight,
                         uint8 *pDst, int pDstLen, int dstWidth, int dstHeight, int rotate)
{
    //get vctm
    compute_res_attr_t res_info;
    unsigned int context_id = 0;
    unsigned char *vtcm = NULL;

    int res = HAP_compute_res_attr_init(&res_info);
    if (res != HAP_COMPUTE_RES_NOT_SUPPORTED) {
        HAP_compute_res_attr_set_serialize(&res_info, 1);
        HAP_compute_res_attr_set_vtcm_param(&res_info, 64 * 1024, 0);
        context_id = HAP_compute_res_acquire(&res_info, 100000);
        if (context_id == 0) {
            return -1;
        }

        vtcm = HAP_compute_res_attr_get_vtcm_ptr(&res_info);
    } else {
        vtcm = HAP_request_VTCM(64 * 1024, 1);
    }

    (void)dspCV_hvx_power_on();

    (void)dspCV_worker_pool_init();

    setClocks();

    float xratio = 0.f;
    float yratio = 0.f;
    if (rotate == 0 || rotate == 180) {
        xratio = (float)srcWidth / dstWidth;
        yratio = (float)srcHeight / dstHeight;
    } else {
        xratio = (float)srcHeight / dstWidth;
        yratio = (float)srcWidth / dstHeight;
    }

    //计算xy在原图上的坐标
    uint8_t *ptr = (uint8_t *)vtcm;
    int *sxAry = (int *)ptr;
    ptr += roundup_t(dstWidth * sizeof(int), VECLEN);

    int *syAry = (int *)ptr;
    ptr += roundup_t(dstHeight * sizeof(int), VECLEN);

    uint16_t *fuAry = (uint16_t *)ptr;
    ptr += roundup_t(dstWidth * sizeof(uint16_t), VECLEN);

    uint16_t *fvAry = (uint16_t *)ptr;
    ptr += roundup_t(dstHeight * sizeof(uint16_t), VECLEN);

    const int scale = 1 << 8;
    for (int dx = 0; dx < dstWidth; ++dx) {
        float fx = (float)((dx + 0.5) * xratio - 0.5);

        int x = (int)floor(fx);
        float u = fx - x;
        if (x < 0) {
            x = 0;
            u = 0.f;
        } else if (rotate == 0 || rotate == 180) {
            if (x >= srcWidth) {
                x = srcWidth - 1;
                u = 0.f;
            }
        } else if (x >= srcHeight) {
            x = srcHeight - 1;
            u = 0.f;
        }

        sxAry[dx] = x;
        fuAry[dx] = (uint16_t)(u * scale);
    }

    for (int dy = 0; dy < dstHeight; ++dy) {
        float fy = (float)((dy + 0.5f) * yratio - 0.5f);

        int y = (int)floor(fy);
        float v = fy - y;
        if (y < 0) {
            y = 0;
            v = 0.f;
        } else if(rotate == 0 || rotate == 180) {
            if (y >= srcHeight) {
                y = srcHeight - 1;
                v = 0.f;
            }
        } else if (y >= srcWidth) {
            y = srcWidth - 1;
            v = 0.f;
        }

        syAry[dy] = y;
        fvAry[dy] = (uint16_t)(v * scale);
    }

    dspCV_worker_job_t job;
    dspCV_synctoken_t token;
    nv12hvx_callback_t dptr;
    dptr.retval = 0;
    dptr.token = &token;

    dptr.pSrcImg = pSrc;
    dptr.srcWidth = srcWidth;
    dptr.srcHeight = srcHeight;
    dptr.pDstImg = pDst;
    dptr.dstWidth = dstWidth;
    dptr.dstHeight = dstHeight;
    dptr.rotate = rotate;

    dptr.sxAry = sxAry;
    dptr.syAry = syAry;
    dptr.fuAry = fuAry;
    dptr.fvAry = fvAry;
    dptr.threadIdx = 0;

    int numWorkers = 2;
    dptr.threadCount = numWorkers;
    job.dptr = (void *)&dptr;
    job.fptr = pre_process_nv12_callback;
    dspCV_worker_pool_synctoken_init(&token, numWorkers);

    for (int i = 0; i < numWorkers; ++i) {
        dspCV_worker_pool_submit(job);
    }

    dspCV_worker_pool_synctoken_wait(&token);
    dspCV_worker_pool_deinit();

    if (res != HAP_COMPUTE_RES_NOT_SUPPORTED) {
        HAP_compute_res_release(context_id);
    } else if (vtcm != NULL) {
        HAP_release_VTCM(vtcm);
        vtcm = NULL;
    }

    return 0;
}