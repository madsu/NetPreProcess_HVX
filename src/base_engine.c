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