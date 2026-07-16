/* 
SPDX-FileCopyrightText: 2025 Caleb Dawson
SPDX-License-Identifier: Apache-2.0
*/

#include <windows.h>

#include <../src/shm.h>

typedef int64_t I64;

#define ERR_MESSAGE_MAX_LEN 128

PixErr pixioFileOpen(
	PixioFile *pFile,
	const char *filePath,
	PixioFileOpenType action
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
	pFile->pFile = CreateFile(
		filePath,
		access,
		false,
		NULL,
		disposition,
		FILE_ATTRIBUTE_NORMAL, NULL
	);
	if (pFile->pFile == INVALID_HANDLE_VALUE) {
		char message[ERR_MESSAGE_MAX_LEN] = {0};
		snprintf(message, ERR_MESSAGE_MAX_LEN, "Win Error: %d\n", GetLastError());
		PIX_ERR_RETURN(err, message);
	}
	return err;
}

PixErr pixioFileGetSize(PixioFile *pFile, I64 *pSize) {
	PixErr err = PIX_ERR_SUCCESS;
	LARGE_INTEGER size = {0};
	if (!GetFileSizeEx(pFile->pFile, &size)) {
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
	PixioFile *pFile,
	const void *pData,
	I64 dataSize
) {
	PixErr err = PIX_ERR_SUCCESS;
	DWORD bytesWritten;
	if (!WriteFile(pFile->pFile, pData, (I32)dataSize, &bytesWritten, NULL)) {
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

PixErr pixioFileRead(PixioFile *pFile, void *pData, I64 bytesToRead) {
	PixErr err = PIX_ERR_SUCCESS;
	DWORD bytesRead;
	if (!ReadFile(pFile->pFile, pData, (I64)bytesToRead, &bytesRead, NULL)) {
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
		bytesRead == bytesToRead,
		"Number of bytes read does not match specififed amount\n"
	);
	return err;
}

PixErr pixioFilePosSet(PixioFile *pFile, I64 pos) {
	PixErr err = PIX_ERR_SUCCESS;
	if (_fseeki64(pFile, pos, SEEK_SET)) {
		char message[ERR_MESSAGE_MAX_LEN] = {0};
		snprintf(
			message,
			ERR_MESSAGE_MAX_LEN,
			"Win error: %d\n",
			GetLastError()
		);
		PIX_ERR_RETURN(err, message);
	}
	return err;
}

PixErr pixioFileClose(PixioFile *pFile) {
	PixErr err = PIX_ERR_SUCCESS;
	bool success = CloseHandle(pFile->pFile);
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
