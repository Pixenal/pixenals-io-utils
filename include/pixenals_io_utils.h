/* 
SPDX-FileCopyrightText: 2025 Caleb Dawson
SPDX-License-Identifier: Apache-2.0
*/

#pragma once
#include <stdint.h>

#include "../../pixenals-alloc-utils/include/pixenals_alloc_utils.h"

#define PIXIO_PATH_MAX 32768

typedef struct PixioFile {
	void *pFile;
} PixioFile;

typedef enum PixioFileOpenType {
	PIX_IO_FILE_OPEN_WRITE,
	PIX_IO_FILE_OPEN_READ
} PixioFileOpenType;

typedef struct PixioFPtrs {
	PixErr (*fpOpen)(PixioFile *, const char *, PixioFileOpenType);
	PixErr (*fpWrite)(PixioFile *, const void *, int64_t);
	PixErr (*fpRead)(PixioFile *, void *, int64_t);
	PixErr (*fpClose)(PixioFile *);
} PixioFPtrs;

PixErr pixioFileOpen(
	PixioFile *pFile,
	const char *filePath,
	PixioFileOpenType action
);
PixErr pixioFileGetSize(PixioFile *pFile, int64_t *pSize);
PixErr pixioFileWrite(PixioFile *pFile, const void *pData, int64_t dataSize);
PixErr pixioFileRead(PixioFile *pFile, void *pData, int64_t bytesToRead);
PixErr pixioFileClose(PixioFile *pFile);
int32_t pixioPathMaxGet();

#define PIXIO_SHM_NAME_USER_MAX_LEN 16
// v  prefix + user provided name + timestamp(hex) + spacing  v
#define PIXIO_SHM_NAME_MAX_LEN (5 + 16 + 16 + 2)
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

union PixioShmFile {
	int64_t posix;
	void *pWin;
};

typedef struct PixioShmCtx {
	union PixioShmFile file;
	union PixioShmAccess access;
	char name[PIXIO_SHM_NAME_MAX_LEN + 1];
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
	int64_t byteIdx;
	int32_t nextBitIdx;
} PixioByteArr;

void pixioReallocByteArrIfNeeded(
	const PixalcFPtrs *pAlloc,
	PixioByteArr *pByteArr,
	int64_t bitOffset
);
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
