#ifndef _NET_PRE_PROCESS_H_
#define _NET_PRE_PROCESS_H_

void nv12_pre_process(const unsigned char *pSrc, int srcWidth, int srcHeight,
                      unsigned char *pDst, int dstWidth, int dstHeight, int rotate);

#endif