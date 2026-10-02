#ifndef SEEDLESSDS_FRONTEND_ARCHIVE_PROGRESS_H
#define SEEDLESSDS_FRONTEND_ARCHIVE_PROGRESS_H

#include <stddef.h>
#include <stdint.h>

typedef struct archive_rar_window_sink {
    uint8_t *destination;
    uint32_t room;
    uint32_t threshold;
    uint32_t position;
} archive_rar_window_sink_t;

typedef struct archive_progress {
    uint64_t total;
    uint64_t done;
    archive_rar_window_sink_t rar_sink;
} archive_progress_t;

extern archive_progress_t archive_progress_state;

#define ARCHIVE_PROGRESS (&archive_progress_state)
#endif
