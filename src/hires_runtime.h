#ifndef RECON_H
#define RECON_H
#include <stdint.h>

int recon_shortcut_all(void);

extern unsigned recon_scale_3d;
extern unsigned recon_scale_geometry;
extern unsigned recon_scale_out;
extern unsigned recon_scale_ceiling;
extern unsigned recon_scale_render_tag;
extern unsigned recon_page_scale_tag[2];
extern unsigned recon_pacer_behind;
extern unsigned recon_block_header;
extern unsigned long recon_block_stride;
unsigned recon_block_header_bytes(unsigned e);
unsigned long recon_block_stride_bytes(unsigned e);
void recon_scale_ceiling_set(unsigned e);
void recon_scale_ceiling_read(void);
void recon_scale_request(unsigned n);
void recon_scale_note_swap(void);
unsigned recon_scale_requested(void);
void recon_scale_commit_render(void *gpu);
void recon_scale_commit_output(unsigned e);
void recon_scale_frame_close(void *gpu);
unsigned recon_page_scale(void);
extern unsigned recon_page_latched;
void recon_scale_align_ceiling(void);
void recon_height_reset(void);
void recon_drs_set(unsigned enabled, unsigned floor);
void recon_drs_reset(void);
unsigned recon_drs_transitions(void);
unsigned recon_shadow_stride_max(void);

extern unsigned recon_native;

extern int recon_native_loaded, recon_scale_3d_loaded;
void recon_native_apply(void);
void recon_scale_3d_apply(void);
static inline void recon_native_read(void)
{ if (__builtin_expect(!recon_native_loaded, 0)) recon_native_apply(); }
static inline void recon_scale_3d_read(void)
{ if (__builtin_expect(!recon_scale_3d_loaded, 0)) recon_scale_3d_apply(); }

void recon_set_scale(unsigned n, int native);
int recon_app_scale(unsigned *n, int *native);

extern unsigned recon_scale_pages;

unsigned recon_scale_output(void);

unsigned recon_shadow_stride(void);
unsigned recon_s0_shadow(void);

extern __thread unsigned recon_sublines_hires;

typedef struct recon_b2_prep {
    uint8_t *buf;
    uint8_t *lv21, *s576, *s752;
    uint8_t *three;
    uint32_t line, doubled, flags, m, modes_f, extra, bright23, sext9;
} recon_b2_prep;
struct gpu_output;
void     gpu2d_compose_render_line_prologue(void *ctx, uint32_t line, struct gpu_output *cap, uint32_t doubled,
                             uint8_t *buf, recon_b2_prep *p);
uint32_t gpu2d_compose_render_line_body(void *ctx, uint8_t *dst, struct gpu_output *cap, recon_b2_prep *p,
                            uint32_t *high);

#define RECON_B2_FRAME 0x1c00u
typedef struct recon_b2_job {
    recon_b2_prep prep;
    uint8_t *dst;
    unsigned engine_idx, slot;
    volatile unsigned busy, ready;
    uint32_t line, doubled, has_cap, bpp, scale, NS, mid, disp;
    uint8_t  obj0[0x300];
    uint8_t  obj21400[0x10];
    uint16_t fill;
    uint8_t  cap[0x60];
    uint8_t  row_source[0x200];
    uint8_t  three_copy[0x400];
    uint8_t  buf[RECON_B2_FRAME + 16];
} recon_b2_job;
int  recon_b2_active(const void *ctx);
recon_b2_job *recon_b2_take(const void *ctx);
void recon_b2_enqueue(recon_b2_job *t, const void *ctx);
void recon_b2_drain(const void *ctx);
void gpu2d_line_finish_work(uint8_t *shadow, recon_b2_job *t);

#define RECON_SCREEN_BYTES ((unsigned long)0x30000ul * recon_scale_out * recon_scale_out)
#define RECON_PAGE_PAIR_BYTES ((unsigned long)0x60000ul * recon_scale_pages * recon_scale_pages)

void     recon_band_reset(void);
unsigned recon_band_take(void);
int      recon_dynamic_share(void);
void     recon_band_publish(unsigned band, unsigned dirty);
unsigned recon_band_steal(unsigned h);
void     recon_band_publish_dirty(void);
extern __thread unsigned recon_band_current;
unsigned recon_band_sibling_dirty(unsigned band);

#define RECON_HIRES_THREADS  8u
extern unsigned recon_threads_created;
unsigned recon_threads_3d(unsigned requested);

extern unsigned recon_bands_n;
extern unsigned recon_bands_map;
extern unsigned recon_which_3d;
extern unsigned recon_height_hits, recon_height_misses, recon_height_evictions;
void     recon_height_set(unsigned char *objs, unsigned idx, unsigned value);
unsigned recon_height_get(const unsigned char *o, unsigned fallback);

#define RECON_3D_SHIFT  (UINT32_C(0x3f) - RECON_LOG2_3D)
#define RECON_LOG2_GEOMETRY ((unsigned)__builtin_ctz(recon_scale_geometry))
#define RECON_3D_SHIFT_GEOMETRY (UINT32_C(0x3f) - RECON_LOG2_GEOMETRY)

#define RECON_3D_ROW_ARRAY (0x58u * recon_scale_3d)

#define RECON_3D_WIDTH  (256u * recon_scale_3d)
#define RECON_3D_GROUPS (16u  * recon_scale_3d)

extern unsigned recon_bands_3d;
extern unsigned recon_band_rows;
void recon_bands_set(void);
#define RECON_BANDS      (recon_bands_3d)
#define RECON_BAND_ROWS (recon_band_rows)

struct gpu3d_band_bucket *recon_buckets(unsigned char *arena, unsigned side);

unsigned char *recon_buf3d_front(unsigned char *arena);
extern unsigned char *recon_arena_3d;
const unsigned char *recon_cfg3d(const unsigned char *buffer);
unsigned char *recon_buf3d_back(unsigned char *arena);
unsigned long  recon_buf3d_bytes(void);

#define RECON_LOG2_3D   ((unsigned)__builtin_ctz(recon_scale_3d))
#define RECON_3D_HEIGHT   (192u * recon_scale_3d)
#define RECON_BAND_LSR (4u + RECON_LOG2_3D)
#define RECON_WRAP_ROWS  (RECON_BAND_ROWS - 2u)
#define RECON_ROW_STEP (0x400u * recon_scale_3d)
#define RECON_STEP_LSL  (8u + RECON_LOG2_3D)

const uint32_t *recon_steps_table(const uint32_t *fallback, uint32_t base);
unsigned char *recon_buckets_lst(unsigned char *fallback);
int16_t       *recon_buckets_cnt(int16_t *fallback);

#define RECON_SHIFT_A  (10u + RECON_LOG2_3D)
#define RECON_SHIFT_B  ( 9u + RECON_LOG2_3D)
#define RECON_ARENA_A0 0x228480u

#define RECON_ARENA_B0 (0x228480u + (RECON_BANDS - 1u) * 4u * RECON_ROW_STEP)
#define RECON_ARENA_A  (RECON_ARENA_A0 + 2u * recon_scale_3d * 0x400u)
#define RECON_ARENA_B  (RECON_ARENA_B0 + recon_scale_3d * 0x400u)

extern unsigned recon_pass;
extern unsigned long recon_poly_band;

#ifndef RECON_POLY_THRESHOLD
#define RECON_POLY_THRESHOLD 61UL
#endif


#define RECON_3D_BAND    (RECON_ROW_STEP * RECON_BAND_ROWS)
#define RECON_LAST_ROW (RECON_ROW_STEP * (RECON_WRAP_ROWS + 1u))
#define RECON_3D_SPAN    (RECON_ROW_STEP * RECON_WRAP_ROWS)

#define RECON_3D_PLANE  (0x4000u * recon_scale_3d * recon_scale_3d < 0x10000u                          ? 0x10000u : 0x4000u * recon_scale_3d * recon_scale_3d)
#define RECON_3D_PLANE2 (2u * RECON_3D_PLANE)

#define RECON_PLANE2_SIZE ((0x1000u * recon_scale_3d * recon_scale_3d < 0x4000u                           ? 0x4000u : 0x1000u * recon_scale_3d * recon_scale_3d) * (recon_scale_3d > 2u ? 2u : 1u))

#define RECON_PLANE2_USED (RECON_BAND_ROWS * RECON_3D_WIDTH < 0x4000u                            ? 0x4000u : RECON_BAND_ROWS * RECON_3D_WIDTH)

#define RECON_3D_HEADER    (2u * RECON_3D_PLANE + RECON_PLANE2_SIZE)
#define RECON_3D_BLOCK (RECON_3D_HEADER + 0x100u)

unsigned char *recon_ctx3d_base(unsigned char *arena);

unsigned char *recon_tables_base(unsigned char *arena);

#endif

unsigned long nds_module_base(void);
int nds_clock_override(long *value);
void nds_output_read_begin(void);
void nds_capture_screens(const void *page, const volatile uint32_t *mode);
void nds_trace_3d_dispatch(void);
unsigned nds_configured_2d_threads(unsigned fallback);
void nds_capture_audio(const void *pcm, unsigned samples);
void nds_trace_2d_start(unsigned threads, int boss_only, int shut_down);
void nds_trace_2d_line(void);
void nds_trace_2d_serial(void);
void nds_trace_2d_fixed(void);
void nds_trace_2d_race(void);
void nds_trace_2d_report(unsigned generation);
void nds_trace_drain_wait(int engine, unsigned in_flight_0, unsigned in_flight_1, unsigned head,
                       unsigned tail, unsigned sleeping, unsigned active, unsigned threads,
                       unsigned long turns);
extern unsigned long nds_output_reads;
extern unsigned long nds_output_frames;
