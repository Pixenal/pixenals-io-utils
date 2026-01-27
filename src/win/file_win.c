/* 
SPDX-FileCopyrightText: 2025 Caleb Dawson
SPDX-License-Identifier: Apache-2.0
*/

#include <stdbool.h>
#include <stdio.h>

#pragma comment(lib, "winmm.lib")//required for timeGetTime()
#include <windows.h>

#include <pixenals_io_utils.h>

#define ERR_MESSAGE_MAX_LEN 128

typedef uint32_t U32;
typedef uint64_t U64;

typedef struct PlatformContext {
	HANDLE *pHFile;
	PixalcFPtrs alloc;
} PlatformContext;

PixErr pixioFileOpen(
	void **ppFile,
	const char *filePath,
	PixioFileOpenType action,
	const PixalcFPtrs *pAlloc
) {
	PixErr err = PIX_ERR_SUCCESS;
	DWORD access;
	DWORD disposition;
	switch (action) {
		case PIX_IO_FILE_OPEN_WRITE:
			access = GENERIC_WRITE;
			disposition = CREATE_ALWAYS;
			break;
		case PIX_IO_FILE_OPEN_READ:
			access = GENERIC_READ;
			disposition = OPEN_EXISTING;
			break;
		default:
			PIX_ERR_RETURN(err, "Invalid action passed to function\n");
	}
	PlatformContext *pState = pAlloc->fpMalloc(sizeof(PlatformContext));
	pState->alloc = *pAlloc;
	pState->pHFile = CreateFile(
		filePath,
		access,
		false,
		NULL,
		disposition,
		FILE_ATTRIBUTE_NORMAL, NULL
	);
	if (pState->pHFile == INVALID_HANDLE_VALUE) {
		char message[ERR_MESSAGE_MAX_LEN] = {0};
		snprintf(message, ERR_MESSAGE_MAX_LEN, "Win Error: %d\n", GetLastError());
		PIX_ERR_RETURN(err, message);
	}
	*ppFile = pState;
	return err;
}

PixErr pixioFileGetSize(void *pFile, int64_t *pSize) {
	PixErr err = PIX_ERR_SUCCESS;
	PlatformContext *pState = pFile;
	LARGE_INTEGER size = {0};
	if (!GetFileSizeEx(pState->pHFile, &size)) {
		char message[ERR_MESSAGE_MAX_LEN] = {0};
		snprintf(
			message,
			ERR_MESSAGE_MAX_LEN,
			"Win error: %d\n",
			GetLastError()
		);
		PIX_ERR_RETURN(err, message);
	}
	*pSize = size.QuadPart;
	return err;
}

PixErr pixioFileWrite(
	void *pFile,
	const unsigned char *data,
	int32_t dataSize
) {
	PixErr err = PIX_ERR_SUCCESS;
	PlatformContext *pState = pFile;
	DWORD bytesWritten;
	if (!WriteFile(pState->pHFile, data, dataSize, &bytesWritten, NULL)) {
		char message[ERR_MESSAGE_MAX_LEN] = {0};
		snprintf(
			message,
			ERR_MESSAGE_MAX_LEN,
			"Win error: %d\n",
			GetLastError()
		);
		PIX_ERR_RETURN(err, message);
	}
	PIX_ERR_RETURN_IFNOT_COND(
		err,
		(int32_t)bytesWritten == dataSize,
		"Number of bytes written does not match data len"
	);
	return err;
}

PixErr pixioFileRead(
	void *pFile,
	unsigned char *data,
	int32_t bytesToRead
) {
	PixErr err = PIX_ERR_SUCCESS;
	PlatformContext *pState = pFile;
	DWORD bytesRead;
	if (!ReadFile(pState->pHFile, data, bytesToRead, &bytesRead, NULL)) {
		char message[ERR_MESSAGE_MAX_LEN] = {0};
		snprintf(
			message,
			ERR_MESSAGE_MAX_LEN,
			"Win error: %d\n",
			GetLastError()
		);
		PIX_ERR_RETURN(err, message);
	}
	PIX_ERR_RETURN_IFNOT_COND(
		err,
		(int32_t)bytesRead == bytesToRead,
		"Number of bytes read does not match specififed amount\n"
	);
	return err;
}

PixErr pixioFileClose(void *pFile) {
	PixErr err = PIX_ERR_SUCCESS;
	PlatformContext *pState = pFile;
	bool success = CloseHandle(pState->pHFile);
	pState->alloc.fpFree(pState);
	if (!success) {
		char message[ERR_MESSAGE_MAX_LEN] = {0};
		snprintf(message, ERR_MESSAGE_MAX_LEN, "Win error: %d\n", GetLastError());
		PIX_ERR_RETURN(err, message);
	}
	return err;
}

int32_t pixioPathMaxGet() {
	return PIXIO_PATH_MAX;
}

static
U32 fnvHash(const U8 *value, I32 valueSize, U32 size) {
	PIX_ERR_ASSERT("", value && valueSize > 0 && size > 0);
	U32 hash = 2166136261;
	for (I32 i = 0; i < valueSize; ++i) {
		hash ^= value[i];
		hash *= 16777619;
	}
	hash %= size;
	PIX_ERR_ASSERT("", hash >= 0);
	return hash;
}

#define SHM_BUF_SIZE (sizeof(ShmHeader) + PIXIO_SHM_DATA_SIZE)

static
void *shmDataPtr(PixioShmCtx *pCtx) {
	return (U8 *)pCtx->access.pBuf + sizeof(ShmHeader);
}

PixErr pixioShmOpenServer(PixioShmCtx *pCtx, const char *pName, char *pNameOut) {
	PixErr err = PIX_ERR_SUCCESS;
	PIX_ERR_RETURN_IFNOT_COND(err, strnlen(pName, PIXIO_SHM_NAME_MAX_LEN) <= 8, "name is too long");
	char nameBuf[PIXIO_SHM_NAME_MAX_LEN] = "";
	U32 rand = fnvHash((U8 *)&(void *){&err}, sizeof(void *), INT32_MAX);
	snprintf(nameBuf, sizeof(nameBuf), "PIXIO_%s_", pName);

	pCtx->pFile = CreateFileMapping(
		INVALID_HANDLE_VALUE,
		NULL,
		PAGE_READWRITE,
		0,
		SHM_BUF_SIZE,
		nameBuf
	);
	PIX_ERR_THROW_IFNOT_COND(err, pCtx->pFile, "unable to create file mapping", 0);
	pCtx->access.pBuf = MapViewOfFile(pCtx->pFile, FILE_MAP_ALL_ACCESS, 0, 0, SHM_BUF_SIZE);
	PIX_ERR_THROW_IFNOT_COND(err, pCtx->access.pBuf, "unable to access shared memory", 0);
	{
		ShmHeader header = {
			.pMutex = CreateMutex(NULL, 0, NULL)
		};
		CopyMemory(pCtx->access.pBuf, &header, sizeof(header));
	}
	memcpy(pNameOut, nameBuf, PIXIO_SHM_NAME_MAX_LEN);
	PIX_ERR_CATCH(0, err,
		if (pCtx->access.pBuf) {
			UnmapViewOfFile(pCtx->access.pBuf);
		}
		if (pCtx->pFile) {
			CloseHandle(pCtx->pFile);
		}
		*pCtx = (struct PixioShmCtx){0};
	);
	return err;
}

PixErr pixioShmOpenClient(PixioShmCtx *pCtx, const char *pName) {
	PixErr err = PIX_ERR_SUCCESS;
	pCtx->pFile = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, pName);
	PIX_ERR_THROW_IFNOT_COND(err, pCtx->pFile, "unable to open file mapping", 0);
	pCtx->access.pBuf = MapViewOfFile(pCtx->pFile, FILE_MAP_ALL_ACCESS, 0, 0, SHM_BUF_SIZE);
	PIX_ERR_THROW_IFNOT_COND(err, pCtx->access.pBuf, "unable to access shared memory", 0);
	PIX_ERR_CATCH(0, err,
		if (pCtx->access.pBuf) {
			UnmapViewOfFile(pCtx->access.pBuf);
		}
		if (pCtx->pFile) {
			CloseHandle(pCtx->pFile);
		}
		*pCtx = (struct PixioShmCtx){0};
	);
	return err;
}

static
PixErr pixioShmLock(PixioShmCtx *pCtx) {
	PixErr err = PIX_ERR_SUCCESS;
	WaitForSingleObject(pCtx->access.pHeader->pMutex, INFINITE);
	return err;
}

static
PixErr pixioShmUnlock(PixioShmCtx *pCtx) {
	PixErr err = PIX_ERR_SUCCESS;
	ReleaseMutex(pCtx->access.pHeader->pMutex);
	return err;
}

static
PixErr pixioShmWrite(PixioShmCtx *pCtx, I32 size, const void *pSrc) {
	PixErr err = PIX_ERR_SUCCESS;
	PIX_ERR_RETURN_IFNOT_COND(err, size <= PIXIO_SHM_DATA_SIZE, "data size exceeds max");
	pixioShmLock(pCtx);
	PIX_ERR_THROW_IFNOT_COND(
		err,
		pCtx->access.pHeader->flag != PIXIO_SHM_WRITTEN,
		"write was called, but flag is already set to written",
		0
	);
	CopyMemory(shmDataPtr(pCtx), pSrc, size);
	pCtx->access.pHeader->size = size;
	pCtx->access.pHeader->flag = PIXIO_SHM_WRITTEN;
	PIX_ERR_CATCH(0, err, ;);
	pixioShmUnlock(pCtx);
	return err;
}

static
PixErr shmDataSizeGet(PixioShmCtx *pCtx, I32 *pSize) {
	PixErr err = PIX_ERR_SUCCESS;
	pixioShmLock(pCtx);
	PIX_ERR_THROW_IFNOT_COND(err, pCtx->access.pHeader->flag == PIXIO_SHM_WRITTEN, "", 0);
	*pSize = pCtx->access.pHeader->size;
	PIX_ERR_CATCH(0, err, ;);
	pixioShmUnlock(pCtx);
	return err;
}

static
PixErr shmFlagGet(PixioShmCtx *pCtx, PixioShmFlag *pFlag) {
	PixErr err = PIX_ERR_SUCCESS;
	pixioShmLock(pCtx);
	*pFlag = pCtx->access.pHeader->flag;
	pixioShmUnlock(pCtx);
	return err;
}

static
PixErr shmRead(PixioShmCtx *pCtx, void *pDest, I32 *pSize) {
	PixErr err = PIX_ERR_SUCCESS;
	pixioShmLock(pCtx);
	PIX_ERR_THROW_IFNOT_COND(err, pCtx->access.pHeader->flag == PIXIO_SHM_WRITTEN, "", 0);
	CopyMemory(pDest, shmDataPtr(pCtx), pCtx->access.pHeader->size);
	pCtx->access.pHeader->flag = PIXIO_SHM_READ;
	if (pSize) {
		*pSize = pCtx->access.pHeader->size;
	}
	PIX_ERR_CATCH(0, err, ;);
	pixioShmUnlock(pCtx);
	return err;
}

typedef enum WaitCond {
	WAIT_TILL,
	WAIT_TILL_NOT
} WaitCond;

static
PixErr shmWait(
	PixioShmCtx *pCtx,
	PixioShmFlag target,
	PixioShmFlag *pOut,
	I32 timeout,
	WaitCond cond
) {
	PixErr err = PIX_ERR_SUCCESS;
	U64 timeout64 = (U64)timeout * (U64)1000u;
	U64 timeStart = (U64)timeGetTime();
	do {
		PixioShmFlag flag = PIXIO_SHM_NONE;
		err = shmFlagGet(pCtx, &flag);
		PIX_ERR_RETURN_IFNOT(err, "");
		if (cond == WAIT_TILL ? flag == target : flag != target
		) {
			if (pOut) {
				*pOut = flag;
			}
			break;
		}
		U64 timeNow = (U64)timeGetTime();
		U64 elapsed = timeNow - timeStart;
		elapsed = elapsed < 0 ? (U64)UINT_MAX + timeNow - timeStart : elapsed;
		PIX_ERR_RETURN_IFNOT_COND(err, elapsed <= timeout64, "timed out");
		Sleep(1);
	} while(true);
	return err;
}

//this func should NOT be used alongside pixioShmWrite, only one or the other
static
PixErr shmWriteFlag(PixioShmCtx *pCtx, PixioShmFlag flag) {
	PixErr err = PIX_ERR_SUCCESS;
	pixioShmLock(pCtx);
	pCtx->access.pHeader->flag = flag;
	pCtx->access.pHeader->size = 0;
	pixioShmUnlock(pCtx);
	return err;
}

//this func should NOT be used alongside shmRead, only one or the other
static
PixErr shmReadFlag(PixioShmCtx *pCtx, PixioShmFlag *pFlag) {
	PixErr err = PIX_ERR_SUCCESS;
	pixioShmLock(pCtx);
	*pFlag = pCtx->access.pHeader->flag;
	pixioShmUnlock(pCtx);
	return err;
}

static
PixErr shmHandshakeServer(PixioShmCtx *pCtx, PixioShmFlag flag) {
	PixErr err = PIX_ERR_SUCCESS;
	PIX_ERR_ASSERT(
		"",
		flag == PIXIO_SHM_BLOCK_START || flag == PIXIO_SHM_BLOCK_END 
	);
	shmWriteFlag(pCtx, flag);
	err = shmWait(pCtx, flag + 1, NULL, 8, WAIT_TILL);
	PIX_ERR_RETURN_IFNOT(
		err,
		"client failed to acknowledge handshake"
	);
	return err;
}

static
PixErr shmHandshakeClient(PixioShmCtx *pCtx, PixioShmFlag flag) {
	PixErr err = PIX_ERR_SUCCESS;
	PIX_ERR_ASSERT(
		"",
		flag == PIXIO_SHM_BLOCK_START || flag == PIXIO_SHM_BLOCK_END
	);
	err = shmWait(pCtx, flag, NULL, 8, WAIT_TILL);
	PIX_ERR_RETURN_IFNOT(
		err,
		"failed to receive handshake from server"
	);
	err = shmWriteFlag(pCtx, flag + 1);
	PIX_ERR_RETURN_IFNOT(err, "");
	return err;
}

PixErr pixioShmSend(PixioShmCtx *pCtx, I32 size, I32 desc, const void *pData) {
	PixErr err = PIX_ERR_SUCCESS;
	PIX_ERR_ASSERT("invalid size", size > 0);
	err = shmHandshakeServer(pCtx, PIXIO_SHM_BLOCK_START);
	PIX_ERR_RETURN_IFNOT(err, "block start handshake failed");
	err = pixioShmWrite(pCtx, sizeof(I32), &size);
	PIX_ERR_RETURN_IFNOT(err, "");
	PixioShmFlag flag = PIXIO_SHM_NONE;
	err = shmWait(pCtx, PIXIO_SHM_WRITTEN, &flag, 8, WAIT_TILL_NOT);
	PIX_ERR_RETURN_IFNOT_COND(err, flag == PIXIO_SHM_READ,"");
	err = pixioShmWrite(pCtx, sizeof(I32), &desc);
	PIX_ERR_RETURN_IFNOT(err, "");
	I32 written = 0;
	do {
		err = shmWait(pCtx, PIXIO_SHM_WRITTEN, &flag, 8, WAIT_TILL_NOT);
		PIX_ERR_RETURN_IFNOT_COND(err, flag == PIXIO_SHM_READ,"");
		I32 packetSize = size - written;
		packetSize = packetSize > PIXIO_SHM_DATA_SIZE ? PIXIO_SHM_DATA_SIZE : packetSize;
		if (packetSize) {
			err = pixioShmWrite(pCtx, packetSize, (U8 *)pData + written);
			PIX_ERR_RETURN_IFNOT(err, "");
			written += packetSize;
		}
	} while(written < size);
	err = shmWait(pCtx, PIXIO_SHM_WRITTEN, &flag, 8, WAIT_TILL_NOT);
	PIX_ERR_RETURN_IFNOT_COND(err, flag == PIXIO_SHM_READ, "");
	shmHandshakeServer(pCtx, PIXIO_SHM_BLOCK_END);
	PIX_ERR_RETURN_IFNOT(err, "block end handshake failed");
	return err;
}

PixErr pixioShmReceiveInit(PixioShmCtx *pCtx, I32 *pSize, I32 *pDesc, bool *pClose) {
	PixErr err = PIX_ERR_SUCCESS;
	PIX_ERR_ASSERT("", pCtx && pSize && pDesc);
	err = shmHandshakeClient(pCtx, PIXIO_SHM_BLOCK_START);
	PIX_ERR_RETURN_IFNOT(err, "block start handshake failed");
	PixioShmFlag flag = PIXIO_SHM_NONE;
	err = shmWait(pCtx, PIXIO_SHM_BLOCK_START_ACK, &flag, 8, WAIT_TILL_NOT);
	if (flag == PIXIO_SHM_CLOSE) {
		err = shmWriteFlag(pCtx, PIXIO_SHM_CLOSE_ACK);
		PIX_ERR_RETURN_IFNOT(err, "");
		PIX_ERR_RETURN_IFNOT_COND(err, pClose, "unexpected close flag");
		*pClose = true;
		*pDesc = 0;
		*pSize = 0;
		return err;
	}
	PIX_ERR_RETURN_IFNOT_COND(err, flag == PIXIO_SHM_WRITTEN, "");
	I32 blockSize = 0;
	I32 read = 0;
	err = shmRead(pCtx, (U8 *)&blockSize, NULL);
	PIX_ERR_RETURN_IFNOT_COND(err, blockSize > 0, "bad blocksize, reading invalid data?");
	err = shmWait(pCtx, PIXIO_SHM_READ, &flag, 8, WAIT_TILL_NOT);
	PIX_ERR_RETURN_IFNOT_COND(err, flag == PIXIO_SHM_WRITTEN,"");
	I32 desc = 0;
	err = shmRead(pCtx, (U8 *)&desc, NULL);
	PIX_ERR_RETURN_IFNOT_COND(err, desc >= 0, "invalid desc, data may be invalid?");
	*pSize = pCtx->blockSize = blockSize;
	*pDesc = desc;
	if (pClose) {
		*pClose = false;
	}
	return err;
}

PixErr pixioShmReceive(PixioShmCtx *pCtx, void *pDest) {
	PixErr err = PIX_ERR_SUCCESS;
	I32 read = 0;
	do {
		PixioShmFlag flag = PIXIO_SHM_NONE;
		err = shmWait(pCtx, PIXIO_SHM_READ, &flag, 8, WAIT_TILL_NOT);
		PIX_ERR_RETURN_IFNOT_COND(err, flag == PIXIO_SHM_WRITTEN,"");
		I32 size = 0;
		err = shmRead(pCtx, (U8 *)pDest + read, &size);
		PIX_ERR_RETURN_IFNOT(err, "");
		read += size;
	} while(read < pCtx->blockSize);
	pCtx->blockSize = 0;
	err = shmHandshakeClient(pCtx, PIXIO_SHM_BLOCK_END);
	PIX_ERR_RETURN_IFNOT(err, "block end handshake failed");
	return err;
}

static
PixErr shmClose(PixioShmCtx *pCtx, bool server) {
	PixErr err = PIX_ERR_SUCCESS;
	if (server) {
		err = shmHandshakeServer(pCtx, PIXIO_SHM_BLOCK_START);
		PIX_ERR_RETURN_IFNOT(err, "");
		err = shmWriteFlag(pCtx, PIXIO_SHM_CLOSE);
		PIX_ERR_RETURN_IFNOT(err, "");
		err = shmWait(pCtx, PIXIO_SHM_CLOSE_ACK, NULL, 8, WAIT_TILL);
		PIX_ERR_RETURN_IFNOT(err, "");
	}
	if (pCtx->access.pBuf) {
		UnmapViewOfFile(pCtx->access.pBuf);
	}
	if (pCtx->pFile) {
		CloseHandle(pCtx->pFile);
	}
	*pCtx = (struct PixioShmCtx){0};
	PIX_ERR_RETURN_IFNOT(err, "");
	return err;
}

PixErr pixioShmCloseServer(PixioShmCtx *pCtx) {
	return shmClose(pCtx, true);
}

PixErr pixioShmCloseClient(PixioShmCtx *pCtx) {
	return shmClose(pCtx, false);
}
