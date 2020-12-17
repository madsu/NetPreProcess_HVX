#ifndef __LOADIMAGE__
#define __LOADIMAGE__

#include "tcomdef.h"
#include "tvloffscreen.h"
#include "terror.h"

#define LINE_BYTES(Width, BitCt) (((MLong)(Width) * (BitCt) + 31) / 32 * 4)

long LoadBMP(const char* szFile, unsigned char** pBuffer, long* nWidth, long* nHeight, long* nBitCount);
long SaveBMP(const char* szFile, unsigned char* pData, long nWidth, long nHeight, long nBitCount);
long SaveRGBA4stride(const char* szFile, unsigned char* pData, long nWidth, long nHeight);

long LoadYUV(const char* szFile, long nWidth, long nHeight, unsigned char** pBuffer);
long LoadYUV2(const char* szFile, long nWidth, long nHeight, unsigned char** pBuffer);

#endif 
