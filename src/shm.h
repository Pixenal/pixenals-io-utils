/* 
SPDX-FileCopyrightText: 2025 Caleb Dawson
SPDX-License-Identifier: Apache-2.0
*/

#include <stdbool.h>
#include <stdio.h>

#include <pixenals_io_utils.h>

typedef uint32_t U32;
typedef uint64_t U64;
typedef uint8_t U8;

#define SHM_BUF_SIZE (sizeof(ShmHeader) + PIXIO_SHM_DATA_SIZE)

static inline
void *shmDataPtr(PixioShmCtx *pCtx) {
	return (U8 *)pCtx->access.pBuf + sizeof(ShmHeader);
}

void pixioShmPlatDestroy(PixioShmCtx *pCtx);
PixErr pixioShmPlatCreate(PixioShmCtx *pCtx);
PixErr pixioShmPlatOpen(PixioShmCtx *pCtx, const char *pName);
PixErr pixioShmPlatLock(PixioShmCtx *pCtx);
PixErr pixioShmPlatUnlock(PixioShmCtx *pCtx);
PixErr pixioShmPlatCpy(void *, const void *pSrc, I32 size);
void *pixioShmPlatMutexInit(ShmHeader *pHeader);
void pixioShmPlatMutexDestroy(ShmHeader *pHeader);
U64 pixioShmPlatTimeGetMilli();
