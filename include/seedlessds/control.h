#ifndef SEEDLESSDS_CONTROL_H
#define SEEDLESSDS_CONTROL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct nds_control_status {
    uint64_t frames;
    uint64_t output_frames;
    uint64_t output_reads;
    uint64_t target;
    uint64_t capture_records;
    uint64_t capture_bytes;
    uint64_t audio_blocks;
    uint64_t audio_bytes;
    uint64_t first_3d;
    uint64_t first_geometry;
    uint64_t dispatches;
    uint64_t max_polygons;
    uint32_t active;
    uint32_t paused;
    uint32_t capture_complete;
    uint32_t audio_complete;
    int32_t error;
    uint64_t state_request;
    uint64_t state_completed;
    uint64_t epoch;
    int32_t state_result;
    uint32_t state_busy;
    uint64_t audio_position;
} nds_control_status_t;

void nds_control_begin(int64_t clock_seconds, int no_frameskip, unsigned compose_threads);
void nds_control_end(void);
void nds_control_status(nds_control_status_t *out);
void nds_control_frame_boundary(void);
int nds_control_pause_requested(void);
void nds_control_pause_acknowledge(int paused);
void nds_control_pause(int paused);
int nds_control_run_to(uint64_t frame);
int nds_control_step(uint64_t frames);
int nds_control_capture(int fd, uint64_t first, uint64_t last, int raw);
int nds_control_audio(int fd, uint64_t first, uint64_t last);
int nds_control_snapshot(int fd, int raw);
void nds_control_cancel_capture(void);
uint64_t nds_control_request_state(unsigned slot, int load);
void nds_control_poll_state(void *machine);

#ifdef __cplusplus
}
#endif

#endif
