#include "rgba2bgr.h"

#if __ARM_NEON
#include <arm_neon.h>
#endif

void RGBA2BGR(uint8_t *rgba, int32_t width, int32_t height, int32_t stride, uint8_t *bgr)
{
    int32_t w = width;
    int32_t h = height;
    const int32_t wgap = stride - w * 4;
    if (wgap == 0) {
        w = w * h;
        h = 1;
    }

#if __ARM_NEON
    int32_t nn = w >> 3;
    int32_t remain = w - (nn << 3);
#else
    int32_t remain = w;
#endif

    for (int32_t y = 0; y < h; ++y) {
#if __ARM_NEON
        for (; nn > 0; nn--) {
            uint8x8x4_t _rgba = vld4_u8(rgba);
            uint8x8x3_t _bgr;
            _bgr.val[0] = _rgba.val[2];
            _bgr.val[1] = _rgba.val[1];
            _bgr.val[2] = _rgba.val[0];
            vst3_u8(bgr, _bgr);

            rgba += 4 * 8;
            bgr += 3 * 8;
        }
#endif

        for (; remain > 0; remain--) {
            bgr[0] = rgba[2];
            bgr[1] = rgba[1];
            bgr[2] = rgba[0];

            rgba += 4;
            bgr += 3;
        }

        rgba += wgap;
    }
}