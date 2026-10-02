#ifndef SEEDLESSDS_FRONTEND_ARCHIVE_H
#define SEEDLESSDS_FRONTEND_ARCHIVE_H

#include <stddef.h>
#include <stdint.h>
#include "7z.h"
#include "7zBuf.h"
#include "7zFile.h"
#include "Lzma2Dec.h"
#include "progress.h"
#define ARCHIVE_7Z_BLOCK_BYTES 0x10000u
#define ARCHIVE_7Z_METHOD_COPY 0x0u
#define ARCHIVE_7Z_METHOD_LZMA 0x30101u
#define ARCHIVE_7Z_METHOD_LZMA2 0x21u
#define UTF16_HIGH_SURROGATE_BASE 0xd800u
#define UTF16_LOW_SURROGATE_BASE 0xdc00u
#define UTF16_SUPPLEMENTARY_BASE 0x10000u

typedef uint64_t (*archive_7z_sink_write_fn)(void *sink, const void *data, uint64_t size);

typedef struct archive_7z_window_sink {
    archive_7z_sink_write_fn write;
    uint8_t *destination;
    uint32_t cursor;
    uint32_t position;
    uint32_t room;
    uint32_t threshold;
} archive_7z_window_sink_t;

typedef struct archive_7z_range_request {
    uint8_t unmapped_0[12];
    uint32_t offset;
    void *destination;
    CSzFile *file;
    uint8_t unmapped_1[12];
    uint32_t remaining;
    const char *extension;
} archive_7z_range_request_t;

typedef struct archive_7z_range_frame {
    archive_7z_range_request_t request;
    uint8_t unmapped_0[4];
    ELzmaStatus status;
    SizeT out_size;
    SizeT in_size;
    archive_7z_window_sink_t sink;
    size_t read_size;
    CBuf name;
    ISzAlloc alloc_temp;
    ISzAlloc alloc;
    CSzArEx db;
    CFileInStream archive_stream;
    CLzma2Dec decoder;
    Byte out_buffer[ARCHIVE_7Z_BLOCK_BYTES];
    Byte in_buffer[ARCHIVE_7Z_BLOCK_BYTES];
    CLookToRead look_stream;
    uint8_t unmapped_1[0x10];
} archive_7z_range_frame_t;
#endif
