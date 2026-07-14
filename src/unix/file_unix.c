/* 
SPDX-FileCopyrightText: 2025 Caleb Dawson
SPDX-License-Identifier: Apache-2.0
*/

#include <stdio.h>

#include <sys/stat.h>

#include <pixenals_io_utils.h>

typedef int32_t I32;
typedef int64_t I64;

PixErr pixioFileOpen(
	PixioFile *pFile,
	const char *pFilePath,
	PixioFileOpenType action
) {
	PixErr err = PIX_ERR_SUCCESS;
	PIX_ERR_ASSERT("", pFilePath);
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
	pFile->pFile = fopen(pFilePath, mode);
	PIX_ERR_RETURN_IFNOT_COND(err, pFile->pFile, "");
	return err;
}

PixErr pixioFileGetSize(PixioFile *pFile, I64 *pSize) {
	PixErr err = PIX_ERR_SUCCESS;
	struct stat info;
	PIX_ERR_RETURN_IFNOT_COND(err, fstat(fileno(pFile->pFile), &info) != -1, "");
	*pSize = info.st_size;
	return err;
}

PixErr pixioFileWrite(
	PixioFile *pFile,
	const void *pData,
	I64 dataSize
) {
	PixErr err = PIX_ERR_SUCCESS;
	PIX_ERR_ASSERT("", pFile && pFile->pFile && pData && dataSize > 0);
	I64 bytesWritten = fwrite(pData, 1, dataSize, pFile->pFile);
	PIX_ERR_RETURN_IFNOT_COND(
		err,
		bytesWritten == dataSize,
		"Number of bytes read does not match data len"
	);
	return err;
}

PixErr pixioFileRead(
	PixioFile *pFile,
	void *pData,
	I64 bytesToRead
) {
	PixErr err = PIX_ERR_SUCCESS;
	PIX_ERR_ASSERT("", pFile && pFile->pFile && pData && bytesToRead > 0);
	I64 bytesRead = fread(pData, 1, bytesToRead, pFile->pFile);
	PIX_ERR_RETURN_IFNOT_COND(
		err,
		bytesRead == bytesToRead,
		"Number of bytes read does not match data len"
	);
	return err;
}

PixErr pixioFileClose(PixioFile *pFile) {
	PixErr err = PIX_ERR_SUCCESS;
	PIX_ERR_ASSERT("", pFile && pFile->pFile);
	fclose(pFile->pFile);
	return err;
}

I32 pixioPathMaxGet() {
	return PIXIO_PATH_MAX;
}
