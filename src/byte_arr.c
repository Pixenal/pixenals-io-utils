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
I64 getByteLen(I64 bitSize) {
	I64 byteSize = bitSize / 8;
	byteSize += bitSize != byteSize * 8;
	return byteSize;
}

void pixioByteArrAlign(PixioByteArr *pByteArr) {
	if (pByteArr->nextBitIdx) {
		++pByteArr->byteIdx;
		pByteArr->nextBitIdx = 0;
	}
}

void pixioByteArrInc(PixioByteArr *pByteArr, I64 bitSize) {
	pByteArr->nextBitIdx += bitSize;
	pByteArr->byteIdx += pByteArr->nextBitIdx / 8;
	pByteArr->nextBitIdx %= 8;
}

void pixioByteArrWrite(
	const PixalcFPtrs *pAlloc,
	PixioByteArr *pByteArr,
	const void *pData,
	I64 bitSize
) {
	PIX_ERR_ASSERT("", bitSize > 0);
	pixioByteArrResize(pAlloc, pByteArr, bitSize);
	U8 *pStart = pByteArr->pArr + pByteArr->byteIdx;

	I64 byteSizeSrc = getByteLen(bitSize);
	if (!pByteArr->nextBitIdx && byteSizeSrc * 8 == bitSize) {
		//start & size are aligned, so just memcpy
		memcpy(pStart, pData, byteSizeSrc);
		pByteArr->byteIdx += byteSizeSrc;
		return;
	}
	I64 byteSizeDest = getByteLen(bitSize + pByteArr->nextBitIdx);
	pStart[0] |= ((U8 *)pData)[0] << pByteArr->nextBitIdx;
	for (I64 i = 1; i < byteSizeDest; ++i) {
		pStart[i] = i == byteSizeSrc ? 0x0 : ((U8 *)pData)[i] << pByteArr->nextBitIdx;
		U8 nextByte = ((U8 *)pData)[i - 1];
		nextByte >>= 8 - pByteArr->nextBitIdx;
		pStart[i] |= nextByte;
	}
	pixioByteArrInc(pByteArr, bitSize);
}

void pixioByteArrWriteStr(
	const PixalcFPtrs *pAlloc,
	PixioByteArr *pByteArr,
	const char *pStr
) {
	I64 byteSize = (I64)strlen(pStr) + 1;
	I64 bitSize = byteSize * 8;

	//+8 bits for for potential padding
	pixioByteArrResize(pAlloc, pByteArr, bitSize + 8);
	if (pByteArr->nextBitIdx != 0) {
		//pad to beginning of next byte
		pByteArr->nextBitIdx = 0;
		++pByteArr->byteIdx;
	}
	memcpy(pByteArr->pArr + pByteArr->byteIdx, pStr, byteSize);
	pByteArr->byteIdx += byteSize;
}

void pixioByteArrRead(PixioByteArr *pByteArr, void *pData, I64 bitSize) {
	PIX_ERR_ASSERT("", bitSize > 0);
	U8 *pStart = pByteArr->pArr + pByteArr->byteIdx;
	I64 byteSizeDest = getByteLen(bitSize);
	if (!pByteArr->nextBitIdx && byteSizeDest * 8 == bitSize) {
		//size & start are aligned, so just memcpy
		memcpy(pData, pByteArr->pArr + pByteArr->byteIdx, byteSizeDest);
		pByteArr->byteIdx += byteSizeDest;
		return;
	}
	I64 byteSizeSrc = getByteLen(bitSize + pByteArr->nextBitIdx);
	for (I64 i = 0; i < byteSizeDest; ++i) {
		((U8 *)pData)[i] = pStart[i] >> pByteArr->nextBitIdx;
		if (i != byteSizeSrc - 1) {
			U8 nextByte = pStart[i + 1];
			nextByte <<= 8 - pByteArr->nextBitIdx;
			((U8 *)pData)[i] |= nextByte;
		}
	}
	if (bitSize % 8) {
		U8 mask = 0xff >> 8 - bitSize % 8;
		((U8 *)pData)[byteSizeDest - 1] &= mask;
	}
	pixioByteArrInc(pByteArr, bitSize);
}

void pixioByteArrReadStr(PixioByteArr *pByteArr, char *pStr, I64 maxLen) {
	pByteArr->byteIdx += pByteArr->nextBitIdx > 0;
	U8 *pSrc = pByteArr->pArr + pByteArr->byteIdx;
	I64 i = 0;
	for (; i < maxLen && pSrc[i]; ++i) {
		pStr[i] = pSrc[i];
	}
	pStr[i] = 0;
	pByteArr->byteIdx += i + 1;
	pByteArr->nextBitIdx = 0;
}