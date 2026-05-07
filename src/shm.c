/* 
SPDX-FileCopyrightText: 2025 Caleb Dawson
SPDX-License-Identifier: Apache-2.0
*/

#include "../../pixenals-error-utils/include/pixenals_error_utils.h"

#include <../src/shm.h>

PixErr pixioShmOpenServer(PixioShmCtx *pCtx, const char *pName, char *pNameOut) {
	PixErr err = PIX_ERR_SUCCESS;
	*pCtx = (PixioShmCtx){0};
	PIX_ERR_RETURN_IFNOT_COND(
		err,
		strnlen(pName, PIXIO_SHM_NAME_USER_MAX_LEN) <= PIXIO_SHM_NAME_USER_MAX_LEN,
		"name is too long"
	);
	U64 timestamp = pixioShmPlatTimeGetMilli();
	snprintf(
		pCtx->name,
		sizeof(pCtx->name),
#ifdef WIN32
		"PIXIO_%s_%llX",
#else
		"PIXIO_%s_%lX",
#endif
		pName, timestamp
	);

	err = pixioShmPlatCreate(pCtx);
	PIX_ERR_RETURN_IFNOT(err, "");
	pixioShmPlatCpy(pCtx->access.pBuf, &(ShmHeader){0}, sizeof(ShmHeader));
	pixioShmPlatMutexInit(pCtx);
	memcpy(pNameOut, pCtx->name, sizeof(pCtx->name));
	return err;
}

PixErr pixioShmOpenClient(PixioShmCtx *pCtx, const char *pName) {
	PixErr err = PIX_ERR_SUCCESS;
	*pCtx = (PixioShmCtx){0};
	pixioShmPlatOpen(pCtx, pName);
	PIX_ERR_RETURN_IFNOT(err, "");
	return err;
}

static
PixErr pixioShmWrite(PixioShmCtx *pCtx, I32 size, const void *pSrc) {
	PixErr err = PIX_ERR_SUCCESS;
	PIX_ERR_RETURN_IFNOT_COND(err, size <= PIXIO_SHM_DATA_SIZE, "data size exceeds max");
	pixioShmPlatLock(pCtx);
	PIX_ERR_THROW_IFNOT_COND(
		err,
		pCtx->access.pHeader->flag != PIXIO_SHM_WRITTEN,
		"write was called, but flag is already set to written",
		0
	);
	err = pixioShmPlatCpy(pixioShmPlatDataPtr(pCtx), pSrc, size);
	PIX_ERR_RETURN_IFNOT(err, "");
	pCtx->access.pHeader->size = size;
	pCtx->access.pHeader->flag = PIXIO_SHM_WRITTEN;
	PIX_ERR_CATCH(0, err, ;);
	pixioShmPlatUnlock(pCtx);
	return err;
}

/*
static
PixErr shmDataSizeGet(PixioShmCtx *pCtx, I32 *pSize) {
	PixErr err = PIX_ERR_SUCCESS;
	pixioShmPlatLock(pCtx);
	PIX_ERR_THROW_IFNOT_COND(err, pCtx->access.pHeader->flag == PIXIO_SHM_WRITTEN, "", 0);
	*pSize = pCtx->access.pHeader->size;
	PIX_ERR_CATCH(0, err, ;);
	pixioShmPlatUnlock(pCtx);
	return err;
}
*/

static
PixErr shmFlagGet(PixioShmCtx *pCtx, PixioShmFlag *pFlag) {
	PixErr err = PIX_ERR_SUCCESS;
	pixioShmPlatLock(pCtx);
	*pFlag = pCtx->access.pHeader->flag;
	pixioShmPlatUnlock(pCtx);
	return err;
}

static
PixErr shmRead(PixioShmCtx *pCtx, void *pDest, I32 *pSize) {
	PixErr err = PIX_ERR_SUCCESS;
	pixioShmPlatLock(pCtx);
	PIX_ERR_THROW_IFNOT_COND(err, pCtx->access.pHeader->flag == PIXIO_SHM_WRITTEN, "", 0);
	err = pixioShmPlatCpy(pDest, pixioShmPlatDataPtr(pCtx), pCtx->access.pHeader->size);
	PIX_ERR_THROW_IFNOT(err, "", 0);
	pCtx->access.pHeader->flag = PIXIO_SHM_READ;
	if (pSize) {
		*pSize = pCtx->access.pHeader->size;
	}
	PIX_ERR_CATCH(0, err, ;);
	pixioShmPlatUnlock(pCtx);
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
	U64 timeStart = pixioShmPlatTimeGetMilli();
	do {
		PixioShmFlag flag = PIXIO_SHM_NONE;
		err = shmFlagGet(pCtx, &flag);
		PIX_ERR_RETURN_IFNOT(err, "");
		if (cond == WAIT_TILL ? flag == target : flag != target) {
			if (pOut) {
				*pOut = flag;
			}
			break;
		}
		U64 timeNow = pixioShmPlatTimeGetMilli();
		U64 elapsed = timeNow - timeStart;
		PIX_ERR_RETURN_IFNOT_COND(err, elapsed <= timeout64, "timed out");
		//Sleep(1);
	} while(true);
	return err;
}

#define SHM_TIMEOUT 8

//this func should NOT be used alongside pixioShmWrite, only one or the other
static
PixErr shmWriteFlag(PixioShmCtx *pCtx, PixioShmFlag flag) {
	PixErr err = PIX_ERR_SUCCESS;
	pixioShmPlatLock(pCtx);
	pCtx->access.pHeader->flag = flag;
	pCtx->access.pHeader->size = 0;
	pixioShmPlatUnlock(pCtx);
	return err;
}

//this func should NOT be used alongside shmRead, only one or the other
/*
static
PixErr shmReadFlag(PixioShmCtx *pCtx, PixioShmFlag *pFlag) {
	PixErr err = PIX_ERR_SUCCESS;
	pixioShmPlatLock(pCtx);
	*pFlag = pCtx->access.pHeader->flag;
	pixioShmPlatUnlock(pCtx);
	return err;
}
*/

static
PixErr shmHandshakeServer(PixioShmCtx *pCtx, PixioShmFlag flag) {
	PixErr err = PIX_ERR_SUCCESS;
	PIX_ERR_ASSERT(
		"",
		flag == PIXIO_SHM_BLOCK_START || flag == PIXIO_SHM_BLOCK_END 
	);
	shmWriteFlag(pCtx, flag);
	err = shmWait(pCtx, flag + 1, NULL, SHM_TIMEOUT, WAIT_TILL);
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
	err = shmWait(pCtx, flag, NULL, SHM_TIMEOUT, WAIT_TILL);
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
	err = shmWait(pCtx, PIXIO_SHM_WRITTEN, &flag, SHM_TIMEOUT, WAIT_TILL_NOT);
	PIX_ERR_RETURN_IFNOT_COND(err, flag == PIXIO_SHM_READ,"");
	err = pixioShmWrite(pCtx, sizeof(I32), &desc);
	PIX_ERR_RETURN_IFNOT(err, "");
	I32 written = 0;
#ifdef SHM_PRINT
	I32 packetCount = 0;
#endif
	do {
		err = shmWait(pCtx, PIXIO_SHM_WRITTEN, &flag, SHM_TIMEOUT, WAIT_TILL_NOT);
		PIX_ERR_RETURN_IFNOT_COND(err, flag == PIXIO_SHM_READ,"");
		I32 packetSize = size - written;
		packetSize = packetSize > PIXIO_SHM_DATA_SIZE ? PIXIO_SHM_DATA_SIZE : packetSize;
		if (packetSize) {
			err = pixioShmWrite(pCtx, packetSize, (U8 *)pData + written);
			PIX_ERR_RETURN_IFNOT(err, "");
			written += packetSize;
#ifdef SHM_PRINT
			++packetCount;
#endif
		}
	} while(written < size);
#ifdef SHM_PRINT
	printf("sent %d bytes in %d packets\n", size, packetCount);
#endif
	err = shmWait(pCtx, PIXIO_SHM_WRITTEN, &flag, SHM_TIMEOUT, WAIT_TILL_NOT);
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
	err = shmWait(pCtx, PIXIO_SHM_BLOCK_START_ACK, &flag, SHM_TIMEOUT, WAIT_TILL_NOT);
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
	err = shmRead(pCtx, (U8 *)&blockSize, NULL);
	PIX_ERR_RETURN_IFNOT_COND(err, blockSize > 0, "bad blocksize, reading invalid data?");
	err = shmWait(pCtx, PIXIO_SHM_READ, &flag, SHM_TIMEOUT, WAIT_TILL_NOT);
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
#ifdef SHM_PRINT
	I32 packetCount = 0;
#endif
	do {
		PixioShmFlag flag = PIXIO_SHM_NONE;
		err = shmWait(pCtx, PIXIO_SHM_READ, &flag, SHM_TIMEOUT, WAIT_TILL_NOT);
		PIX_ERR_RETURN_IFNOT_COND(err, flag == PIXIO_SHM_WRITTEN,"");
		I32 size = 0;
		err = shmRead(pCtx, (U8 *)pDest + read, &size);
		PIX_ERR_RETURN_IFNOT(err, "");
		read += size;
#ifdef SHM_PRINT
		++packetCount;
#endif
	} while(read < pCtx->blockSize);
#ifdef SHM_PRINT
	printf("received %d bytes in %d packets\n", read, packetCount);
#endif
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
		err = shmWait(pCtx, PIXIO_SHM_CLOSE_ACK, NULL, SHM_TIMEOUT, WAIT_TILL);
		PIX_ERR_RETURN_IFNOT(err, "");
	}
	pixioShmPlatDestroy(pCtx);
	PIX_ERR_RETURN_IFNOT(err, "");
	return err;
}

PixErr pixioShmCloseServer(PixioShmCtx *pCtx) {
	return shmClose(pCtx, true);
}

PixErr pixioShmCloseClient(PixioShmCtx *pCtx) {
	return shmClose(pCtx, false);
}
