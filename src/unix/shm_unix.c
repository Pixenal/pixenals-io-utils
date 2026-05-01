/* 
SPDX-FileCopyrightText: 2025 Caleb Dawson
SPDX-License-Identifier: Apache-2.0
*/

#include <sys/time.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <pthread.h>
#include <unistd.h>

#include <../src/shm.h>

void pixioShmPlatDestroy(PixioShmCtx *pCtx) {
	if (pCtx->access.pBuf) {
		munmap(pCtx->access.pBuf, SHM_BUF_SIZE);
	}
	if (pCtx->file.posix >= 0) {
		close(pCtx->file.posix);
	}
	if (pCtx->name[0]) {
		shm_unlink(pCtx->name);
	}
	*pCtx = (struct PixioShmCtx){0};
}

void pixioShmPlatMutexInit(ShmHeader *pHeader) {
	pthread_mutex_init((pthread_mutex_t *)pHeader->mutex.posix, NULL);
}

void pixioShmPlatMutexDestroy(ShmHeader *pHeader) {
	pthread_mutex_destroy((pthread_mutex_t *)pHeader->mutex.posix);
}

PixErr pixioShmPlatCreate(PixioShmCtx *pCtx) {
	PixErr err = PIX_ERR_SUCCESS;
	pCtx->file.posix = shm_open(pCtx->name, O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR);
	PIX_ERR_THROW_IFNOT_COND(err, pCtx->file.posix >= 0, "unable to create file mapping", 0);
	PIX_ERR_THROW_IFNOT_COND(err, !ftruncate(pCtx->file.posix, SHM_BUF_SIZE), "", 0);
	pCtx->access.pBuf = mmap(
		NULL,
		SHM_BUF_SIZE,
		PROT_READ | PROT_WRITE,
		MAP_SHARED,
		pCtx->file.posix,
		0
	);
	PIX_ERR_THROW_IFNOT_COND(
		err,
		pCtx->access.pBuf != MAP_FAILED,
		"failed to map shared memory",
		0
	);
	PIX_ERR_CATCH(0, err,
		pixioShmPlatDestroy(pCtx);
	);
	return err;
}

PixErr pixioShmPlatOpen(PixioShmCtx *pCtx, const char *pName) {
	PixErr err = PIX_ERR_SUCCESS;
	pCtx->file.posix = shm_open(pName, O_RDWR, S_IRUSR | S_IWUSR);
	PIX_ERR_THROW_IFNOT_COND(err, pCtx->file.posix, "unable to open shm file", 0);
	pCtx->access.pBuf = mmap(
		NULL,
		SHM_BUF_SIZE,
		PROT_READ | PROT_WRITE,
		MAP_SHARED,
		pCtx->file.posix,
		0
	);
	PIX_ERR_THROW_IFNOT_COND(err, pCtx->access.pBuf != MAP_FAILED, "failed to map shared memory", 0);
	PIX_ERR_CATCH(0, err,
		pixioShmPlatDestroy(pCtx);
	);
	return err;
}

PixErr pixioShmPlatLock(PixioShmCtx *pCtx) {
	PixErr err = PIX_ERR_SUCCESS;
	pthread_mutex_lock((pthread_mutex_t *)pCtx->access.pHeader->mutex.posix);
	return err;
}

PixErr pixioShmPlatUnlock(PixioShmCtx *pCtx) {
	PixErr err = PIX_ERR_SUCCESS;
	pthread_mutex_unlock((pthread_mutex_t *)pCtx->access.pHeader->mutex.posix);
	return err;
}

PixErr pixioShmPlatCpy(void *pDest, const void *pSrc, I32 size) {
	PixErr err = PIX_ERR_SUCCESS;
	memcpy(pDest, pSrc, size);
	return err;
}

U64 pixioShmPlatTimeGetMilli() {
	struct timeval time;
	gettimeofday(&time, NULL);
	return (U64)time.tv_sec * (U64)1000u + (U64)time.tv_usec / (U64)1000u;
}
