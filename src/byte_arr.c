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

static
void reallocByteArrIfNeeded(
	const PixalcFPtrs *pAlloc,
	PixioByteArr *pByteArr,
	I64 bitOffset
) {
	I64 bitCount = ((pByteArr->byteIdx) * 8) + pByteArr->nextBitIdx;
	PIX_ERR_ASSERT("", bitCount <= pByteArr->size * 8);
	bitCount += bitOffset;
	I64 byteCount = bitCount / 8 + (bitCount % 8 != 0);
	if (byteCount >= pByteArr->size) {
		I64 oldSize = pByteArr->size;
		pByteArr->size *= 2;
		pByteArr->pString = pAlloc->fpRealloc(pByteArr->pString, pByteArr->size);
		memset(pByteArr->pString + oldSize, 0, pByteArr->size - oldSize);
	}
}

void pixioByteArrWrite(
	const PixalcFPtrs *pAlloc,
	PixioByteArr *pByteArr,
	const void *pData,
	int32_t bitLen
) {
	reallocByteStringIfNeeded(pAlloc, pByteArr, bitLen);
	U8 valueBuf[PIXIO_BYTE_ARR_IO_BUF_LEN] = {0};
	I32 lengthInBytes = bitLen / 8;
	lengthInBytes += (bitLen - lengthInBytes * 8) > 0;
	for (I32 i = 1; i <= lengthInBytes; ++i) {
		valueBuf[i] = ((U8 *)pData)[i - 1];
	}
	for (I32 i = lengthInBytes - 1; i >= 1; --i) {
		valueBuf[i] <<= pByteArr->nextBitIdx;
		U8 nextByteCopy = valueBuf[i - 1];
		nextByteCopy >>= 8 - pByteArr->nextBitIdx;
		valueBuf[i] |= nextByteCopy;
	}
	I32 writeUpTo = lengthInBytes + (pByteArr->nextBitIdx > 0);
	for (I32 i = 0; i < writeUpTo; ++i) {
		pByteArr->pString[pByteArr->byteIdx + i] |= valueBuf[i + 1];
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
	I32 bitLen = ((I32)strlen((char *)pStr) + 1) * 8;
	I32 lengthInBytes = bitLen / 8;
	//+8 for potential padding
	reallocByteArrIfNeeded(pAlloc, pByteArr, bitLen + 8);
	if (pByteArr->nextBitIdx != 0) {
		//pad to beginning of next byte
		pByteArr->nextBitIdx = 0;
		pByteArr->byteIdx++;
	}
	for (I32 i = 0; i < lengthInBytes; ++i) {
		pByteArr->pString[pByteArr->byteIdx] = pStr[i];
		pByteArr->byteIdx++;
	}
}

void pixioByteArrRead(PixioByteArr *pByteArr, void *pData, int32_t bitLen) {
	I32 lengthInBytes = bitLen / 8;
	I32 bitDifference = bitLen - lengthInBytes * 8;
	lengthInBytes += bitDifference > 0;
	U8 buf[PIXIO_BYTE_ARR_IO_BUF_LEN] = {0};
	for (I32 i = 0; i < lengthInBytes; ++i) {
		buf[i] = pByteArr->pString[pByteArr->byteIdx + i];
	}
	for (I32 i = 0; i < lengthInBytes; ++i) {
		buf[i] >>= pByteArr->nextBitIdx;
		U8 nextByteCopy = buf[i + 1];
		nextByteCopy <<= 8 - pByteArr->nextBitIdx;
		buf[i] |= nextByteCopy;
	}
	for (I32 i = 0; i < lengthInBytes; ++i) {
		((U8 *)pData)[i] = buf[i];
	}
	U8 mask = UCHAR_MAX >> ((8 - bitDifference) % 8);
	((U8 *)pData)[lengthInBytes - 1] &= mask;
	pByteArr->nextBitIdx = pByteArr->nextBitIdx + bitLen;
	pByteArr->byteIdx += pByteArr->nextBitIdx / 8;
	pByteArr->nextBitIdx %= 8;
}

void pixioByteArrReadStr(PixioByteArr *pByteArr, char *pStr, int32_t maxLen) {
	pByteArr->byteIdx += pByteArr->nextBitIdx > 0;
	U8 *dataPtr = pByteArr->pString + pByteArr->byteIdx;
	I32 i = 0;
	for (; i < maxLen && dataPtr[i]; ++i) {
		pStr[i] = dataPtr[i];
	}
	pStr[i] = 0;
	pByteArr->byteIdx += i + 1;
	pByteArr->nextBitIdx = 0;
}

static
void encodeDataName(const PixalcFPtrs *pAlloc, PixioByteArr *pByteArr, char *pName) {
	//not using stucEncodeString, as there's not need for a null terminator.
	//Only using 2 characters
	
	//ensure string is aligned with byte (we need to do this manually,
	//as stucEncodeValue is being used instead of stucEncodeString)
	if (pByteArr->nextBitIdx != 0) {
		pByteArr->nextBitIdx = 0;
		pByteArr->byteIdx++;
	}
	stucEncodeValue(pAlloc, pByteArr, (U8 *)pName, 16);
}

static
PixErr isDataNameInvalid(PixioByteArr *pByteArr, char *pName) {
	//ensure string is aligned with byte (we need to do this manually,
	//as stucDecodeValue is being used instead of stucDecodeString, given there's
	//only 2 characters)
	pByteArr->byteIdx += pByteArr->nextBitIdx > 0;
	pByteArr->nextBitIdx = 0;
	char dataName[2] = {0};
	stucDecodeValue(pByteArr, (U8 *)&dataName, 16);
	if (dataName[0] != pName[0] || dataName[1] != pName[1]) {
		return PIX_ERR_ERROR;
	}
	else {
		return PIX_ERR_SUCCESS;
	}
}