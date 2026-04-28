/* 
SPDX-FileCopyrightText: 2025 Caleb Dawson
SPDX-License-Identifier: Apache-2.0
*/

#pragma once
#include <stdint.h>

#include "../../pixenals-alloc-utils/include/pixenals_alloc_utils.h"

#define PIXIO_PATH_MAX 32768

typedef struct {
	void *file;
} PixioFile;

typedef enum PixioFileOpenType {
	PIX_IO_FILE_OPEN_WRITE,
	PIX_IO_FILE_OPEN_READ
} PixioFileOpenType;

PixErr pixioFileOpen(
	void **ppFile,
	const char *filePath,
	PixioFileOpenType action,
	const PixalcFPtrs *pAlloc
);
PixErr pixioFileGetSize(void *pFile, int64_t *pSize);
PixErr pixioFileWrite(void *pFile, const unsigned char *data, int32_t dataSize);
PixErr pixioFileRead(void *pFile, unsigned char *data, int32_t bytesToRead);
PixErr pixioFileClose(void *pFile);
int32_t pixioPathMaxGet();

#define PIXIO_SHM_NAME_MAX_LEN 57
#define PIXIO_SHM_DATA_SIZE (1024 * 1024)

typedef enum PixioShmFlag {
	PIXIO_SHM_NONE,
	PIXIO_SHM_WRITTEN,
	PIXIO_SHM_READ,
	PIXIO_SHM_BLOCK_START,
	PIXIO_SHM_BLOCK_START_ACK,
	PIXIO_SHM_BLOCK_END,
	PIXIO_SHM_BLOCK_END_ACK,
	PIXIO_SHM_CLOSE,
	PIXIO_SHM_CLOSE_ACK,
	PIXIO_SHM_ERROR
} PixioShmFlag;

typedef struct ShmHeader{
	void *pMutex;
	I32 size;
	PixioShmFlag flag;
} ShmHeader;

union PixioShmAccess {
	void *pBuf;
	ShmHeader *pHeader;
};

typedef struct PixioShmCtx {
	void *pFile;
	union PixioShmAccess access;
	I32 blockSize;
} PixioShmCtx;

PixErr pixioShmOpenServer(PixioShmCtx *pCtx, const char *pName, char *pNameOut);
PixErr pixioShmOpenClient(PixioShmCtx *pCtx, const char *pName);
PixErr pixioShmCloseServer(PixioShmCtx *pCtx);
PixErr pixioShmCloseClient(PixioShmCtx *pCtx);
PixErr pixioShmSend(PixioShmCtx *pCtx, int32_t size, I32 desc, const void *pData);
PixErr pixioShmReceiveInit(PixioShmCtx *pCtx, int32_t *pSize, I32 *pDesc, bool *pClose);
PixErr pixioShmReceive(PixioShmCtx *pCtx, void *pDest);

#define PIXIO_BYTE_ARR_IO_BUF_LEN 34

typedef struct PixioByteArr {
	uint8_t *pArr;
	int64_t size;
	int64_t nextBitIdx;
	int64_t byteIdx;
} PixioByteArr;

void pixioByteArrWrite(
	const PixalcFPtrs *pAlloc,
	PixioByteArr *pByteArr,
	const void *pData,
	int32_t bitLen
);
void pixioByteArrWriteStr(
	const PixalcFPtrs *pAlloc,
	PixioByteArr *pByteArr,
	const char *pStr
);
void pixioByteArrRead(PixioByteArr *pByteArr, void *pData, int32_t bitLen);
void pixioByteArrReadStr(PixioByteArr *pByteArr, char *pStr, int32_t maxLen);
