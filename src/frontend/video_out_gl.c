#include <stdint.h>
#include "hires_runtime.h"
#include "blob_symbols.h"
#include <dlfcn.h>
#include "present_hook.h"
#include <string.h>
#include <stddef.h>
#include <stdarg.h>
#include "frontend/video_out_gl.h"
#include "seedlessds/platform.h"
#include "core_internals.h"

uint64_t video_out_gl_hook_noop(uint64_t x0)
{
    return x0;
}

uint64_t video_out_gl_hook_noop_clone(uint64_t x0)
{
    return x0;
}

void video_out_gl_init_hook_noop(void) {
}

#define GL_RGB                   0x1907
#define GL_RGBA                  0x1908
#define GL_UNSIGNED_BYTE         0x1401
#define GL_UNSIGNED_SHORT_5_6_5  0x8363

void video_out_gl_set_texture_format(int32_t bpp) {
    video_out_gl_t *g = VIDEO_OUT_GL;
    int is16 = (bpp == 16);
    *(volatile int32_t *)&g->bits_per_pixel = bpp;
    *(volatile uint32_t *)&g->gl_type = is16 ? GL_UNSIGNED_SHORT_5_6_5 : GL_UNSIGNED_BYTE;
    *(volatile uint32_t *)&g->gl_format = is16 ? GL_RGB : GL_RGBA;
}
#undef GL_RGB
#undef GL_RGBA
#undef GL_UNSIGNED_BYTE
#undef GL_UNSIGNED_SHORT_5_6_5

#define GL_RGBA 0x1908
#define GL_UNSIGNED_BYTE 0x1401
typedef void *(*fn_memset)(void *, int, size_t);
typedef int   (*fn_posix_memalign)(void **, size_t, size_t);
typedef int   (*fn_sync_init)(void *, void *);

int video_out_gl_context_init(void) {
    video_out_gl_t *ctx = VIDEO_OUT_GL;

    static fn_memset core_memset;
    static fn_posix_memalign core_posix_memalign;
    static fn_sync_init core_mutex_init;
    static fn_sync_init core_cond_init;
    if (!core_memset) {
        core_memset         = (fn_memset)sym_libc_memset;
        core_posix_memalign = (fn_posix_memalign)sym_libc_posix_memalign;
        core_mutex_init      = (fn_sync_init)platform_mutex_init;
        core_cond_init        = (fn_sync_init)platform_cond_init;
    }

    core_memset(ctx, 0, sizeof *ctx);

    ctx->bits_per_pixel = 32;
    ctx->gl_type = GL_UNSIGNED_BYTE;

    ctx->gl_format = GL_RGBA;

    ctx->screen_height[0] = VIDEO_OUT_GL_DEFAULT_SCREEN_HEIGHT;
    ctx->screen_height[1] = VIDEO_OUT_GL_DEFAULT_SCREEN_HEIGHT;

    ctx->screen_changed[0] = 0;
    ctx->screen_changed[1] = 0;
    ctx->screen_width[0] = 256;
    ctx->screen_width[1] = 256;

    recon_native_read();
    recon_scale_3d_read();
    recon_scale_align_ceiling();
    size_t pages_size = 0x300000u;
    recon_scale_pages = 2u;
    if (recon_native) {
        recon_scale_pages = recon_scale_3d;
        pages_size = (size_t)4u * 0x30000u * recon_scale_3d * recon_scale_3d;
    }


    void *buf = NULL;

    if (!recon_pages_reserve) {
        void *h = dlopen("libseedless_bridge.so", RTLD_NOW | RTLD_NOLOAD);
        if (h) recon_pages_reserve = (void *(*)(unsigned long))dlsym(h, "sds_reserve_pages");
    }
    if (recon_pages_reserve) buf = recon_pages_reserve((unsigned long)pages_size);
    if (!buf) core_posix_memalign(&buf, 0x10, pages_size);
    ctx->page_memory = buf;

    unsigned char *half = (unsigned char *)buf + pages_size / 2u;
    ctx->page[0] = buf;
    ctx->page[1] = half;

    core_memset(buf, 0, pages_size);

    core_mutex_init(ctx->mutex, NULL);

    return core_cond_init(ctx->cond, NULL);
}
#undef GL_RGBA

#define GL_TEXTURE_2D 0x0de1u
#define GL_TRIANGLES  0x0004u

typedef void (*fn_bind_texture)(unsigned int dest, unsigned int texture);
typedef void (*fn_tex_sub_image_2d)(unsigned int dest, int level,
                                    int x, int y, int width, int height,
                                    unsigned int format, unsigned int type,
                                    const void *data);
typedef void (*fn_draw_arrays)(unsigned int mode, int first, int count);

void video_out_gl_draw_screen_rect4(uint32_t texture, uint32_t entry_idx)
{

    if (VIDEO_OUT_GL->page_memory == NULL)
        return;

    uint32_t parity = VIDEO_OUT_GL->page_parity;

    uint32_t level = VIDEO_OUT_GL->screen_mode[(int32_t)entry_idx];

    uint32_t slot = (~parity) & 1u;
    const uint8_t *base = VIDEO_OUT_GL->page[slot];

    uint32_t dimension = level + 1u;
    uint32_t width = dimension << 8;
    uint32_t height = (dimension + (dimension << 1u)) << 6;

    uint32_t half = entry_idx & 1u;
    const uint8_t *data = base + (uint64_t)half * (uint64_t)VIDEO_OUT_GL_SCREEN_PAGE_BYTES;

    fn_bind_texture bind_texture = (fn_bind_texture)sym_libc_glBindTexture;
    bind_texture(GL_TEXTURE_2D, texture);

    uint32_t format = VIDEO_OUT_GL->gl_format;
    uint32_t type = VIDEO_OUT_GL->gl_type;

    fn_tex_sub_image_2d upload_texture =
        (fn_tex_sub_image_2d)sym_libc_glTexSubImage2D;
    upload_texture(GL_TEXTURE_2D, 0, 0, 0, (int)width, (int)height,
                 format, type, data);

    fn_draw_arrays draw = (fn_draw_arrays)sym_libc_glDrawArrays;
    draw(GL_TRIANGLES, 18, 6);
}
#undef GL_TEXTURE_2D
#undef GL_TRIANGLES

#define GL_TEXTURE_2D 0x0de1u
#define GL_TRIANGLES  0x0004u

typedef void (*fn_bind_texture_6)(unsigned int dest, unsigned int texture);
typedef void (*fn_tex_sub_image_2d_6)(unsigned int dest, int level,
                                    int x, int y, int width, int height,
                                    unsigned int format, unsigned int type,
                                    const void *data);
typedef void (*fn_draw_arrays_6)(unsigned int mode, int first, int count);

void video_out_gl_draw_screen_rect2(uint32_t texture, uint32_t entry_idx)
{

    if (VIDEO_OUT_GL->page_memory == NULL)
        return;

    uint32_t parity = VIDEO_OUT_GL->page_parity;

    uint32_t level = VIDEO_OUT_GL->screen_mode[(int32_t)entry_idx];

    uint32_t slot = (~parity) & 1u;
    const uint8_t *base = VIDEO_OUT_GL->page[slot];

    uint32_t dimension = level + 1u;
    uint32_t width = dimension << 8;
    uint32_t height = (dimension + (dimension << 1u)) << 6;

    uint32_t half = entry_idx & 1u;
    const uint8_t *data = base + (uint64_t)half * (uint64_t)VIDEO_OUT_GL_SCREEN_PAGE_BYTES;

    fn_bind_texture_6 bind_texture = (fn_bind_texture_6)sym_libc_glBindTexture;
    bind_texture(GL_TEXTURE_2D, texture);

    uint32_t format = VIDEO_OUT_GL->gl_format;
    uint32_t type = VIDEO_OUT_GL->gl_type;

    fn_tex_sub_image_2d_6 upload_texture =
        (fn_tex_sub_image_2d_6)sym_libc_glTexSubImage2D;
    upload_texture(GL_TEXTURE_2D, 0, 0, 0, (int)width, (int)height,
                 format, type, data);

    fn_draw_arrays_6 draw = (fn_draw_arrays_6)sym_libc_glDrawArrays;
    draw(GL_TRIANGLES, 6, 6);
}
#undef GL_TEXTURE_2D
#undef GL_TRIANGLES


int32_t video_out_gl_load_shader_recipe_slot0(const char *param_1, uint32_t param_2, uint32_t param_3) {

    void *slot = &VIDEO_OUT_GL->renderer[0];

    return video_out_gl_load_shader_recipe(param_1, slot, (void *)(uintptr_t)param_2,
                                           (void *)(uintptr_t)param_3);
}

typedef void (*fn_120418)(void *, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);

void video_out_gl_call_renderer_object_slot0(int a, int b, int c, int d, int e, int f) {
    void *self = &VIDEO_OUT_GL->renderer[0];
    fn_120418 dest = video_out_gl_renderer_resize_nodes;
    dest(self, a, b, c, d, e, f);
}

typedef int (*fn_mutex)(void *);
typedef void (*fn_active)(unsigned int);
typedef void (*fn_bind)(unsigned int, unsigned int);
typedef void (*fn_sub)(unsigned int, int, int, int, int, int,
                        unsigned int, unsigned int, const void *);
typedef void (*fn_120690)(void *, uint32_t, int32_t, int32_t, uint32_t, uint32_t);

void video_out_gl_render_frame(uint32_t param_1, uint32_t param_2,
                         uint32_t param_3, uint32_t param_4,
                         uint32_t param_5, uint32_t param_6,
                         uint32_t param_7, uint32_t param_8,
                         uint32_t param_9, uint32_t param_10)
{

    if (VIDEO_OUT_GL->page_memory == NULL) return;

    if (recon_present_mode & RECON_PRESENT_NO_UPLOAD) {
        fn_120690 p = video_out_gl_renderer_draw_passes;
        ((fn_active)sym_libc_glActiveTexture)(0x84c0u);
        p(&VIDEO_OUT_GL->renderer[0], param_1, param_3, param_5, param_6, param_7);
        if (param_2 != 0) p(&VIDEO_OUT_GL->renderer[0], param_2, param_4, param_5, param_8, param_9);
        return;
    }

    fn_mutex p_lock    = nds_platform_default()->threads.mutex_lock;
    fn_mutex p_unlock  = nds_platform_default()->threads.mutex_unlock;
    fn_active p_active = (fn_active)sym_libc_glActiveTexture;
    fn_bind   p_bind   = (fn_bind)sym_libc_glBindTexture;
    fn_sub    p_sub    = (fn_sub)sym_libc_glTexSubImage2D;
    fn_120690 p_120690 = video_out_gl_renderer_draw_passes;

    p_lock(VIDEO_OUT_GL->mutex);

    uint32_t idx     = param_10 & 1u;
    uint32_t idx_sel = (~VIDEO_OUT_GL->page_parity) & 1u;
    if (recon_page_latched <= 1u) { idx_sel = recon_page_latched; recon_page_latched = 0xffu; }
    const uint8_t *lvar3 = VIDEO_OUT_GL->page[idx_sel];
    int32_t  iVar1   = (int32_t)VIDEO_OUT_GL->screen_mode[idx];
    int32_t  iVar2   = (int32_t)VIDEO_OUT_GL->screen_mode[idx ^ 1u];

    p_unlock(VIDEO_OUT_GL->mutex);

    recon_native_read();
    uint32_t page_scale = recon_page_scale_tag[idx_sel];
    if (page_scale < 1u || page_scale > 8u) page_scale = recon_scale_output();
    uint32_t n1 = (recon_native && iVar1 + 1 >= 2)
                  ? page_scale : (uint32_t)(iVar1 + 1);
    uint32_t n2 = (recon_native && iVar2 + 1 >= 2)
                  ? page_scale : (uint32_t)(iVar2 + 1);
    uint64_t step_p = recon_native ? (uint64_t)0x30000ul * page_scale * page_scale : (uint64_t)VIDEO_OUT_GL_SCREEN_PAGE_BYTES;

    p_active(0x84c0u);
    p_bind(0xde1u, param_1);
    p_sub(0xde1u, 0, 0, 0,
          (int)(n1 * 0x100u), (int)(n1 * 0xc0u),
          VIDEO_OUT_GL->gl_format, VIDEO_OUT_GL->gl_type,
          lvar3 + (uint64_t)idx * step_p);

    p_120690(&VIDEO_OUT_GL->renderer[0], param_1, param_3, param_5, param_6, param_7);

    if (param_2 != 0) {
        p_active(0x84c0u);
        p_bind(0xde1u, param_2);
        p_sub(0xde1u, 0, 0, 0,
              (int)(n2 * 0x100u), (int)(n2 * 0xc0u),
              VIDEO_OUT_GL->gl_format, VIDEO_OUT_GL->gl_type,
              lvar3 + (uint64_t)(idx ^ 1u) * step_p);

        p_120690(&VIDEO_OUT_GL->renderer[0], param_2, param_4, param_5, param_8, param_9);
    }
}


int32_t video_out_gl_load_shader_recipe_slot1(const char *param_1, uint32_t param_2, uint32_t param_3) {

    void *slot = &VIDEO_OUT_GL->renderer[1];

    return video_out_gl_load_shader_recipe(param_1, slot, (void *)(uintptr_t)param_2,
                                           (void *)(uintptr_t)param_3);
}

typedef void (*fn_120418_11)(void *, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);

void video_out_gl_call_renderer_object_slot1(int a, int b, int c, int d, int e, int f) {
    void *self = &VIDEO_OUT_GL->renderer[1];
    fn_120418_11 dest = video_out_gl_renderer_resize_nodes;
    dest(self, a, b, c, d, e, f);
}

typedef void (*fn_active_12)(unsigned int);
typedef void (*fn_bind_12)(unsigned int, unsigned int);
typedef void (*fn_sub_12)(unsigned int, int, int, int, int, int,
                       unsigned int, unsigned int, const void *);
void video_out_gl_render_single_screen(uint64_t texture, uint32_t entry_idx,
                             uint32_t param_3, uint32_t param_4,
                             uint32_t param_5, uint32_t param_6)
{

    if (VIDEO_OUT_GL->page_memory == NULL)
        return;

    if (recon_present_mode & RECON_PRESENT_EXT_NO_UPLOAD) {

        ((fn_active_12)sym_libc_glActiveTexture)(0x84c0u);
        video_out_gl_renderer_draw_passes(&VIDEO_OUT_GL->renderer[1], (uint32_t)texture,
                                          param_3, param_4, param_5, param_6);
        return;
    }

    nds_platform_default()->threads.mutex_lock(VIDEO_OUT_GL->mutex);

    if (VIDEO_OUT_GL->page_memory == NULL)
        return;

    uint32_t selector = VIDEO_OUT_GL->page_parity;
    uint32_t level = VIDEO_OUT_GL->screen_mode[entry_idx];
    const uint8_t *base_pixels = VIDEO_OUT_GL->page[(~selector) & 1u];
    uint32_t dimension = level + 1u;
    uint32_t width = dimension << 8;
    uint32_t triple = dimension + (dimension << 1);
    uint32_t height = triple << 6;
    const void *pixels = base_pixels +
                              (uint64_t)(entry_idx & 1u) * VIDEO_OUT_GL_SCREEN_PAGE_BYTES;

    nds_platform_default()->threads.mutex_unlock(VIDEO_OUT_GL->mutex);

    ((fn_active_12)sym_libc_glActiveTexture)(0x84c0u);
    ((fn_bind_12)sym_libc_glBindTexture)(0x0de1u, (uint32_t)texture);
    uint32_t format = VIDEO_OUT_GL->gl_format;
    uint32_t type = VIDEO_OUT_GL->gl_type;
    ((fn_sub_12)sym_libc_glTexSubImage2D)(0x0de1u, 0, 0, 0,
                                             (int)width, (int)height,
                                             format, type, pixels);

    video_out_gl_renderer_draw_passes(&VIDEO_OUT_GL->renderer[1], (uint32_t)texture,
                                      param_3, param_4, param_5, param_6);
}

#define BUFFER_SIZE (recon_native ? (size_t)RECON_PAGE_PAIR_BYTES : (size_t)VIDEO_OUT_GL_PAGE_PAIR_BYTES)
#define GL_TEXTURE_2D 0x0de1
typedef void *(*fn_memset_13)(void *, int, size_t);
typedef void  (*fn_bind_13)(unsigned, unsigned);
typedef void  (*fn_sub_13)(unsigned, int, int, int, int, int, unsigned, unsigned,
                        const void *);

void video_out_gl_clear_screens(unsigned texture_a, unsigned texture_b) {

    if (VIDEO_OUT_GL->page_memory == NULL) return;

    void *buf0 = VIDEO_OUT_GL->page[0];
    void *buf1 = VIDEO_OUT_GL->page[1];

    fn_memset_13 p_memset = (fn_memset_13)sym_libc_memset;
    fn_bind_13   p_bind   = (fn_bind_13)sym_libc_glBindTexture;
    fn_sub_13    p_sub    = (fn_sub_13)sym_libc_glTexSubImage2D;

    recon_native_read();
    p_memset(buf0, 0, BUFFER_SIZE);
    p_memset(buf1, 0, BUFFER_SIZE);

    int32_t format = (int32_t)VIDEO_OUT_GL->gl_format;
    int32_t type    = (int32_t)VIDEO_OUT_GL->gl_type;
    int32_t side_a  = (int32_t)VIDEO_OUT_GL->screen_width[0];
    int32_t side_b  = (int32_t)VIDEO_OUT_GL->screen_width[1];

    p_bind(GL_TEXTURE_2D, texture_a);
    p_sub(GL_TEXTURE_2D, 0, 0, 0, side_a, side_a, (unsigned)format,
          (unsigned)type, buf0);

    p_bind(GL_TEXTURE_2D, texture_b);
    p_sub(GL_TEXTURE_2D, 0, 0, 0, side_b, side_b, (unsigned)format,
          (unsigned)type, buf0);
}
#undef BUFFER_SIZE
#undef GL_TEXTURE_2D

static void *(*core_memset)(void *, int, size_t);

void *video_out_gl_clear_framebuffers(void *arg) {
    if (!core_memset)
        core_memset = (void *(*)(void *, int, size_t))sym_libc_memset;

    if (VIDEO_OUT_GL->page_memory == NULL)
        return arg;

    void **bufs = VIDEO_OUT_GL->page;

    recon_native_read();
    size_t page = recon_native ? (size_t)RECON_PAGE_PAIR_BYTES : VIDEO_OUT_GL_PAGE_PAIR_BYTES;
    core_memset(bufs[0], 0, page);
    return core_memset(bufs[1], 0, page);
}

typedef unsigned int (*fn_glCreateShader)(unsigned int type);
typedef void         (*fn_glShaderSource)(unsigned int shader, int count,
                                           const char *const *strs,
                                           const int *lens);
typedef void         (*fn_glCompileShader)(unsigned int shader);
typedef void         (*fn_glGetShaderiv)(unsigned int shader,
                                          unsigned int pname, int *param);
typedef void         (*fn_glGetShaderInfoLog)(unsigned int shader,
                                               int buf_size, int *len,
                                               char *info_log);
typedef unsigned int (*fn_glCreateProgram)(void);
typedef void         (*fn_glAttachShader)(unsigned int program,
                                           unsigned int shader);
typedef void         (*fn_glLinkProgram)(unsigned int program);
typedef void         (*fn_glDetachShader)(unsigned int program,
                                           unsigned int shader);
typedef void         (*fn_glDeleteShader)(unsigned int shader);
typedef int          (*fn_glGetAttribLocation)(unsigned int program,
                                                const char *name);
typedef int          (*fn_glGetUniformLocation)(unsigned int program,
                                                 const char *name);
typedef void         *(*fn_malloc)(unsigned long size);
typedef void          (*fn_free)(void *p);

int video_out_gl_compile_shader_program(const char *const *vertex_src,
                        const char *const *fragment_src,
                        void *program_info,
                        char **out_error)
{

    static fn_glCreateShader       core_glCreateShader;
    static fn_glShaderSource       core_glShaderSource;
    static fn_glCompileShader      core_glCompileShader;
    static fn_glGetShaderiv        core_glGetShaderiv;
    static fn_glGetShaderInfoLog   core_glGetShaderInfoLog;
    static fn_glCreateProgram      core_glCreateProgram;
    static fn_glAttachShader       core_glAttachShader;
    static fn_glLinkProgram        core_glLinkProgram;
    static fn_glDetachShader       core_glDetachShader;
    static fn_glDeleteShader       core_glDeleteShader;
    static fn_glGetAttribLocation  core_glGetAttribLocation;
    static fn_glGetUniformLocation core_glGetUniformLocation;
    static fn_malloc               core_malloc;
    static fn_free                 core_free;

    if (!core_glCreateShader)
        core_glCreateShader = (fn_glCreateShader)sym_libc_glCreateShader;
    if (!core_glShaderSource)
        core_glShaderSource = (fn_glShaderSource)sym_libc_glShaderSource;
    if (!core_glCompileShader)
        core_glCompileShader = (fn_glCompileShader)sym_libc_glCompileShader;
    if (!core_glGetShaderiv)
        core_glGetShaderiv = (fn_glGetShaderiv)sym_libc_glGetShaderiv;
    if (!core_glGetShaderInfoLog)
        core_glGetShaderInfoLog = (fn_glGetShaderInfoLog)sym_libc_glGetShaderInfoLog;
    if (!core_glCreateProgram)
        core_glCreateProgram = (fn_glCreateProgram)sym_libc_glCreateProgram;
    if (!core_glAttachShader)
        core_glAttachShader = (fn_glAttachShader)sym_libc_glAttachShader;
    if (!core_glLinkProgram)
        core_glLinkProgram = (fn_glLinkProgram)sym_libc_glLinkProgram;
    if (!core_glDetachShader)
        core_glDetachShader = (fn_glDetachShader)sym_libc_glDetachShader;
    if (!core_glDeleteShader)
        core_glDeleteShader = (fn_glDeleteShader)sym_libc_glDeleteShader;
    if (!core_glGetAttribLocation)
        core_glGetAttribLocation = (fn_glGetAttribLocation)sym_libc_glGetAttribLocation;
    if (!core_glGetUniformLocation)
        core_glGetUniformLocation = (fn_glGetUniformLocation)sym_libc_glGetUniformLocation;
    if (!core_malloc)
        core_malloc = (fn_malloc)sym_libc_malloc;
    if (!core_free)
        core_free = (fn_free)sym_libc_free;

    gl_program_t *p3 = (gl_program_t *)program_info;

    int loglen = 0;
    int status = 0;

    unsigned int vs = core_glCreateShader(0x8b31);
    core_glShaderSource(vs, 1, vertex_src, 0);
    core_glCompileShader(vs);
    core_glGetShaderiv(vs, 0x8b84, &loglen);
    core_glGetShaderiv(vs, 0x8b81, &status);

    if (status == 0) goto vs_fail;

fs_compile:
    {
        unsigned int fs = core_glCreateShader(0x8b30);
        core_glShaderSource(fs, 1, fragment_src, 0);
        core_glCompileShader(fs);
        core_glGetShaderiv(fs, 0x8b84, &loglen);
        core_glGetShaderiv(fs, 0x8b81, &status);

        if (status == 0) goto fs_fail_with_fs;

    link:
        {
            unsigned int program = core_glCreateProgram();
            p3->program = program;
            core_glAttachShader(p3->program, vs);
            core_glAttachShader(p3->program, fs);
            core_glLinkProgram(p3->program);
            core_glDetachShader(p3->program, vs);
            core_glDetachShader(p3->program, fs);
            core_glDeleteShader(vs);
            core_glDeleteShader(fs);

            p3->vertex_coordinate_attrib = core_glGetAttribLocation(p3->program,
                                              "a_vertex_coordinate");
            p3->texture_coordinate_attrib = core_glGetAttribLocation(p3->program,
                                              "a_texture_coordinate");
            p3->texture_size_uniform = core_glGetUniformLocation(p3->program,
                                               "u_texture_size");
            p3->target_size_uniform = core_glGetUniformLocation(p3->program,
                                               "u_target_size");
            p3->time_uniform = core_glGetUniformLocation(p3->program,
                                               "u_time");

            return 0;
        }

    fs_fail_with_fs:
        if (out_error != 0) {
            int loglen_local = loglen;
            void *buf = core_malloc((unsigned long)(long)loglen_local);
            *out_error = (char *)buf;
            if (buf != 0) {
                int length_ignored = 0;
                core_glGetShaderInfoLog(fs, loglen_local, &length_ignored, (char *)buf);
                if (status != 0) {

                    core_free(*out_error);
                    *out_error = 0;
                    if (status != 0) goto link;
                }
            }
        }
        core_glDeleteShader(fs);
        return -1;
    }

vs_fail:
    if (out_error != 0) {
        int loglen_local = loglen;
        void *buf = core_malloc((unsigned long)(long)loglen_local);
        *out_error = (char *)buf;
        if (buf != 0) {
            int length_ignored = 0;
            core_glGetShaderInfoLog(vs, loglen_local, &length_ignored, (char *)buf);
            if (status != 0) {
                core_free(*out_error);
                *out_error = 0;
                if (status != 0) goto fs_compile;
            }
        }
    }
    core_glDeleteShader(vs);
    return -1;
}

typedef long          (*fn_ftell)(void *);
typedef char          *(*fn_fgets)(char *, int, void *);
typedef char          *(*fn_strstr)(const char *, const char *);
typedef int            (*fn_fseek)(void *, long, int);
typedef void           *(*fn_malloc_16)(unsigned long);
typedef void           *(*fn_memcpy)(void *, const void *, unsigned long);
typedef unsigned long  (*fn_fread)(void *, unsigned long, unsigned long, void *);

void *video_out_gl_shader_recipe_read_block(void *fp, const char *needle, void *param_3, void *param_4)
{

    char line[1024];
    char *l, *p;
    long off, pos_mark, pos_after, n;
    void *dest, *ret, *ptr_p3, *ptr_p4, *pos1, *pos2;
    unsigned long p4_size, p3_size;

    gl_text_block_t *p3 = (gl_text_block_t *)param_3;
    gl_text_block_t *p4 = (gl_text_block_t *)param_4;

    off = ((fn_ftell)sym_libc_ftell)(fp);
    l = ((fn_fgets)sym_libc_fgets)(line, 0x400, fp);
    if (l == 0) goto no_loop;

    p = ((fn_strstr)sym_libc_strstr)(line, needle);
    if (p == 0) goto loop;

no_loop:
    pos_mark = 0;
    n = pos_mark - off;
    if ((int)n >= 1) goto common;
    ret = 0;

    return ret;

loop:
    for (;;) {
        pos_mark = ((fn_ftell)sym_libc_ftell)(fp);
        l = ((fn_fgets)sym_libc_fgets)(line, 0x400, fp);
        if (l == 0) break;
        p = ((fn_strstr)sym_libc_strstr)(line, needle);
        if (p != 0) break;
    }
    n = pos_mark - off;
    if ((int)n < 1) goto no_loop_ret;
    goto common;

no_loop_ret:
    ret = 0;
    return ret;

common:
    pos_after = ((fn_ftell)sym_libc_ftell)(fp);
    ((fn_fseek)sym_libc_fseek)(fp, off, 0);

    p3_size = p3->size;
    p4_size = p4->size;

    dest = ((fn_malloc_16)sym_libc_malloc)((unsigned long)n + p3_size + p4_size + 1);
    ret = dest;
    if (dest != 0) {
        if (p4_size != 0) {
            ptr_p4 = p4->text;
            if (ptr_p4 != 0) {
                ((fn_memcpy)sym_libc_memcpy)(dest, ptr_p4, p4_size);
            }
        }

        ptr_p3 = p3->text;
        pos1 = (char *)dest + p4_size;
        if (ptr_p3 != 0) {
            p3_size = p3->size;
            if (p3_size != 0) {
                ((fn_memcpy)sym_libc_memcpy)(pos1, ptr_p3, p3_size);
            }
        }

        p3_size = p3->size;

        pos2 = (char *)pos1 + p3_size;
        ((fn_fread)sym_libc_fread)(pos2, 1, (unsigned long)n, fp);
        ((char *)pos2)[n] = 0;
    }

    ((fn_fseek)sym_libc_fseek)(fp, pos_after, 0);

    return ret;
}

static int (*core_vsnprintf)(char *, size_t, const char *, va_list);

int video_out_gl_format_log_path(char *param_1, long param_2, long param_3,
                        long param_4, ...) {
    (void)param_2; (void)param_3; (void)param_4;
    if (!core_vsnprintf)
        core_vsnprintf = (int (*)(char *, size_t, const char *, va_list))
                         sym_libc_vsnprintf;

    const char *format = "%s.log";

    va_list ap;
    va_start(ap, param_4);
    int r = core_vsnprintf(param_1, 0x400, format, ap);
    va_end(ap);
    return r;
}

extern void video_out_gl_shader_recipe_extract_value_18(const char *line, char *dest, uint64_t size) __asm__("video_out_gl_shader_recipe_extract_value");


int32_t video_out_gl_shader_recipe_read_options(gl_renderer_t *r, void *stream, char *buf,
                                                const gl_recipe_tag_t *tag)
{

    typedef char *(*fn_fgets)(char *, int, void *);
    typedef char *(*fn_strstr)(const char *, const char *);
    typedef char *(*fn_strchr)(const char *, int);
    typedef int   (*fn_sscanf)(const char *, const char *, ...);
    typedef void *(*fn_malloc)(unsigned long);

    static fn_fgets  core_fgets;
    static fn_strstr core_strstr;
    static fn_strchr core_strchr;
    static fn_sscanf core_sscanf;
    static fn_malloc core_malloc;
    if (!core_fgets)  core_fgets  = (fn_fgets)sym_libc_fgets;
    if (!core_strstr) core_strstr = (fn_strstr)sym_libc_strstr;
    if (!core_strchr) core_strchr = (fn_strchr)sym_libc_strchr;
    if (!core_sscanf) core_sscanf = (fn_sscanf)sym_libc_sscanf;
    if (!core_malloc) core_malloc = (fn_malloc)sym_libc_malloc;

    char *field_name = r->name;
    char *line;
    char *p;

    line = core_fgets(buf, 0x400, stream);
    if (line == 0) goto end_empty;

    p = core_strstr(buf, tag->close);
    if (p != 0) goto found;

check_name:
    p = core_strstr(buf, "name=");
    if (p != 0) goto is_name;

    p = core_strstr(buf, "textures=");
    if (p == 0) goto next_line;

    if (r->textures != NULL) goto error_generic;
    if (r->texture_count != 0) goto error_generic;

    {
        char *eq = core_strchr(buf, '=');
        if (eq == 0) goto no_equal;

        int local_6c = 0;
        int scanf_ret = core_sscanf(eq, "=%d", &local_6c);

        int32_t count = (scanf_ret == 1) ? (int32_t)local_6c : -1;
        r->texture_count = (uint32_t)count;
        if ((uint32_t)(count - 1) > VIDEO_OUT_GL_MAX_TEXTURES - 1u) goto error_generic;

        void *updated = core_malloc((uint64_t)((int64_t)count * sizeof(gl_recipe_texture_t)));
        r->textures = (gl_recipe_texture_t *)updated;
        if (updated == 0) goto error_generic;
        goto next_line;
    }

is_name:
    video_out_gl_shader_recipe_extract_value(buf, field_name, sizeof r->name);

next_line:
    line = core_fgets(buf, 0x400, stream);
    if (line == 0) goto end_empty;

    p = core_strstr(buf, tag->close);
    if (p != 0) goto found;
    goto check_name;

found:
    return 0;

end_empty:
    return 0;

no_equal:
    r->texture_count = 0xffffffffu;
    return -1;

error_generic:
    return -1;
}

static long   (*core_ftell)(void *);
static char  *(*core_fgets)(char *, int, void *);
static char  *(*core_strstr)(const char *, const char *);
static int    (*core_fseek)(void *, long, int);
static void  *(*core_realloc)(void *, size_t);
static size_t (*core_fread)(void *, size_t, size_t, void *);

int32_t video_out_gl_shader_recipe_read_text_block(gl_renderer_t *obj, void *f, char *buf,
                            const gl_recipe_tag_t *tag) {
    if (!core_ftell) {
        core_ftell   = (long (*)(void *))sym_libc_ftell;
        core_fgets   = (char *(*)(char *, int, void *))sym_libc_fgets;
        core_strstr  = (char *(*)(const char *, const char *))sym_libc_strstr;
        core_fseek   = (int (*)(void *, long, int))sym_libc_fseek;
        core_realloc = (void *(*)(void *, size_t))sym_libc_realloc;
        core_fread   = (size_t (*)(void *, size_t, size_t, void *))sym_libc_fread;
    }

    long off = core_ftell(f);

    int32_t sel = tag->id;
    gl_text_block_t *dest;
    if (sel == GL_RECIPE_TAG_FHEADER)      dest = &obj->fragment_header;
    else if (sel == GL_RECIPE_TAG_VHEADER) dest = &obj->vertex_header;
    else                                   dest = &obj->header;

    const char *needle = tag->close;

    char *line = core_fgets(buf, 0x400, f);
    long mark = 0;
    while (line != NULL && core_strstr(buf, needle) == NULL) {
        mark = core_ftell(f);
        line = core_fgets(buf, 0x400, f);
    }

    long n = mark - off;
    if ((int32_t)n < 1) {
        return 0;
    }

    long restore = core_ftell(f);
    core_fseek(f, off, 0);

    void *current  = dest->text;
    long  act_size  = (long)dest->size;
    void *updated = core_realloc(current, (size_t)(act_size + n));
    dest->text = (char *)updated;
    if (updated == NULL) {
        return -1;
    }

    long for_dest_size = (long)dest->size;
    core_fread((unsigned char *)updated + for_dest_size, 1, (size_t)n, f);

    long for_total_size = (long)dest->size;
    dest->size = (uint64_t)(for_total_size + n);
    core_fseek(f, restore, 0);

    return 0;
}


extern int   str_vsprintf_caller_limit_2(char *dest, size_t size, const char *format, ...);
extern void  video_out_gl_shader_recipe_extract_value_20(const char *line, char *dest, uint64_t size) __asm__("video_out_gl_shader_recipe_extract_value");
extern void *files_fopen_resolved(const char *path, const char *mode);

int32_t video_out_gl_shader_recipe_read_include(gl_renderer_t *ctx, void *f, char *line,
                           const gl_recipe_tag_t *tag)
{

    static char   *(*p_fgets)(char *, int, void *);
    static char   *(*p_strstr)(const char *, const char *);
    static char   *(*p_strrchr_chk)(const char *, int, size_t);
    static int     (*p_fseek)(void *, long, int);
    static long    (*p_ftell)(void *);
    static void   *(*p_realloc)(void *, size_t);
    static size_t  (*p_fread)(void *, size_t, size_t, void *);
    static int     (*p_fclose)(void *);

    if (!p_fgets) {
        p_fgets       = (char *(*)(char *, int, void *))sym_libc_fgets;
        p_strstr      = (char *(*)(const char *, const char *))sym_libc_strstr;
        p_strrchr_chk = (char *(*)(const char *, int, size_t))fortify_strrchr;
        p_fseek       = (int (*)(void *, long, int))sym_libc_fseek;
        p_ftell       = (long (*)(void *))sym_libc_ftell;
        p_realloc     = (void *(*)(void *, size_t))sym_libc_realloc;
        p_fread       = (size_t (*)(void *, size_t, size_t, void *))sym_libc_fread;
        p_fclose      = (int (*)(void *))sym_libc_fclose;
    }

    if (p_fgets(line, 0x400, f) == NULL)
        return 0;

    char  path[0x400];
    char *end_path = path + 0x400;

    const char *dir     = ctx->base_dir;
    const char *s_file  = "file=";
    const char *s_fmt   = "%s/";
    const char *s_mode  = "rb";

    if (p_strstr(line, tag->close) != NULL)
        return 0;

    for (;;) {

        if (p_strstr(line, s_file) != NULL) {

            str_vsprintf_caller_limit_2(path, 0x400, s_fmt, dir);

            char *bar = p_strrchr_chk(path, '/', 0x400);

            if (bar != NULL) {

                char    *dest = bar + 1;
                uint64_t gap   = (uint64_t)(end_path - dest);
                video_out_gl_shader_recipe_extract_value(line, dest, gap);

                void *fp = nds_platform_default()->files.open(nds_platform_default()->user, path, s_mode);
                if (fp == NULL)
                    return -1;

                p_fseek(fp, 0, 2);
                long n = p_ftell(fp);
                p_fseek(fp, 0, 0);

                uint64_t used    = ctx->header.size;
                void    *prev = ctx->header.text;

                void *updated = p_realloc(prev, (size_t)(used + (uint64_t)n));

                ctx->header.text = (char *)updated;

                if (updated == NULL) {
                    p_fclose(fp);
                    return -1;
                }

                uint64_t used2 = ctx->header.size;
                p_fread((unsigned char *)updated + used2, 1, (size_t)n, fp);

                uint64_t used3 = ctx->header.size;
                ctx->header.size = used3 + (uint64_t)n;

                p_fclose(fp);
            }
        }

        if (p_fgets(line, 0x400, f) == NULL)
            return 0;

        if (p_strstr(line, tag->close) != NULL)
            return 0;
    }
}

extern void      video_out_gl_shader_recipe_extract_value_21(const char *line, char *dest, uint64_t size) __asm__("video_out_gl_shader_recipe_extract_value");
extern uint32_t  video_out_gl_token_lookup_21(const char *param_1) __asm__("video_out_gl_token_lookup");

int32_t video_out_gl_shader_recipe_read_texture(gl_renderer_t *r, void *stream, char *line,
                                                const gl_recipe_tag_t *tag)
{

    typedef char  *(*fn_strstr)(const char *, const char *);
    typedef char  *(*fn_strchr)(const char *, int);
    typedef int    (*fn_sscanf)(const char *, const char *, ...);
    typedef int    (*fn_memcmp)(const void *, const void *, unsigned long);
    typedef char  *(*fn_fgets)(char *, int, void *);
    typedef void  *(*fn_malloc)(unsigned long);
    typedef void   (*fn_free)(void *);
    typedef int    (*fn_fseek)(void *, long, int);
    typedef long   (*fn_ftell)(void *);
    typedef unsigned long (*fn_fread)(void *, unsigned long, unsigned long, void *);
    typedef int    (*fn_fclose)(void *);
    typedef void   (*fn_glGenTextures)(int, unsigned int *);
    typedef void   (*fn_glBindTexture)(unsigned int, unsigned int);
    typedef void   (*fn_glTexImage2D)(unsigned int, int, int, int, int, int,
                                       unsigned int, unsigned int, const void *);
    typedef void   (*fn_glTexParameteri)(unsigned int, unsigned int, int);
    typedef void   (*fn_glFinish)(void);

    static fn_strstr core_strstr;
    static fn_strchr core_strchr;
    static fn_sscanf core_sscanf;
    static fn_memcmp core_memcmp;
    static fn_fgets  core_fgets;
    static fn_malloc core_malloc;
    static fn_free   core_free;
    static fn_fseek  core_fseek;
    static fn_ftell  core_ftell;
    static fn_fread  core_fread;
    static fn_fclose core_fclose;
    static fn_glGenTextures   core_glGenTextures;
    static fn_glBindTexture   core_glBindTexture;
    static fn_glTexImage2D    core_glTexImage2D;
    static fn_glTexParameteri core_glTexParameteri;
    static fn_glFinish        core_glFinish;

    if (!core_strstr) core_strstr = (fn_strstr)sym_libc_strstr;
    if (!core_strchr) core_strchr = (fn_strchr)sym_libc_strchr;
    if (!core_sscanf) core_sscanf = (fn_sscanf)sym_libc_sscanf;
    if (!core_memcmp) core_memcmp = (fn_memcmp)sym_libc_memcmp;
    if (!core_fgets)  core_fgets  = (fn_fgets)sym_libc_fgets;
    if (!core_malloc) core_malloc = (fn_malloc)sym_libc_malloc;
    if (!core_free)   core_free   = (fn_free)sym_libc_free;
    if (!core_fseek)  core_fseek  = (fn_fseek)sym_libc_fseek;
    if (!core_ftell)  core_ftell  = (fn_ftell)sym_libc_ftell;
    if (!core_fread)  core_fread  = (fn_fread)sym_libc_fread;
    if (!core_fclose) core_fclose = (fn_fclose)sym_libc_fclose;
    if (!core_glGenTextures)   core_glGenTextures   = (fn_glGenTextures)sym_libc_glGenTextures;
    if (!core_glBindTexture)   core_glBindTexture   = (fn_glBindTexture)sym_libc_glBindTexture;
    if (!core_glTexImage2D)    core_glTexImage2D    = (fn_glTexImage2D)sym_libc_glTexImage2D;
    if (!core_glTexParameteri) core_glTexParameteri = (fn_glTexParameteri)sym_libc_glTexParameteri;
    if (!core_glFinish)        core_glFinish        = (fn_glFinish)sym_libc_glFinish;

    char *p;
    char *line_read;
    const char *sentinel;
    char valbuf[256];
    char *pathbuf = NULL;
    void *filebuf = NULL;
    void *filestream_close = NULL;
    gl_recipe_texture_t *E;

    uint32_t count = r->texture_count;
    if (count == 0) goto fail;

    gl_recipe_texture_t *arrbase_chk = r->textures;
    if (arrbase_chk == NULL) goto fail;

    p = core_strchr(line, ':');
    if (p == NULL) goto fail;

    {
        int32_t idx_parse = 0;
        int sret = core_sscanf(p, ":%d>", &idx_parse);
        if (sret != 1) goto fail;
        if (idx_parse < 0) goto fail;

        uint32_t count2 = r->texture_count;
        if ((uint32_t)idx_parse >= count2) goto fail;

        E = &r->textures[idx_parse];
    }

    E->id = 0;
    E->format = 0x00001908u;
    E->internal_format = 0x00001908u;
    E->type = 0x00001401u;
    E->mag_filter = 0x00002601u;
    E->min_filter = 0x00002601u;
    E->width = 0xffffffffu;
    E->height = 0xffffffffu;
    E->source = GL_RECIPE_TEXTURE_UNSET;

read_line:
    line_read = core_fgets(line, 0x400, stream);
    if (line_read == NULL) goto end_loop;

    sentinel = tag->close;
    p = core_strstr(line, sentinel);
    if (p != NULL) goto end_loop;

    p = core_strstr(line, "width=");
    if (p != NULL) goto is_width;

    p = core_strstr(line, "height=");
    if (p != NULL) goto is_height;

    video_out_gl_shader_recipe_extract_value(line, valbuf, 0x100);

    p = core_strstr(line, "input=");
    if (p != NULL) goto is_input;

    p = core_strstr(line, "internalformat=");
    if (p != NULL) goto is_internalformat;

    p = core_strstr(line, "format=");
    if (p != NULL) goto is_format;

    p = core_strstr(line, "type=");
    if (p != NULL) goto is_type;

    p = core_strstr(line, "min_filter=");
    if (p != NULL) goto is_min_filter;

    p = core_strstr(line, "mag_filter=");
    if (p != NULL) goto is_mag_filter;

    goto read_line;

is_width:
    p = core_strchr(line, '=');
    if (p == NULL) { E->width = 0xffffffffu; goto read_line; }
    {
        int32_t tmp = 0;
        int n_scanned = core_sscanf(p, "=%d", &tmp);
        uint32_t val = (n_scanned == 1) ? (uint32_t)tmp : 0xffffffffu;
        E->width = val;
    }
    goto read_line;

is_height:
    p = core_strchr(line, '=');
    if (p == NULL) { E->height = 0xffffffffu; goto read_line; }
    {
        int32_t tmp = 0;
        int n_scanned = core_sscanf(p, "=%d", &tmp);
        uint32_t val = (n_scanned == 1) ? (uint32_t)tmp : 0xffffffffu;
        E->height = val;
    }
    goto read_line;

is_input:
    if (core_memcmp(valbuf, "null", 5) == 0) {
        E->source = GL_RECIPE_TEXTURE_NULL;
        goto read_line;
    }
    if (core_memcmp(valbuf, "framebuffer", 12) == 0) {
        E->source = GL_RECIPE_TEXTURE_FRAMEBUFFER;
        goto read_line;
    }
    {
        char *path = (char *)core_malloc(0x400);
        pathbuf = path;
        if (path == NULL) goto end_loop;

        const char *basedir = r->base_dir;
        str_vsprintf_caller_limit_2(path, 0x400, "%s/%s", basedir, valbuf);

        void *fp = nds_platform_default()->files.open(nds_platform_default()->user, path, "rb");
        if (fp == NULL) {
            filestream_close = NULL;

            goto end_loop;
        }

        core_fseek(fp, 0, 2 );
        long size = core_ftell(fp);
        core_fseek(fp, 0, 0 );

        void *data = core_malloc((unsigned long)size);
        if (data == NULL) {

            core_fclose(fp);
            filebuf = NULL;
            goto after_close;
        }

        core_fread(data, 1, (unsigned long)size, fp);
        filebuf = data;
        filestream_close = fp;
        E->source = GL_RECIPE_TEXTURE_FILE;
        goto read_line;
    }

is_internalformat:
    E->internal_format = video_out_gl_token_lookup(valbuf);
    goto read_line;

is_format:
    E->format = video_out_gl_token_lookup(valbuf);
    goto read_line;

is_type:
    E->type = video_out_gl_token_lookup(valbuf);
    goto read_line;

is_min_filter:
    E->min_filter = video_out_gl_token_lookup(valbuf);
    goto read_line;

is_mag_filter:
    E->mag_filter = video_out_gl_token_lookup(valbuf);
    goto read_line;

end_loop:
    if (filebuf != NULL) {
        uint32_t kind = E->source;
        if (kind == GL_RECIPE_TEXTURE_FILE) {

            core_glGenTextures(1, &E->id);
            unsigned int texid = E->id;
            core_glBindTexture(0xde1u, texid);

            unsigned int internalformat = E->internal_format;
            unsigned int width          = E->width;
            unsigned int height         = E->height;
            unsigned int format         = E->format;
            unsigned int type           = E->type;
            core_glTexImage2D(0xde1u, 0, (int)internalformat, (int)width,
                               (int)height, 0, format, type, filebuf);

            unsigned int min_filter = E->min_filter;
            core_glTexParameteri(0xde1u, 0x2801u, (int)min_filter);
            unsigned int mag_filter = E->mag_filter;
            core_glTexParameteri(0xde1u, 0x2800u, (int)mag_filter);
            core_glTexParameteri(0xde1u, 0x2802u, 0x812f);
            core_glTexParameteri(0xde1u, 0x2803u, 0x812f);
            core_glFinish();
        }
    }
    if (filestream_close != NULL) core_fclose(filestream_close);

after_close:
    if (pathbuf != NULL) core_free(pathbuf);
    if (filebuf != NULL) core_free(filebuf);

    {
        uint32_t kind_final = E->source;
        return (kind_final == GL_RECIPE_TEXTURE_UNSET) ? -1 : 0;
    }

fail:
    return -1;
}

extern void video_out_gl_shader_recipe_extract_value_22(const char *line, char *dest, uint64_t size) __asm__("video_out_gl_shader_recipe_extract_value");
extern int  video_out_shader_load_and_report_errors(const char *param_1, void *param_2, void *param_3);

int32_t video_out_gl_shader_recipe_read_pass(gl_renderer_t *r, void *stream, char *buf,
                                             const gl_recipe_tag_t *tag)
{

    typedef char *(*fn_fgets)(char *, int, void *);
    typedef char *(*fn_strstr)(const char *, const char *);
    typedef char *(*fn_strchr)(const char *, int);
    typedef int   (*fn_sscanf)(const char *, const char *, ...);
    typedef void *(*fn_malloc)(unsigned long);
    typedef void  (*fn_gl_useprogram)(unsigned int);
    typedef int   (*fn_gl_getuniformlocation)(unsigned int, const char *);
    typedef void  (*fn_gl_uniform1i)(int, int);

    static fn_fgets  core_fgets;
    static fn_strstr core_strstr;
    static fn_strchr core_strchr;
    static fn_sscanf core_sscanf;
    static fn_malloc core_malloc;
    static fn_gl_useprogram         core_gl_useprogram;
    static fn_gl_getuniformlocation core_gl_getuniformlocation;
    static fn_gl_uniform1i          core_gl_uniform1i;
    if (!core_fgets)  core_fgets  = (fn_fgets)sym_libc_fgets;
    if (!core_strstr) core_strstr = (fn_strstr)sym_libc_strstr;
    if (!core_strchr) core_strchr = (fn_strchr)sym_libc_strchr;
    if (!core_sscanf) core_sscanf = (fn_sscanf)sym_libc_sscanf;
    if (!core_malloc) core_malloc = (fn_malloc)sym_libc_malloc;
    if (!core_gl_useprogram)
        core_gl_useprogram = (fn_gl_useprogram)sym_libc_glUseProgram;
    if (!core_gl_getuniformlocation)
        core_gl_getuniformlocation =
            (fn_gl_getuniformlocation)sym_libc_glGetUniformLocation;
    if (!core_gl_uniform1i)
        core_gl_uniform1i = (fn_gl_uniform1i)sym_libc_glUniform1i;

    gl_pass_t *node = (gl_pass_t *)core_malloc(sizeof *node);

    memset(node, 0, sizeof *node);
    node->output_scale = 1;

    gl_text_block_t *parts[3];
    parts[0] = &r->header;
    parts[1] = &r->vertex_header;
    parts[2] = &r->fragment_header;

    gl_pass_t *head = r->passes;
    if (head == NULL) {
        r->passes = node;
    } else {
        gl_pass_t *current = head;
        gl_pass_t *next = current->next;
        while (next != NULL) {
            current = next;
            next = current->next;
        }
        current->next = node;
    }

    char buf_shader[0x100];
    char buf_path[0x400];

    for (;;) {

        char *line = core_fgets(buf, 0x400, stream);
        if (line == NULL) return 0;

        const char *end = tag->close;
        if (core_strstr(buf, end) != NULL) return 0;

        if (core_strstr(buf, "shader=") != NULL) {
            video_out_gl_shader_recipe_extract_value(buf, buf_shader, 0x100);
            str_vsprintf_caller_limit_2(buf_path, 0x400, "%s/%s",
                                r->base_dir,
                                buf_shader);
            int loaded = video_out_shader_load_and_report_errors(buf_path, &parts[0], node);
            if (loaded != 0) return -1;
            if (node->program.time_uniform >= 0) {
                r->uses_time = 1;
            }
            continue;
        }

        if (core_strstr(buf, "sampler:") != NULL) {
            uint32_t entry_idx = node->sampler_count;
            gl_pass_sampler_t *eb = &node->sampler[entry_idx];

            eb->texture_unit = entry_idx + GL_TEXTURE0;

            char *eq = core_strchr(buf, '=');
            if (eq == NULL) {
                eb->texture_index = 0xffffffffu;
                return -1;
            }

            int local_574 = 0;
            int nsc = core_sscanf(eq, "=%d", &local_574);
            if (nsc != 1) {
                eb->texture_index = 0xffffffffu;
                return -1;
            }

            eb->texture_index = (uint32_t)local_574;

            if (local_574 < 0) return -1;
            int32_t limit = (int32_t)r->texture_count;
            if (local_574 >= limit) return -1;

            char *colon = core_strchr(buf, ':');
            if (colon == NULL) return -1;

            uint64_t len;
            unsigned char c_current = (unsigned char)colon[1];
            if (c_current == '=') {
                len = 0;
            } else {
                uint64_t idx = 0;
                uint64_t idx1;
                for (;;) {
                    idx1 = idx + 1;
                    eb->name[idx] = c_current;
                    if (idx1 > sizeof eb->name - 2) { len = idx1; break; }
                    c_current = (unsigned char)colon[idx + 2];
                    idx = idx1;
                    if (c_current == '=') { len = idx1; break; }
                }
            }
            eb->name[len] = 0;

            core_gl_useprogram(node->program.program);
            int loc = core_gl_getuniformlocation(node->program.program,
                                                  eb->name);
            core_gl_uniform1i(loc, (int32_t)entry_idx);
            node->sampler_count = entry_idx + 1;
            continue;
        }

        if (core_strstr(buf, "output=") != NULL) {
            char *eq2 = core_strchr(buf, '=');
            if (eq2 == NULL) return -1;
            int r2 = core_sscanf(eq2, "=%d:%d",
                                  (int *)&node->output_texture,
                                  (int *)&node->output_scale);
            if (r2 != 2) return -1;

        }

    }
}

const gl_recipe_tag_t gl_recipe_tags[7] = {
    { "<options>",  "</options>",  video_out_gl_shader_recipe_read_options,    0, { 0 } },
    { "<header>",   "</header>",   video_out_gl_shader_recipe_read_text_block, 1, { 0 } },
    { "<vheader>",  "</vheader>",  video_out_gl_shader_recipe_read_text_block, 2, { 0 } },
    { "<fheader>",  "</fheader>",  video_out_gl_shader_recipe_read_text_block, 3, { 0 } },
    { "<include>",  "</include>",  video_out_gl_shader_recipe_read_include,    4, { 0 } },
    { "<texture",   "</texture>",  video_out_gl_shader_recipe_read_texture,    5, { 0 } },
    { "<pass>",     "</pass>",     video_out_gl_shader_recipe_read_pass,       6, { 0 } },
};


typedef char *(*fn_strchr)(const char *, int);
typedef int   (*fn_sscanf)(const char *, const char *, ...);

int32_t video_out_gl_shader_recipe_parse_block_index(const char *s)
{

    const char *p = ((fn_strchr)sym_libc_strchr)(s, ':');
    if (p == NULL) {
        return -1;
    }

    int32_t value;
    int n = ((fn_sscanf)sym_libc_sscanf)(p, ":%d>", &value);

    if (n != 1) {
        value = -1;
    }
    return value;
}

typedef char *(*fn_strchr_t)(const char *, int);
typedef int   (*fn_sscanf_t)(const char *, const char *, ...);

int32_t video_out_gl_shader_recipe_parse_equals_int(const char *param_1)
{

    char *eq = ((fn_strchr_t)sym_libc_strchr)(param_1, '=');

    if (eq == 0) {

        return -1;
    }

    int32_t tmp;
    int matched = ((fn_sscanf_t)sym_libc_sscanf)(eq, "=%d", &tmp);

    if (matched != 1) {
        return -1;
    }
    return tmp;
}

typedef int (*fn_strcmp)(const char *, const char *);

uint32_t video_out_gl_token_lookup(const char *param_1)
{

    static fn_strcmp core_strcmp;
    if (!core_strcmp) core_strcmp = (fn_strcmp)sym_libc_strcmp;

    static const char *const literals[19] = {
        "GL_NEAREST",
        "GL_LINEAR",
        "GL_ALPHA",
        "GL_LUMINANCE",
        "GL_LUMINANCE_ALPHA",
        "GL_RGB",
        "GL_RGBA",
        "GL_DEPTH_COMPONENT",
        "GL_UNSIGNED_BYTE",
        "GL_UNSIGNED_SHORT_5_6_5",
        "GL_UNSIGNED_SHORT_4_4_4_4",
        "GL_UNSIGNED_SHORT_5_5_5_1",
        "GL_BYTE",
        "GL_SHORT",
        "GL_UNSIGNED_SHORT",
        "GL_INT",
        "GL_UNSIGNED_INT",
        "GL_FLOAT",
        "GL_FIXED",
    };

    int entry_idx = -1;
    for (int i = 0; i < 19; i++) {
        const char *lit = literals[i];
        if (core_strcmp(param_1, lit) == 0) {
            entry_idx = i;
            break;
        }
    }

    if (entry_idx < 0) {
        return (uint32_t)-1;
    }

    static const uint32_t values[19] = {
        0x2600,
        0x2601,
        0x1906,
        0x1909,
        0x190A,
        0x1907,
        0x1908,
        0x1902,
        0x1401,
        0x8363,
        0x8033,
        0x8034,
        0x1400,
        0x1402,
        0x1403,
        0x1404,
        0x1405,
        0x1406,
        0x140C,
    };

    return values[entry_idx];
}

void video_out_gl_shader_recipe_extract_value(const char *line, char *dest, uint64_t size) {

    const char *eq = strchr(line, '=');
    dest[0] = 0;
    if (!eq) return;

    const char *value = eq + 1;
    uint64_t len = strlen(value);

    while (len != 0) {
        unsigned char c = (unsigned char)eq[len];
        if (c != 0x0d && c != 0x0a) break;
        len--;
    }

    uint64_t gap = size - 1;
    if (len > gap) len = gap;

    memcpy(dest, value, len);
    dest[len] = 0;
}

extern void *recon_core_handle;
static void (*core_free)(void *);

static void free_core(void *p) {
    if (!core_free) core_free = (void (*)(void *))sym_libc_free;
    core_free(p);
}

void video_out_gl_release_pointer_pair(void **obj) {

    void *p = obj[0];
    if (p != NULL) {
        free_core(p);
    }
    obj[0] = NULL;
    obj[1] = NULL;
}


int video_out_gl_init_default_shader_program(void)
{

    const char *fragment_src = "#if GL_ES\nprecision mediump float;\n#endif\nvarying vec2 v_texture_coordinate;\nuniform sampler2D u_texture;\nvoid main() {\n  vec4 color = texture2D(u_texture, v_texture_coordinate);\n  gl_FragColor = vec4(color.rgb, 1.0);\n}\n";
    const char *vertex_src = "attribute vec2 a_vertex_coordinate;\nattribute vec2 a_texture_coordinate;\nvarying vec2 v_texture_coordinate;\nvoid main() {\n  gl_Position = vec4(a_vertex_coordinate.xy, 0.0, 1.0);\n  v_texture_coordinate = a_texture_coordinate;\n}\n";

    return video_out_gl_compile_shader_program(&vertex_src, &fragment_src,
                                            &VIDEO_OUT_GL_DEFAULT_PROGRAM->program, 0);
}

extern void     video_out_gl_renderer_release_29(void *obj, int32_t free_gl) __asm__("video_out_gl_renderer_release");
extern int      video_out_gl_compile_shader_program_29(const char *const *vertex_src,
                                    const char *const *fragment_src,
                                    void *program_info, char **out_error) __asm__("video_out_gl_compile_shader_program");

int32_t video_out_gl_load_shader_recipe(const char *path, gl_renderer_t *ctx, void *val3,
                            void *val4)
{

    static char  *(*p_strrchr)(const char *, int);
    static void  *(*p_memcpy)(void *, const void *, unsigned long);
    static void  *(*p_malloc)(unsigned long);
    static char  *(*p_fgets)(char *, int, void *);
    static char  *(*p_strstr)(const char *, const char *);
    static int    (*p_fclose)(void *);
    static void   (*p_free)(void *);

    if (!p_strrchr) {
        p_strrchr = (char *(*)(const char *, int))sym_libc_strrchr;
        p_memcpy  = (void *(*)(void *, const void *, unsigned long))sym_libc_memcpy;
        p_malloc  = (void *(*)(unsigned long))sym_libc_malloc;
        p_fgets   = (char *(*)(char *, int, void *))sym_libc_fgets;
        p_strstr  = (char *(*)(const char *, const char *))sym_libc_strstr;
        p_fclose  = (int (*)(void *))sym_libc_fclose;
        p_free    = (void (*)(void *))sym_libc_free;
    }

    void       *f;
    char       *slash;
    uint32_t    len;
    char       *buf;
    char       *r;
    char       *hit;
    int32_t     rv;
    const char *vertex_source, *fragment_source;

    if (ctx == 0)
        return -1;

    video_out_gl_renderer_release((void *)ctx, 1);
    VIDEO_OUT_GL_DEFAULT_PROGRAM->vertex_coordinates = val3;
    VIDEO_OUT_GL_DEFAULT_PROGRAM->texture_coordinates = val4;

    if (path == 0)
        goto L_7;

    f = nds_platform_default()->files.open(nds_platform_default()->user, path, "rb");
    ctx->vertex_coordinates = val3;
    ctx->texture_coordinates = val4;

    if (f == 0)
        goto L_7;

    slash = p_strrchr(path, '/');
    if (slash == 0)
        slash = p_strrchr(path, '\\');
    if (slash != 0) {
        len = (uint32_t)(slash - path);
        p_memcpy(ctx->base_dir, path, len);
    }

    buf = p_malloc(0x400);
    if (buf == 0)
        goto L_5;

    r = p_fgets(buf, 0x400, f);
    if (r == 0)
        goto L_6;

    hit = p_strstr(buf, "<options>");
    if (hit != 0)
        goto L_2;
    goto L_3;

L_1:
    r = p_fgets(buf, 0x400, f);
    if (r == 0)
        goto L_6;
    hit = p_strstr(buf, "<options>");
    if (hit != 0)
        goto L_2;
    goto L_3;

L_2:
    rv = video_out_gl_shader_recipe_read_options(ctx, f, buf,
                             &VIDEO_OUT_GL_RECIPE_TAGS[GL_RECIPE_TAG_OPTIONS]);
    if (rv != 0)
        goto L_4;

L_3:
    hit = p_strstr(buf, "<header>");
    if (hit != 0) {
        rv = video_out_gl_shader_recipe_read_text_block(ctx, f, buf, &VIDEO_OUT_GL_RECIPE_TAGS[GL_RECIPE_TAG_HEADER]);
        if (rv != 0) goto L_4;
    }
    hit = p_strstr(buf, "<vheader>");
    if (hit != 0) {
        rv = video_out_gl_shader_recipe_read_text_block(ctx, f, buf, &VIDEO_OUT_GL_RECIPE_TAGS[GL_RECIPE_TAG_VHEADER]);
        if (rv != 0) goto L_4;
    }
    hit = p_strstr(buf, "<fheader>");
    if (hit != 0) {
        rv = video_out_gl_shader_recipe_read_text_block(ctx, f, buf, &VIDEO_OUT_GL_RECIPE_TAGS[GL_RECIPE_TAG_FHEADER]);
        if (rv != 0) goto L_4;
    }
    hit = p_strstr(buf, "<include>");
    if (hit != 0) {
        rv = video_out_gl_shader_recipe_read_include(ctx, f, buf, &VIDEO_OUT_GL_RECIPE_TAGS[GL_RECIPE_TAG_INCLUDE]);
        if (rv != 0) goto L_4;
    }
    hit = p_strstr(buf, "<texture");
    if (hit != 0) {
        rv = video_out_gl_shader_recipe_read_texture(ctx, f, buf, &VIDEO_OUT_GL_RECIPE_TAGS[GL_RECIPE_TAG_TEXTURE]);
        if (rv != 0) goto L_4;
    }
    hit = p_strstr(buf, "<pass>");
    if (hit != 0) {
        rv = video_out_gl_shader_recipe_read_pass(ctx, f, buf, &VIDEO_OUT_GL_RECIPE_TAGS[GL_RECIPE_TAG_PASS]);
        if (rv != 0) goto L_4;
    }
    goto L_1;

L_4:
    video_out_gl_renderer_release((void *)ctx, 1);
L_5:
    p_fclose(f);
    goto L_7;

L_6:
    if (ctx->header.text != NULL)
        p_free(ctx->header.text);
    ctx->header.text = NULL;
    ctx->header.size = 0;
    if (ctx->vertex_header.text != NULL)
        p_free(ctx->vertex_header.text);
    ctx->vertex_header.text = NULL;
    ctx->vertex_header.size = 0;
    if (ctx->fragment_header.text != NULL)
        p_free(ctx->fragment_header.text);
    ctx->fragment_header.text = NULL;
    ctx->fragment_header.size = 0;
    p_free(buf);
    p_fclose(f);
    ctx->loaded = 1;
    return 0;

L_7:
    vertex_source = "attribute vec2 a_vertex_coordinate;\nattribute vec2 a_texture_coordinate;\nvarying vec2 v_texture_coordinate;\nvoid main() {\n  gl_Position = vec4(a_vertex_coordinate.xy, 0.0, 1.0);\n  v_texture_coordinate = a_texture_coordinate;\n}\n";
    fragment_source = "#if GL_ES\nprecision mediump float;\n#endif\nvarying vec2 v_texture_coordinate;\nuniform sampler2D u_texture;\nvoid main() {\n  vec4 color = texture2D(u_texture, v_texture_coordinate);\n  gl_FragColor = vec4(color.rgb, 1.0);\n}\n";
    (void)video_out_gl_compile_shader_program((const char *const *)&vertex_source,
                              (const char *const *)&fragment_source,
                              &VIDEO_OUT_GL_DEFAULT_PROGRAM->program, 0);
    return -1;
}

typedef void  (*fn_free_30)(void *);
typedef void *(*fn_memset_30)(void *, int, size_t);
typedef void  (*fn_prog)(unsigned);
typedef void  (*fn_n)(int, const void *);

void video_out_gl_renderer_release(void *param_1, int32_t param_2) {

    if (param_1 == NULL) return;

    gl_renderer_t *p1 = (gl_renderer_t *)param_1;

    gl_pass_t *list = p1->passes;
    p1->loaded = 0;

    fn_free_30     p_free     = (fn_free_30)sym_libc_free;
    fn_memset_30   p_memset   = (fn_memset_30)sym_libc_memset;
    fn_prog p_prog = (fn_prog)sym_libc_glDeleteProgram;
    fn_n    p_fb   = (fn_n)sym_libc_glDeleteFramebuffers;
    fn_n    p_tex  = (fn_n)sym_libc_glDeleteTextures;

    if (list != NULL) {
        if (param_2 == 0) {

            gl_pass_t *n = list;
            while (n != NULL) {
                gl_pass_t *sig = n->next;
                p_free(n);
                n = sig;
            }
        } else {
            gl_pass_t *n = list;
            for (;;) {
                uint32_t fb = n->framebuffer;
                if (fb != 0)
                    p_fb(1, &n->framebuffer);
                uint32_t program = n->program.program;
                p_prog(program);
                gl_pass_t *sig = n->next;
                p_free(n);
                if (sig == NULL) break;
                n = sig;
            }
        }
    }

    gl_recipe_texture_t *textures = p1->textures;
    if (textures != NULL) {
        if (param_2 != 0) {
            int32_t count = (int32_t)p1->texture_count;
            if (count > 0) {
                gl_recipe_texture_t *base = textures;
                int32_t idx = 0;
                for (;;) {
                    int32_t state = (int32_t)base[idx].source;
                    if (state != GL_RECIPE_TEXTURE_FRAMEBUFFER) {
                        p_tex(1, &base[idx].id);
                        count = (int32_t)p1->texture_count;
                    }
                    idx++;
                    if (idx >= count) break;
                    base = p1->textures;
                }
            }
            textures = p1->textures;
        }
        p_free(textures);
    }

    if (param_2 != 0) {
        int32_t program_global = (int32_t)VIDEO_OUT_GL_DEFAULT_PROGRAM->program.program;
        if (program_global != 0) {
            p_prog((unsigned)program_global);
            VIDEO_OUT_GL_DEFAULT_PROGRAM->program.program = 0;
        }
    }

    void *p;

    p = p1->header.text;
    if (p != NULL) p_free(p);
    p_memset(&p1->header, 0, sizeof p1->header);

    p = p1->vertex_header.text;
    if (p != NULL) p_free(p);
    p_memset(&p1->vertex_header, 0, sizeof p1->vertex_header);

    p = p1->fragment_header.text;
    if (p != NULL) p_free(p);
    p_memset(&p1->fragment_header, 0, sizeof p1->fragment_header);

    p_memset(param_1, 0, sizeof *p1);
}

#define GL_TEXTURE_2D          0x0de1u
#define GL_FRAMEBUFFER         0x8d40u
#define GL_COLOR_ATTACHMENT0   0x8ce0u
#define GL_TEXTURE_MAG_FILTER  0x2800u
#define GL_TEXTURE_MIN_FILTER  0x2801u
#define GL_TEXTURE_WRAP_S      0x2802u
#define GL_TEXTURE_WRAP_T      0x2803u
#define GL_CLAMP_TO_EDGE       0x812fu

typedef void     (*fn_n_31)(int32_t, void *);
typedef void     (*fn_gen_n)(int32_t, void *);
typedef void     (*fn_bind_tex)(uint32_t, uint32_t);
typedef void     (*fn_teximg)(uint32_t, int32_t, int32_t, int32_t, int32_t,
                              int32_t, uint32_t, uint32_t, const void *);
typedef void     (*fn_texparami)(uint32_t, uint32_t, int32_t);
typedef void     (*fn_bind_fb)(uint32_t, uint32_t);
typedef void     (*fn_fb_tex2d)(uint32_t, uint32_t, uint32_t, uint32_t, int32_t);
typedef uint32_t (*fn_check_fb)(uint32_t);
typedef void     (*fn_use_prog)(uint32_t);
typedef void     (*fn_uniform4f)(int32_t, float, float, float, float);

void video_out_gl_renderer_resize_nodes(void *param_1, uint32_t param_2, uint32_t param_3,
                         uint32_t param_4, uint32_t param_5,
                         uint32_t param_6, uint32_t param_7)
{

    gl_renderer_t *p1 = (gl_renderer_t *)param_1;
    gl_pass_t *head = p1->passes;

    p1->viewport_x = (int32_t)param_4;
    p1->viewport_y = (int32_t)param_5;
    p1->viewport_width = (int32_t)param_6;
    p1->viewport_height = (int32_t)param_7;

    if (head == NULL) return;

    int both = (param_2 != 0) && (param_3 != 0);
    float f_width   = (float)param_2;
    float f_height    = (float)param_3;
    float inv_width = 1.0f / f_width;
    float inv_height  = 1.0f / f_height;

    fn_n_31     p_fb    = (fn_n_31)sym_libc_glDeleteFramebuffers;
    fn_n_31     p_tex   = (fn_n_31)sym_libc_glDeleteTextures;
    fn_gen_n     p_gen_tex   = (fn_gen_n)sym_libc_glGenTextures;
    fn_bind_tex  p_bind_tex  = (fn_bind_tex)sym_libc_glBindTexture;
    fn_teximg    p_teximg    = (fn_teximg)sym_libc_glTexImage2D;
    fn_texparami p_texpar    = (fn_texparami)sym_libc_glTexParameteri;
    fn_gen_n     p_gen_fb    = (fn_gen_n)sym_libc_glGenFramebuffers;
    fn_bind_fb   p_bind_fb   = (fn_bind_fb)sym_libc_glBindFramebuffer;
    fn_fb_tex2d  p_fb_tex2d  = (fn_fb_tex2d)sym_libc_glFramebufferTexture2D;
    fn_check_fb  p_check_fb  = (fn_check_fb)sym_libc_glCheckFramebufferStatus;
    fn_use_prog  p_use_prog  = (fn_use_prog)sym_libc_glUseProgram;
    fn_uniform4f p_uniform4f = (fn_uniform4f)sym_libc_glUniform4f;

    gl_pass_t *node;
    gl_pass_t *sig;
    int32_t loc;

    node = head;

    if (both) goto process;

skip:
    sig = node->next;
    if (sig == NULL) goto final;
    node = sig;
    if (!both) goto skip;

process:
    sig = node->next;
    if (sig == NULL) goto after;

    {
        gl_recipe_texture_t *textures = p1->textures;
        uint32_t entry_idx = node->output_texture;
        uint32_t scale = node->output_scale;
        gl_recipe_texture_t *tex = &textures[entry_idx];

        uint32_t width_updated = scale * param_2;
        uint32_t height_updated  = scale * param_3;

        uint32_t width_old = tex->width;
        uint32_t height_old  = tex->height;

        node->width = width_updated;
        node->height = height_updated;

        uint32_t tex_id = tex->id;

        tex->width = width_updated;
        tex->height = height_updated;

        int regenerate;
        uint32_t fb_texture = 0;

        if (tex_id == 0) {
            regenerate = 1;
        } else {
            fb_texture = tex->framebuffer;
            if (fb_texture == 0 ||
                width_updated != width_old ||
                height_updated  != height_old)
                regenerate = 1;
            else
                regenerate = 0;
        }

        if (regenerate) {
            uint32_t fb_node = node->framebuffer;
            if (fb_node != 0)
                p_fb(1, &node->framebuffer);

            uint32_t state = tex->source;
            if (state == GL_RECIPE_TEXTURE_NULL) {
                uint32_t tex_id_now = tex->id;
                if (tex_id_now != 0)
                    p_tex(1, &tex->id);
            }

            tex->framebuffer = 0;
            p_gen_tex(1, &tex->id);
            uint32_t id_updated = tex->id;
            p_bind_tex(GL_TEXTURE_2D, id_updated);
            p_teximg(GL_TEXTURE_2D, 0, (int32_t)tex->internal_format,
                     (int32_t)width_updated, (int32_t)height_updated, 0,
                     tex->format, tex->type, NULL);
            p_texpar(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                     (int32_t)tex->min_filter);
            p_texpar(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                     (int32_t)tex->mag_filter);
            p_texpar(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, (int32_t)GL_CLAMP_TO_EDGE);
            p_texpar(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, (int32_t)GL_CLAMP_TO_EDGE);
            p_gen_fb(1, &node->framebuffer);
            uint32_t fb_updated = node->framebuffer;
            p_bind_fb(GL_FRAMEBUFFER, fb_updated);
            p_fb_tex2d(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                       tex->id, 0);
            (void)p_check_fb(GL_FRAMEBUFFER);
            uint32_t fb_final = node->framebuffer;
            tex->framebuffer = fb_final;
            p_bind_fb(GL_FRAMEBUFFER, 0);
        } else {
            node->framebuffer = fb_texture;
        }
    }

after:
    loc = node->program.texture_size_uniform;
    if (loc < 0) goto skip;

    {
        uint32_t program = node->program.program;
        p_use_prog(program);
        int32_t loc2 = node->program.texture_size_uniform;
        p_uniform4f(loc2, inv_width, inv_height, f_width, f_height);
        p_use_prog(0);
    }

    sig = node->next;
    if (sig == NULL) goto final;
    node = sig;
    if (both) goto process;
    goto skip;

final:
    node->width = param_6;
    node->height = param_7;
}
#undef GL_TEXTURE_2D
#undef GL_FRAMEBUFFER
#undef GL_COLOR_ATTACHMENT0
#undef GL_TEXTURE_MAG_FILTER
#undef GL_TEXTURE_MIN_FILTER
#undef GL_TEXTURE_WRAP_S
#undef GL_TEXTURE_WRAP_T
#undef GL_CLAMP_TO_EDGE

typedef void (*fn_program)(uint32_t program);
typedef void (*fn_framebuffer)(uint32_t target, uint32_t framebuffer);
typedef void (*fn_attr)(uint32_t entry_idx);
typedef void (*fn_pointer_attr)(uint32_t entry_idx, int32_t size,
                                    uint32_t type, uint8_t normalized,
                                    int32_t step, const void *pointer);
typedef void (*fn_texture)(uint32_t target, uint32_t texture);
typedef void (*fn_texture_parameter)(uint32_t target, uint32_t name,
                                     int32_t value);
typedef void (*fn_draw)(uint32_t mode, int32_t first, int32_t count);
typedef void (*fn_clear)(uint32_t mask);
typedef void (*fn_viewport)(int32_t x, int32_t y, int32_t width, int32_t height);
typedef void (*fn_uniform2f)(int32_t location, float x, float y);
typedef void (*fn_uniform1f)(int32_t location, float value);
typedef void (*fn_active_texture)(uint32_t texture);

void video_out_gl_renderer_draw_passes(void *param_1, uint32_t param_2, int32_t param_3,
                         int32_t param_4, uint32_t param_5, uint32_t param_6)
{

    gl_renderer_t *ctx = (gl_renderer_t *)param_1;

    if (ctx == 0)
        return;

    if (ctx->loaded == 0) {
        const gl_default_program_t *short_form = VIDEO_OUT_GL_DEFAULT_PROGRAM;
        fn_program program = (fn_program)sym_libc_glUseProgram;
        fn_framebuffer framebuffer = (fn_framebuffer)sym_libc_glBindFramebuffer;
        fn_attr attr = (fn_attr)sym_libc_glEnableVertexAttribArray;
        fn_pointer_attr pointer_attr =
            (fn_pointer_attr)sym_libc_glVertexAttribPointer;
        fn_texture texture = (fn_texture)sym_libc_glBindTexture;
        fn_draw draw = (fn_draw)sym_libc_glDrawArrays;

        program(short_form->program.program);
        framebuffer(0x8d40u, recon_present_fbo_dest);
        attr((uint32_t)short_form->program.vertex_coordinate_attrib);
        pointer_attr((uint32_t)short_form->program.vertex_coordinate_attrib, 2, 0x1406u, 0, 0,
                          short_form->vertex_coordinates);
        attr((uint32_t)short_form->program.texture_coordinate_attrib);
        pointer_attr((uint32_t)short_form->program.texture_coordinate_attrib, 2, 0x1406u, 0, 0,
                          short_form->texture_coordinates);
        texture(0x0de1u, param_2);
        draw(4u, param_3, 6);
        return;
    }

    gl_pass_t *node = ctx->passes;
    gl_recipe_texture_t *config = ctx->textures;
    float elapsed;
    fn_program program = (fn_program)sym_libc_glUseProgram;
    fn_framebuffer framebuffer = (fn_framebuffer)sym_libc_glBindFramebuffer;
    fn_attr attr = (fn_attr)sym_libc_glEnableVertexAttribArray;
    fn_pointer_attr pointer_attr =
        (fn_pointer_attr)sym_libc_glVertexAttribPointer;
    fn_texture texture = (fn_texture)sym_libc_glBindTexture;
    fn_texture_parameter param_texture =
        (fn_texture_parameter)sym_libc_glTexParameteri;
    fn_draw draw = (fn_draw)sym_libc_glDrawArrays;
    fn_clear clear = (fn_clear)sym_libc_glClear;
    fn_viewport viewport = (fn_viewport)sym_libc_glViewport;
    fn_uniform2f uniform2f = (fn_uniform2f)sym_libc_glUniform2f;
    fn_uniform1f uniform1f = (fn_uniform1f)sym_libc_glUniform1f;
    fn_active_texture texture_active =
        (fn_active_texture)sym_libc_glActiveTexture;

    if (ctx->uses_time == 0) {
        elapsed = 0.0f;
    } else {
        uint64_t now;
        time_now_microseconds(&now);
        uint64_t prev = ctx->start_time_us;
        if (prev == 0) {
            prev = now;
            ctx->start_time_us = now;
        }
        elapsed = (float)((double)(now - prev) / 1000000.0);
    }

    int32_t count_config = (int32_t)ctx->texture_count;
    if (count_config >= 1) {
        int64_t entry_idx = 0;
        gl_recipe_texture_t *entry = config;

        for (;;) {
            if (entry->source == GL_RECIPE_TEXTURE_FRAMEBUFFER) {
                entry->id = param_2;
                texture(0x0de1u, param_2);
                param_texture(0x0de1u, 0x2801u,
                                  (int32_t)entry->min_filter);
                param_texture(0x0de1u, 0x2800u,
                                  (int32_t)entry->mag_filter);
                count_config = (int32_t)ctx->texture_count;
                if (++entry_idx >= (int64_t)count_config)
                    break;
            } else if (++entry_idx >= (int64_t)count_config) {
                break;
            }
            entry++;
        }
    }

    if (node != 0) {
        int32_t width_prev = 0;
        int32_t height_prev = 0;
        float width_uniform = (float)param_5;
        float height_uniform = (float)param_6;

        do {
            program(node->program.program);
            {
                uint32_t fbo = node->framebuffer;
                framebuffer(0x8d40u, fbo != 0 ? fbo : recon_present_fbo_dest);
            }

            if (node->next != NULL) {
                clear(0x4000u);
                int32_t width = (int32_t)node->width;
                int32_t height = (int32_t)node->height;
                if (width != width_prev || height != height_prev) {
                    viewport(0, 0, width, height);
                    width_prev = width;
                    height_prev = height;
                }
            } else {
                viewport(ctx->viewport_x,
                         ctx->viewport_y,
                         ctx->viewport_width,
                         ctx->viewport_height);
            }

            attr((uint32_t)node->program.vertex_coordinate_attrib);
            pointer_attr((uint32_t)node->program.vertex_coordinate_attrib, 2, 0x1406u, 0, 0,
                              ctx->vertex_coordinates);
            attr((uint32_t)node->program.texture_coordinate_attrib);
            pointer_attr((uint32_t)node->program.texture_coordinate_attrib, 2, 0x1406u, 0, 0,
                              ctx->texture_coordinates);

            int32_t location2 = node->program.target_size_uniform;
            if (location2 >= 0)
                uniform2f(location2, width_uniform, height_uniform);
            int32_t location1 = node->program.time_uniform;
            if (location1 >= 0)
                uniform1f(location1, elapsed);

            uint32_t count_textures = node->sampler_count;
            if (count_textures != 0) {
                uint64_t index_texture = 0;
                gl_pass_sampler_t *slot = node->sampler;
                for (;;) {
                    texture_active(slot->texture_unit);
                    uint32_t index_table = slot->texture_index;
                    texture(0x0de1u, ctx->textures[index_table].id);
                    ++index_texture;
                    if (index_texture >= node->sampler_count)
                        break;
                    slot++;
                }
            }

            int32_t first = node->next == NULL ? param_3 : param_4;
            draw(4u, first, 6);
            node = node->next;
        } while (node != 0);
    }

    program(0);
    texture_active(0);
    texture(0x0de1u, 0);
}

video_out_gl_t video_out_gl_state;

gl_default_program_t gl_default_program_state;
