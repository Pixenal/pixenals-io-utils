/* 
SPDX-FileCopyrightText: 2025 Caleb Dawson
SPDX-License-Identifier: Apache-2.0
*/

#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>

#include <pixenals_io_utils.h>

typedef int64_t I64;
typedef int32_t I32;
typedef uint8_t U8;

void pixioByteArrResize(
	const PixalcFPtrs *pAlloc,
	PixioByteArr *pByteArr,
	I64 bitOffset
) {
	I64 bitCount = ((pByteArr->byteIdx) * 8) + pByteArr->nextBitIdx;
	PIX_ERR_ASSERT("", bitCount <= pByteArr->size * 8);
	bitCount += bitOffset;
	I64 byteCount = bitCount / 8 + (bitCount % 8 != 0);
	PIXALC_DYN_ARR_RESIZE_ZERO(U8, pAlloc, pByteArr, byteCount);
}

static
I32 getByteLen(I32 bitLen) {
	I32 byteLen = bitLen / 8;
	byteLen += bitLen != byteLen * 8;
	return byteLen;
}

void pixioByteArrAlign(PixioByteArr *pByteArr) {
	if (pByteArr->nextBitIdx) {
		++pByteArr->byteIdx;
		pByteArr->nextBitIdx = 0;
	}
}

void pixioByteArrInc(PixioByteArr *pByteArr, I32 bitLen) {
	pByteArr->nextBitIdx += bitLen;
	pByteArr->byteIdx += pByteArr->nextBitIdx / 8;
	pByteArr->nextBitIdx %= 8;
}

void pixioByteArrWrite(
	const PixalcFPtrs *pAlloc,
	PixioByteArr *pByteArr,
	const void *pData,
	int32_t bitLen
) {
	PIX_ERR_ASSERT("", bitLen > 0);
	pixioByteArrResize(pAlloc, pByteArr, bitLen);
	U8 *pStart = pByteArr->pArr + pByteArr->byteIdx;

	I32 byteLenSrc = getByteLen(bitLen);
	if (!pByteArr->nextBitIdx && byteLenSrc * 8 == bitLen) {
		//start & size are aligned, so just memcpy
		memcpy(pStart, pData, byteLenSrc);
		pByteArr->byteIdx += byteLenSrc;
		return;
	}
	I32 byteLenDest = getByteLen(bitLen + pByteArr->nextBitIdx);
	pStart[0] |= ((U8 *)pData)[0] << pByteArr->nextBitIdx;
	for (I32 i = 1; i < byteLenDest; ++i) {
		pStart[i] = i == byteLenSrc ? 0x0 : ((U8 *)pData)[i] << pByteArr->nextBitIdx;
		U8 nextByte = ((U8 *)pData)[i - 1];
		nextByte >>= 8 - pByteArr->nextBitIdx;
		pStart[i] |= nextByte;
	}
	pixioByteArrInc(pByteArr, bitLen);
}

void pixioByteArrWriteStr(
	const PixalcFPtrs *pAlloc,
	PixioByteArr *pByteArr,
	const char *pStr
) {
	I32 byteLen = (I32)strlen(pStr) + 1;
	I32 bitLen = byteLen * 8;

	//+8 bits for for potential padding
	pixioByteArrResize(pAlloc, pByteArr, bitLen + 8);
	if (pByteArr->nextBitIdx != 0) {
		//pad to beginning of next byte
		pByteArr->nextBitIdx = 0;
		++pByteArr->byteIdx;
	}
	memcpy(pByteArr->pArr + pByteArr->byteIdx, pStr, byteLen);
	pByteArr->byteIdx += byteLen;
}

void pixioByteArrRead(PixioByteArr *pByteArr, void *pData, int32_t bitLen) {
	PIX_ERR_ASSERT("", bitLen > 0);
	U8 *pStart = pByteArr->pArr + pByteArr->byteIdx;
	I32 byteLenDest = getByteLen(bitLen);
	if (!pByteArr->nextBitIdx && byteLenDest * 8 == bitLen) {
		//size & start are aligned, so just memcpy
		memcpy(pData, pByteArr->pArr + pByteArr->byteIdx, byteLenDest);
		pByteArr->byteIdx += byteLenDest;
		return;
	}
	I32 byteLenSrc = getByteLen(bitLen + pByteArr->nextBitIdx);
	for (I32 i = 0; i < byteLenDest; ++i) {
		((U8 *)pData)[i] = pStart[i] >> pByteArr->nextBitIdx;
		if (i != byteLenSrc - 1) {
			U8 nextByte = pStart[i + 1];
			nextByte <<= 8 - pByteArr->nextBitIdx;
			((U8 *)pData)[i] |= nextByte;
		}
	}
	if (bitLen % 8) {
		U8 mask = UCHAR_MAX >> 8 - bitLen % 8;
		((U8 *)pData)[byteLenDest - 1] &= mask;
	}
	pixioByteArrInc(pByteArr, bitLen);
}

void pixioByteArrReadStr(PixioByteArr *pByteArr, char *pStr, int32_t maxLen) {
	pByteArr->byteIdx += pByteArr->nextBitIdx > 0;
	U8 *pSrc = pByteArr->pArr + pByteArr->byteIdx;
	I32 i = 0;
	for (; i < maxLen && pSrc[i]; ++i) {
		pStr[i] = pSrc[i];
	}
	pStr[i] = 0;
	pByteArr->byteIdx += i + 1;
	pByteArr->nextBitIdx = 0;
}