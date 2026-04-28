/* 
SPDX-FileCopyrightText: 2025 Caleb Dawson
SPDX-License-Identifier: Apache-2.0
*/

#include <stdio.h>

#include <sys/stat.h>

#include <pixenals_io_utils.h>

typedef int32_t I32;

typedef struct PlatformContext{
	FILE *pFile;
	PixalcFPtrs alloc;
} PlatformContext;

PixErr pixioFileOpen(
	void **ppFile,
	const char *pFilePath,
	PixioFileOpenType action,
	const PixalcFPtrs *pAlloc
) {
	PixErr err = PIX_ERR_SUCCESS;
	PIX_ERR_ASSERT("", ppFile && pFilePath && pAlloc);
	PIX_ERR_ASSERT("", action == 0 || action == 1);
	char *mode = "  ";
	switch (action) {
		case PIX_IO_FILE_OPEN_WRITE:
			mode = "wb";
			break;
		case PIX_IO_FILE_OPEN_READ:
			mode = "rb";
			break;
		default:
			PIX_ERR_RETURN(err, "Invalid action passed to function\n");
	}
	PlatformContext *pState = pAlloc->fpMalloc(sizeof(PlatformContext));
	*ppFile = pState;
	pState->alloc = *pAlloc;
	pState->pFile = fopen(pFilePath, mode);
	PIX_ERR_RETURN_IFNOT_COND(err, pState->pFile, "");
	return err;
}

PixErr pixioFileGetSize(void *pFile, int64_t *pSize) {
	PixErr err = PIX_ERR_SUCCESS;
	PlatformContext *pState = pFile;
	struct stat info;
	PIX_ERR_RETURN_IFNOT_COND(err, fstat(pState->pFile->_fileno, &info) != -1, "");
	*pSize = info.st_size;
	return err;
}

PixErr pixioFileWrite(
	void *pFile,
	const void *pData,
	I32 dataSize
) {
	PixErr err = PIX_ERR_SUCCESS;
	PlatformContext *pState = pFile;
	PIX_ERR_ASSERT("", pState && pState->pFile && pData && dataSize > 0);
	I32 bytesWritten = fwrite(pData, 1, dataSize, pState->pFile);
	PIX_ERR_RETURN_IFNOT_COND(
		err,
		bytesWritten == dataSize,
		"Number of bytes read does not match data len"
	);
	return err;
}

PixErr pixioFileRead(
	void *pFile,
	void *pData,
	I32 bytesToRead
) {
	PixErr err = PIX_ERR_SUCCESS;
	PlatformContext *pState = pFile;
	PIX_ERR_ASSERT("", pState && pState->pFile && pData && bytesToRead > 0);
	I32 bytesRead = fread(pData, 1, bytesToRead, pState->pFile);
	PIX_ERR_RETURN_IFNOT_COND(
		err,
		bytesRead == bytesToRead,
		"Number of bytes read does not match data len"
	);
	return err;
}

PixErr pixioFileClose(void *pFile) {
	PixErr err = PIX_ERR_SUCCESS;
	PlatformContext *pState = pFile;
	PIX_ERR_ASSERT("", pState && pState->pFile);
	fclose(pState->pFile);
	pState->alloc.fpFree(pState);
	return err;
}

I32 pixioPathMaxGet() {
	return PIXIO_PATH_MAX;
}
