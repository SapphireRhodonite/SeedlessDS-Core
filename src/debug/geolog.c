#include <stdint.h>
#include <string.h>
#include <stddef.h>
#include "hires_runtime.h"
#include "blob_symbols.h"
#include "../gpu/gpu3d/gpu3d.h"
#include "seedlessds/platform.h"
#include "mem_access.h"

#define VRAM      0x15020
#define VIDEO_IO  0x1b070
#define N_COMMANDS  0x10e740
#define N_PARAMS    0x10e75a
#define N_VRAM      0x10e776
#define N_VIDEO     0x10e78c
#define MODE_W      0x106d1f
#define INDEX_MARK  0x10
#define STATE_MARK  0x16
extern void *files_fopen_resolved(const char *path, const char *mode);
typedef char *(*fn_getcwd)(char *, size_t);
typedef int   (*fn_fputc)(int, void *);
typedef size_t(*fn_fwrite)(const void *, size_t, size_t, void *);
typedef int   (*fn_fclose)(void *);


void geolog_write_frame_dump(gpu3d_t *ctx) {

    uint8_t *file = (uint8_t *)&ctx->trace;
    uint32_t state = ctx->trace.mode;

    fn_fwrite write_fn = (fn_fwrite)sym_libc_fwrite;

    if (state == 2) {

        uint8_t *p = (uint8_t *)ctx->texture_cache;
        p = (uint8_t *)rd_ptr(p);
        uint8_t *v = (uint8_t *)rd_ptr(p);
        uint8_t *tab = v + VRAM;

        void *f_vram  = rd_ptr(file + 16);
        void *f_video = rd_ptr(file + 24);

        write_fn(rd_ptr(tab +  0), 0x20000, 1, f_vram);
        write_fn(rd_ptr(tab +  8), 0x20000, 1, f_vram);
        write_fn(rd_ptr(tab + 16), 0x20000, 1, f_vram);
        write_fn(rd_ptr(tab + 24), 0x20000, 1, f_vram);
        write_fn(rd_ptr(tab + 32), 0x10000, 1, f_vram);
        write_fn(rd_ptr(tab + 40), 0x4000,  1, f_vram);
        write_fn(rd_ptr(tab + 48), 0x4000,  1, f_vram);
        write_fn(rd_ptr(tab + 56), 0x8000,  1, f_vram);
        write_fn(rd_ptr(tab + 64), 0x4000,  1, f_vram);

        write_fn(v + VIDEO_IO, 0x8000, 1, f_video);

        fn_fclose close_fn = (fn_fclose)sym_libc_fclose;
        close_fn(rd_ptr(file));
        close_fn(rd_ptr(file + 8));
        close_fn(f_vram);
        close_fn(f_video);
        ctx->trace.mode = 0;
        return;
    }

    if (state != 1) return;

    const char *mode = "wb";

    wr_ptr(file,      nds_platform_default()->files.open(nds_platform_default()->user, "geometry_log_commands.bin", mode));
    wr_ptr(file + 8,  nds_platform_default()->files.open(nds_platform_default()->user, "geometry_log_parameters.bin",   mode));
    wr_ptr(file + 16, nds_platform_default()->files.open(nds_platform_default()->user, "geometry_log_vram.bin",     mode));
    wr_ptr(file + 24, nds_platform_default()->files.open(nds_platform_default()->user, "geometry_log_video_io.bin",    mode));

    char cwd[1024];
    ((fn_getcwd)sym_libc_getcwd)(cwd, sizeof cwd);

    fn_fputc write = (fn_fputc)sym_libc_fputc;
    void *f_cmd = rd_ptr(file);
    void *f_pair = rd_ptr(file + 8);

    uint32_t idx;

    idx = 0;
    write(INDEX_MARK, f_cmd); write_fn(&idx, 4, 1, f_pair);
    write(STATE_MARK, f_cmd); write_fn(ctx->projection_matrix, 0x40, 1, f_pair);

    idx = 1;
    write(INDEX_MARK, f_cmd); write_fn(&idx, 4, 1, f_pair);
    write(STATE_MARK, f_cmd); write_fn(ctx->position_matrix_ptr, 0x40, 1, f_pair);

    idx = 2;
    write(INDEX_MARK, f_cmd); write_fn(&idx, 4, 1, f_pair);
    write(STATE_MARK, f_cmd); write_fn(ctx->vector_matrix_ptr, 0x40, 1, f_pair);

    idx = 3;
    write(INDEX_MARK, f_cmd); write_fn(&idx, 4, 1, f_pair);
    write(STATE_MARK, f_cmd); write_fn(ctx->texture_matrix, 0x40, 1, f_pair);

    idx = ctx->matrix_mode;
    write(INDEX_MARK, f_cmd); write_fn(&idx, 4, 1, f_pair);

    ctx->trace.mode = 2;
}
#undef VRAM
#undef VIDEO_IO
#undef N_COMMANDS
#undef N_PARAMS
#undef N_VRAM
#undef N_VIDEO
#undef MODE_W
#undef INDEX_MARK
#undef STATE_MARK
