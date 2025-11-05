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
