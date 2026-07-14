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

void pixioReallocByteArrIfNeeded(
	const PixalcFPtrs *pAlloc,
	PixioByteArr *pByteArr,
	I64 bitOffset
) {
	I64 bitCount = ((pByteArr->byteIdx) * 8) + pByteArr->nextBitIdx;
	PIX_ERR_ASSERT("", bitCount <= pByteArr->size * 8);
	bitCount += bitOffset;
	I64 byteCount = bitCount / 8 + (bitCount % 8 != 0);
	if (byteCount && byteCount >= pByteArr->size) {
		I64 oldSize = pByteArr->size;
		pByteArr->size = byteCount * 2;
		pByteArr->pArr = pAlloc->fpRealloc(pByteArr->pArr, pByteArr->size);
		memset(pByteArr->pArr + oldSize, 0, pByteArr->size - oldSize);
	}
}

static
I32 getByteLen(I32 bitLen) {
	I32 byteLen = bitLen / 8;
	byteLen += bitLen != byteLen * 8;
	return byteLen;
}

void pixioByteArrWrite(
	const PixalcFPtrs *pAlloc,
	PixioByteArr *pByteArr,
	const void *pData,
	int32_t bitLen
) {
	pixioReallocByteArrIfNeeded(pAlloc, pByteArr, bitLen);
	U8 *pStart = pByteArr->pArr + pByteArr->byteIdx;

	I32 byteLen = getByteLen(bitLen);
	if (!pByteArr->nextBitIdx && byteLen * 8 == bitLen) {
		//start & size are aligned, so just memcpy
		memcpy(pStart, pData, byteLen);
		pByteArr->byteIdx += byteLen;
		return;
	}
	I32 strByteLen = getByteLen(bitLen + pByteArr->nextBitIdx);
	pStart[0] |= ((U8 *)pData)[0] << pByteArr->nextBitIdx;
	for (I32 i = 1; i < strByteLen; ++i) {
		pStart[i] = i == byteLen ? 0x0 : ((U8 *)pData)[i] << pByteArr->nextBitIdx;
		U8 nextByte = ((U8 *)pData)[i - 1];
		nextByte >>= 8 - pByteArr->nextBitIdx;
		pStart[i] |= nextByte;
	}
	pByteArr->nextBitIdx = pByteArr->nextBitIdx + bitLen;
	pByteArr->byteIdx += pByteArr->nextBitIdx / 8;
	pByteArr->nextBitIdx %= 8;
}

void pixioByteArrWriteStr(
	const PixalcFPtrs *pAlloc,
	PixioByteArr *pByteArr,
	const char *pStr
) {
	I32 byteLen = (I32)strlen(pStr) + 1;
	I32 bitLen = byteLen * 8;

	//+8 bits for for potential padding
	pixioReallocByteArrIfNeeded(pAlloc, pByteArr, bitLen + 8);
	if (pByteArr->nextBitIdx != 0) {
		//pad to beginning of next byte
		pByteArr->nextBitIdx = 0;
		++pByteArr->byteIdx;
	}
	memcpy(pByteArr->pArr + pByteArr->byteIdx, pStr, byteLen);
	pByteArr->byteIdx += byteLen;
}

void pixioByteArrRead(PixioByteArr *pByteArr, void *pData, int32_t bitLen) {
	U8 *pStart = pByteArr->pArr + pByteArr->byteIdx;
	I32 byteLen = getByteLen(bitLen + pByteArr->nextBitIdx);
	if (!pByteArr->nextBitIdx && byteLen * 8 == bitLen) {
		//size & start are aligned, so just memcpy
		memcpy(pData, pByteArr->pArr + pByteArr->byteIdx, byteLen);
		pByteArr->byteIdx += byteLen;
		return;
	}
	for (I32 i = 0; i < byteLen; ++i) {
		((U8 *)pData)[i] = pStart[i] >> pByteArr->nextBitIdx;
		if (i != byteLen - 1) {
			U8 nextByte = pStart[i + 1];
			nextByte <<= 8 - pByteArr->nextBitIdx;
			((U8 *)pData)[i] |= nextByte;
		}
	}
	U8 mask = UCHAR_MAX >> (8 - abs(bitLen - byteLen * 8)) % 8;
	((U8 *)pData)[byteLen - 1] &= mask;
	pByteArr->nextBitIdx = pByteArr->nextBitIdx + bitLen;
	pByteArr->byteIdx += pByteArr->nextBitIdx / 8;
	pByteArr->nextBitIdx %= 8;
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