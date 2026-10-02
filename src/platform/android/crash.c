#include <stdint.h>
#include <string.h>
#include "hires_runtime.h"
#include "blob_symbols.h"
#include <stddef.h>
#include "core/nds_state.h"
#include "frontend/frontend.h"
#include "seedlessds/platform.h"
#include "core_internals.h"
#include "mem_access.h"

#define OFF_GLOB   0x14c000UL





void platform_config_record_reset(void)
{

    nds_config_t *reg = &FRONTEND->machine->config;

    reg->run_limit_enabled = 0;
    reg->run_limit_ms = 0;

    uint64_t clock = FRONTEND->run_limit_us;

    if (clock != 0xffffffffffffffffULL) {

        uint64_t value = clock / 1000u;
        reg->run_limit_enabled = 1;
        reg->run_limit_ms = value;
    }
}
#undef OFF_GLOB

typedef int (*func_fprintf)(void *, const char *, ...);
typedef size_t (*func_fwrite)(const void *, size_t, size_t, void *);
typedef int (*func_fputc)(int, void *);
typedef int (*func_fclose)(void *);




void crash_write_dump_report(void *param_1)
{

    static func_fprintf core_fprintf;
    static func_fwrite core_fwrite;
    static func_fputc core_fputc;
    static func_fclose core_fclose;
    unsigned char path[0x400];
    frontend_t *global = FRONTEND;
    unsigned char *ctx;
    unsigned char *ctx_path;
    void *file;

    if (core_fprintf == 0) {
        core_fprintf = (func_fprintf)sym_libc_fprintf;
        core_fwrite = (func_fwrite)sym_libc_fwrite;
        core_fputc = (func_fputc)sym_libc_fputc;
        core_fclose = (func_fclose)sym_libc_fclose;
    }

    ctx_path = (unsigned char *)global->machine;
    str_vsprintf_caller_limit_1((char *)path, sizeof(path),
                                  "%s%ccrash_dump.txt", ((nds_t *)ctx_path)->cache_dir);
    file = nds_platform_default()->files.open(nds_platform_default()->user, (const char *)path,
                                            "wb");
    if (file == 0)
        return;

    ctx = (unsigned char *)global->machine;
    core_fprintf(file, "Unhandled SIG_SEGV in '%s'\n caused by instruction %s (at %x), address %x\n", ((nds_t *)ctx)->rom_name, param_1,
                 rd32((unsigned char *)param_1 + 0x144),
                 rd32((unsigned char *)param_1 + 0x148));
    core_fprintf(file, "Version: %s build %d\n", "r2.6.0.4a",
                 global->script_overrides_high);
    core_fprintf(file, "Reference: %016lX\n", crash_write_dump_report);
    core_fwrite("Registers:\n", 11, 1, file);

    for (uint32_t i = 0; i != 15; ++i)
        core_fprintf(file, " r%02d: %08X\n", i,
                     rd32((unsigned char *)param_1 + 0x108 + i * 4));
    core_fputc(10, file);

    core_fwrite("Emulated ARM9:\n", 15, 1, file);
    core_fprintf(file, " Mode %02d, IRQ %08x, CPSR %08x, PC %08x, cycles %08d\n",
                 ((nds_t *)ctx)->arm9.bank, ((nds_t *)ctx)->arm9.irq_pending,
                 ((nds_t *)ctx)->arm9.cpsr, ((nds_t *)ctx)->arm9.pc,
                 ((nds_t *)ctx)->arm9.cycle_mark);

    for (uint32_t i = 0; i != 15; ++i)
        core_fprintf(file, " r%d: %08x\n", i,
                     ((nds_t *)ctx)->arm9.r[i]);
    core_fprintf(file, " Debug instruction count: %lx\n\n", ((nds_t *)ctx)->arm9.debug.instruction_count);

    core_fwrite("Emulated ARM7:\n", 15, 1, file);
    {
        jit_arena_t *arena = (jit_arena_t *)rd_ptr((unsigned char *)param_1 + 0x100);
        jit_region_t *region = arena->region;
        long used;
        long left;

        core_fprintf(file, " main: %p - %p\n", arena->code_main_ram,
                     arena->code_main_ram + sizeof arena->code_main_ram);
        used = region[JIT_REGION_MAIN_RAM].code - arena->code_main_ram;
        left = arena->code_main_ram + sizeof arena->code_main_ram - region[JIT_REGION_MAIN_RAM].table;
        core_fprintf(file, " main: %ld + %ld bytes\n", used, left);

        core_fprintf(file, " itcm: %p - %p\n", arena->code_itcm,
                     arena->code_itcm + sizeof arena->code_itcm);
        used = region[JIT_REGION_ITCM].code - arena->code_itcm;
        left = arena->code_itcm + sizeof arena->code_itcm - region[JIT_REGION_ITCM].table;
        core_fprintf(file, " itcm: %ld + %ld bytes\n", used, left);

        core_fprintf(file, " alternate: %p - %p\n", arena->code_other,
                     arena->code_other + sizeof arena->code_other);
        used = region[JIT_REGION_OTHER].code - arena->code_other;
        left = arena->code_other + sizeof arena->code_other - region[JIT_REGION_OTHER].table;
        core_fprintf(file, " alternate: %ld + %ld bytes\n\n", used, left);
    }

    ctx = (unsigned char *)global->machine;
    core_fprintf(file, "%d texture cache bytes allocated, %d texture cache elements.\n",
                 ((nds_t *)ctx)->gpu.texture_cache.occupied, ((nds_t *)ctx)->gpu.texture_cache.count);
    core_fclose(file);
}
