/* 
SPDX-FileCopyrightText: 2025 Caleb Dawson
SPDX-License-Identifier: Apache-2.0
*/

#pragma comment(lib, "winmm.lib")//required for timeGetTime()
#include <windows.h>

#include <../src/shm.h>

void pixioShmPlatDestroy(PixioShmCtx *pCtx) {
	if (pCtx->access.pBuf) {
		UnmapViewOfFile(pCtx->access.pBuf);
	}
	if (pCtx->file.pWin) {
		CloseHandle(pCtx->file.pWin);
	}
	*pCtx = (struct PixioShmCtx){0};
}

void pixioShmPlatMutexInit(PixioShmCtx *pCtx) {
	pCtx->access.pHeader->pMutex = CreateMutex(NULL, 0, NULL);
}

void pixioShmPlatMutexDestroy(ShmHeader *pHeader) {
	CloseHandle(pHeader->pMutex);
}

PixErr pixioShmPlatCreate(PixioShmCtx *pCtx) {
	PixErr err = PIX_ERR_SUCCESS;
	pCtx->file.pWin = CreateFileMapping(
		INVALID_HANDLE_VALUE,
		NULL,
		PAGE_READWRITE,
		0,
		SHM_BUF_SIZE,
		pCtx->name
	);
	PIX_ERR_THROW_IFNOT_COND(err, pCtx->file.pWin, "unable to create file mapping", 0);
	pCtx->access.pBuf = MapViewOfFile(pCtx->file.pWin, FILE_MAP_ALL_ACCESS, 0, 0, SHM_BUF_SIZE);
	PIX_ERR_THROW_IFNOT_COND(err, pCtx->access.pBuf, "unable to access shared memory", 0);
	PIX_ERR_CATCH(0, err,
		pixioShmPlatDestroy(pCtx);
	);
	return err;
}

PixErr pixioShmPlatOpen(PixioShmCtx *pCtx, const char *pName) {
	PixErr err = PIX_ERR_SUCCESS;
	pCtx->file.pWin = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, pName);
	PIX_ERR_THROW_IFNOT_COND(err, pCtx->file.pWin, "unable to open file mapping", 0);
	pCtx->access.pBuf = MapViewOfFile(pCtx->file.pWin, FILE_MAP_ALL_ACCESS, 0, 0, SHM_BUF_SIZE);
	PIX_ERR_THROW_IFNOT_COND(err, pCtx->access.pBuf, "unable to access shared memory", 0);
	PIX_ERR_CATCH(0, err,
		pixioShmPlatDestroy(pCtx);
	);
	return err;
}

PixErr pixioShmPlatLock(PixioShmCtx *pCtx) {
	PixErr err = PIX_ERR_SUCCESS;
	WaitForSingleObject(pCtx->access.pHeader->pMutex, INFINITE);
	return err;
}

PixErr pixioShmPlatUnlock(PixioShmCtx *pCtx) {
	PixErr err = PIX_ERR_SUCCESS;
	ReleaseMutex(pCtx->access.pHeader->pMutex);
	return err;
}

PixErr pixioShmPlatCpy(void *pDest, const void *pSrc, I32 size) {
	PixErr err = PIX_ERR_SUCCESS;
	CopyMemory(pDest, pSrc, size);
	return err;
}

U64 pixioShmPlatTimeGetMilli() {
	return (U64)timeGetTime();
}

void *pixioShmPlatDataPtr(PixioShmCtx *pCtx) {
	return (U8 *)pCtx->access.pBuf + sizeof(ShmHeader);
}
