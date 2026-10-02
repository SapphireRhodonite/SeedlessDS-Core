#include <stdint.h>
#include <string.h>
#include "hires_runtime.h"
#include "blob_symbols.h"
#include <stddef.h>
#include "seedlessds/platform.h"
#include "core_internals.h"

typedef char *(*fn_fgets)(char *, int, void *);
typedef char *(*fn_strstr)(const char *, const char *);

int video_out_shader_parse_source_blocks(void *fp, void *param_2_no_used,
                        void **out_vertex, void **out_fragment,
                        void **parts)
{
    (void)param_2_no_used;

    char line[1024];
    char *p;
    void *r;
    int had_vertex = 0;
    int had_fragment = 0;

    void *common    = parts[0];
    void *hdr_vert = parts[1];
    void *hdr_frag = parts[2];

    char *l = ((fn_fgets)sym_libc_fgets)(line, 0x400, fp);
    if (l == 0) {
        had_vertex = 0;
        had_fragment = 0;
        goto end;
    }

    p = ((fn_strstr)sym_libc_strstr)(line, "<vertex>");
    if (p == 0) goto check_fragment;

process_vertex:
    r = video_out_gl_shader_recipe_read_block(fp, "</vertex>", common, hdr_vert);
    *out_vertex = r;
    if (r != 0) had_vertex = 1;

loop_lines:
    l = ((fn_fgets)sym_libc_fgets)(line, 0x400, fp);
    if (l == 0) goto end;
    p = ((fn_strstr)sym_libc_strstr)(line, "<vertex>");
    if (p != 0) goto process_vertex;

check_fragment:
    p = ((fn_strstr)sym_libc_strstr)(line, "<fragment>");
    if (p != 0) {
        r = video_out_gl_shader_recipe_read_block(fp, "</fragment>", common, hdr_frag);
        if (r != 0) had_fragment = 1;
        *out_fragment = r;
    }
    goto loop_lines;

end:
    return -(int)(!had_vertex || !had_fragment);
}

extern void *files_fopen_resolved(const char *path, const char *mode);
extern int video_out_shader_parse_source_blocks_1(void *fp, void *param_2_no_used,
                               void **out_vertex, void **out_fragment,
                               void **parts) __asm__("video_out_shader_parse_source_blocks");
extern int video_out_gl_format_log_path(char *param_1, long param_2, long param_3,
                               long param_4, ...);

int video_out_shader_load_and_report_errors(const char *param_1, void *param_2, void *param_3) {

    static int (*core_fclose)(void *);
    static void (*core_free)(void *);
    static int (*core_fputs)(const char *, void *);
    if (!core_fclose) core_fclose = (int (*)(void *))sym_libc_fclose;
    if (!core_free)   core_free   = (void (*)(void *))sym_libc_free;
    if (!core_fputs)  core_fputs  = (int (*)(const char *, void *))sym_libc_fputs;

    void *local_450 = 0;
    void *local_458 = 0;
    char *local_460 = 0;
    char buf_path[1024];

    void *pFVar3 = nds_platform_default()->files.open(nds_platform_default()->user, param_1, "rb");

    if (pFVar3 != 0) {

        int iVar2 = video_out_shader_parse_source_blocks(pFVar3, 0,
                                        &local_450, &local_458, param_2);
        core_fclose(pFVar3);

        if (iVar2 == 0) {
            if (local_450 != 0 && local_458 != 0) {
                iVar2 = video_out_gl_compile_shader_program((const char *const *)(&local_450), (const char *const *)(&local_458),
                                                  param_3, &local_460);
                if (iVar2 == -1 && local_460 != 0) {

                    video_out_gl_format_log_path(buf_path, 0, 0, 0, param_1);

                    void *pFVar3b = nds_platform_default()->files.open(nds_platform_default()->user, 
                        buf_path, "wb");
                    if (pFVar3b != 0) {
                        core_fputs(local_460, pFVar3b);
                        core_fclose(pFVar3b);
                    }
                    core_free(local_460);
                }
            }
            if (local_450 != 0) core_free(local_450);
            if (local_458 != 0) core_free(local_458);
            return iVar2;
        }
    }
    return -1;
}
