#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "loadimg.h"

typedef struct tagAM_BMPINFOHEADER
{
	unsigned int biSize;
	int biWidth;
	int biHeight;
	unsigned short biPlanes;
	unsigned short biBitCount;
	unsigned int biCompression;
	unsigned int biSizeImage;
	int biXPelsPerMeter;
	int biYPelsPerMeter;
	unsigned int biClrUsed;
	unsigned int biClrImportant;
} AM_BMPINFOHEADER;

typedef struct tagAM_RGBQUAD
{
	unsigned char rgbBlue;
	unsigned char rgbGreen;
	unsigned char rgbRed;
	unsigned char rgbReserved;
} AM_RGBQUAD, * AM_LPRGBQUAD;

long LoadBMP(const char* szFile, unsigned char** pBuffer, long* nWidth, long* nHeight, long* nBitCount)
{
	int bRet = 0;
	FILE* stream = 0;
	unsigned char* pImageData = 0;

#ifdef _MSC_VER
	errno_t errno;
	errno = fopen_s(&stream, szFile, "rb");
	if (0 != errno) {
		printf("Error(%d): open file(%s).\n", errno, szFile);
	}
#else
	stream = fopen(szFile, "rb");
#endif
	if (stream)
	{
		int nOffset, nLineBytes, i;
		unsigned char* p;
		unsigned short wTemp;
		AM_BMPINFOHEADER bi;
		fread(&wTemp, 1, 2, stream);
		if (wTemp != 0x4D42)
			goto exit;
		fseek(stream, 10, SEEK_SET);
		fread(&nOffset, 1, 4, stream);
		fread(&bi, 1, sizeof(AM_BMPINFOHEADER), stream);
		if (bi.biBitCount != 24 && bi.biBitCount != 16 && bi.biBitCount != 8)
			goto exit;

		*nWidth = bi.biWidth;
		*nHeight = bi.biHeight;
		*nBitCount = bi.biBitCount;
		nLineBytes = LINE_BYTES(bi.biWidth, bi.biBitCount);
		pImageData = (unsigned char*)malloc(nLineBytes * bi.biHeight);
		if (!pImageData)
			goto exit;

		fseek(stream, nOffset, SEEK_SET);
		p = pImageData + (bi.biHeight - 1) * nLineBytes;
		for (i = 0; i < bi.biHeight; i++, p -= nLineBytes)
			fread(p, 1, nLineBytes, stream);
		bRet = 1;
	}

exit:
	if (bRet == 0 && pImageData)
		free(pImageData);
	else
		*pBuffer = pImageData;
	if (stream)
		fclose(stream);
	return bRet;
}

long SaveBMP(const char* szFile, unsigned char* pData, long nWidth, long nHeight, long nBitCount)
{
	FILE* stream = 0;
	unsigned char* p;
	unsigned short wTemp;
	unsigned int dwTemp;
	AM_BMPINFOHEADER bi;
	int nLineBytes, i;

#ifdef _MSC_VER
	errno_t errno;
	errno = fopen_s(&stream, szFile, "wb+");
	if (0 != errno) {
		printf("Error(%d): open file(%s).\n", errno, szFile);
	}
#else
	stream = fopen(szFile, "wb+");
#endif
	if (!stream)
		return 0;

	nLineBytes = LINE_BYTES(nWidth, nBitCount);
	wTemp = 0x4D42;
	dwTemp = 54 + nLineBytes * nHeight;
	if (nBitCount == 16)
		dwTemp += 4 * sizeof(unsigned int);
	else if (nBitCount == 8)
		dwTemp += 256 * sizeof(AM_RGBQUAD);
	fwrite(&wTemp, 1, 2, stream);
	fwrite(&dwTemp, 1, 4, stream);
	wTemp = 0;
	dwTemp = 54;
	if (nBitCount == 16)
		dwTemp += 4 * sizeof(unsigned int);
	else if (nBitCount == 8)
		dwTemp += 256 * sizeof(AM_RGBQUAD);
	fwrite(&wTemp, 1, 2, stream);
	fwrite(&wTemp, 1, 2, stream);
	fwrite(&dwTemp, 1, 4, stream);
	memset(&bi, 0, sizeof(AM_BMPINFOHEADER));
	bi.biSize = sizeof(AM_BMPINFOHEADER);
	bi.biWidth = nWidth;
	bi.biHeight = nHeight;
	bi.biBitCount = (unsigned short)nBitCount;
	bi.biPlanes = 1;
	if (nBitCount == 16)
		bi.biCompression = 3L;
	fwrite(&bi, 1, sizeof(AM_BMPINFOHEADER), stream);

	if (nBitCount == 16)
	{
		unsigned int mask[4] = { 0xF800, 0x7E0, 0x1F, 0 };
		fwrite(mask, 4, sizeof(unsigned int), stream);
	}
	else if (nBitCount == 8)
	{
		AM_RGBQUAD rgb[256];
		for (i = 0; i < 256; i++)
		{
			rgb[i].rgbBlue = (unsigned char)i;
			rgb[i].rgbGreen = (unsigned char)i;
			rgb[i].rgbRed = (unsigned char)i;
			rgb[i].rgbReserved = 0;
		}
		fwrite(rgb, 256, sizeof(AM_RGBQUAD), stream);
	}
	p = pData + (nHeight - 1) * nLineBytes;
	for (i = 0; i < nHeight; i++, p -= nLineBytes)
		fwrite(p, 1, nLineBytes, stream);

	fclose(stream);
	return 1;
}

long SaveRGBA4stride(const char* szFile, unsigned char* pData, long nWidth, long nHeight)
{
	long res = 0;
	unsigned char* pData24 = (unsigned char*)malloc(LINE_BYTES(nWidth, 24) * nHeight);
	if (0 == pData24)
		return -1;
	unsigned char* pData24Tmp = 0;
	unsigned char* pDataTmp = 0;
	for (int j = 0; j < nHeight; ++j)
	{
		/* code */
		pData24Tmp = pData24 + j * LINE_BYTES(nWidth, 24);
		pDataTmp = pData + j * nWidth * 4;
		for (int i = 0; i < nWidth; ++i)
		{
			/* code */
			pData24Tmp[3 * i + 0] = pDataTmp[4 * i + 0];
			pData24Tmp[3 * i + 1] = pDataTmp[4 * i + 1];
			pData24Tmp[3 * i + 2] = pDataTmp[4 * i + 2];
		}
	}
	SaveBMP(szFile, pData24, nWidth, nHeight, 24);
	free(pData24);
	pData24 = 0;
	return res;
}

long LoadYUV(const char* szFile, long nWidth, long nHeight, unsigned char** pBuffer)
{
	int bRet = 0;
	unsigned char* pImageData = 0;
	FILE* fp = NULL;
#ifdef _MSC_VER
	errno_t errno;
	errno = fopen_s(&fp, szFile, "rb");
	if (0 != errno) {
		printf("Error(%d): open file(%s).\n", errno, szFile);
	}
#else
	fp = fopen(szFile, "rb");
#endif

	long bytes_read;
	long npixels;

	npixels = nWidth * nHeight;

	pImageData = (unsigned char*)malloc(npixels + npixels / 4 + npixels / 4);
	if (!pImageData)
		goto exit;

	bytes_read = fread(pImageData, sizeof(unsigned char), npixels, fp);
	if (bytes_read == 0)
		goto exit;
	else if (bytes_read != npixels)
		goto exit;
	bytes_read = fread(pImageData + npixels, sizeof(unsigned char), npixels / 4, fp);
	if (bytes_read != npixels / 4)
		goto exit;

	bytes_read = fread(pImageData + npixels + npixels / 4, sizeof(unsigned char), npixels / 4, fp);
	if (bytes_read != npixels / 4)
		goto exit;

	bRet = 1;
exit:
	if (bRet == 0 && pImageData)
		free(pImageData);
	else
		*pBuffer = pImageData;
	if (fp)
		fclose(fp);
	return bRet;
}

long LoadYUV2(const char* szFile, long nWidth, long nHeight, unsigned char** pBuffer)
{
	int bRet = 0;
	unsigned char* pImageData = 0;
	FILE* fin = NULL;
#ifdef _MSC_VER
	errno_t errno;
	errno = fopen_s(&fin, szFile, "rb");
	if (0 != errno) {
		printf("Error(%d): open file(%s).\n", errno, szFile);
	}
#else
	fin = fopen(szFile, "rb");
#endif
	if (!fin)
	{
		fprintf(stderr, "error: unable to open file: %s\n", szFile);
		return -1;
	}


	long bytes_read;
	long npixels;

	npixels = nWidth * nHeight;

	pImageData = (unsigned char*)malloc(npixels + npixels / 2 + npixels / 2);
	if (!pImageData)
		goto exit;

	bytes_read = fread(pImageData, sizeof(unsigned char), npixels, fin);
	if (bytes_read == 0)
		goto exit;
	else if (bytes_read != npixels)
		goto exit;
	bytes_read = fread(pImageData + npixels, sizeof(unsigned char), npixels / 2, fin);
	if (bytes_read != npixels / 2)
		goto exit;

	bytes_read = fread(pImageData + npixels + npixels / 2, sizeof(unsigned char), npixels / 2, fin);
	if (bytes_read != npixels / 2)
		goto exit;

	bRet = 1;
exit:
	if (bRet == 0 && pImageData)
		free(pImageData);
	else
		*pBuffer = pImageData;
	if (fin)
		fclose(fin);
	return bRet;
}
