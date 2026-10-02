#ifdef __ANDROID__

#include <android/log.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/system_properties.h>
#include <unistd.h>
#include <ucontext.h>

#define ARENA_SIZE  (256 * 1024)
#define MAX_ENTRIES    16

static unsigned char cr_arena[ARENA_SIZE];
static char          cr_zip[512];
static char          cr_rom[512];
static uint32_t      cr_table[256];
static int           cr_installed;
static int           cr_fd_saf = -1;
static int           cr_fd_log = -1;

static char cr_rom_given[256];

void recon_crash_rom(const char *name);
void recon_crash_rom(const char *name)
{
    size_t i = 0;
    if (!name) { cr_rom_given[0] = 0; return; }
    while (name[i] && i + 1 < sizeof cr_rom_given) { cr_rom_given[i] = name[i]; i++; }
    cr_rom_given[i] = 0;
}

void recon_crash_fd(int fd);
void recon_crash_fd(int fd)
{
    if (cr_fd_saf >= 0 && cr_fd_saf != fd) close(cr_fd_saf);
    cr_fd_saf = fd;
    __android_log_print(4, "reconDS", "CRASH REPORT: SAF descriptor %d", fd);
}

void recon_crash_log_fd(int fd);
void recon_crash_log_fd(int fd)
{
    if (cr_fd_log >= 0 && cr_fd_log != fd) close(cr_fd_log);
    cr_fd_log = fd;
}

static size_t cr_len(const char *s) { size_t n = 0; while (s[n]) n++; return n; }

static void cr_put(char *d, size_t *o, size_t max, const char *s)
{
    while (*s && *o + 1 < max) d[(*o)++] = *s++;
    d[*o] = 0;
}

static void cr_put_u(char *d, size_t *o, size_t max, uint64_t v, int hex)
{
    char t[24];
    int n = 0;
    if (!v) t[n++] = 48;
    while (v) {
        unsigned dg = (unsigned)(v % (hex ? 16u : 10u));
        t[n++] = (char)(dg < 10 ? 48 + dg : 97 + dg - 10);
        v /= (hex ? 16u : 10u);
    }
    while (n && *o + 1 < max) d[(*o)++] = t[--n];
    d[*o] = 0;
}

static void cr_table_init(void)
{
    uint32_t i, j, c;
    for (i = 0; i < 256; i++) {
        c = i;
        for (j = 0; j < 8; j++) c = (c & 1) ? (0xedb88320u ^ (c >> 1)) : (c >> 1);
        cr_table[i] = c;
    }
}

static uint32_t cr_crc(uint32_t c, const unsigned char *p, size_t n)
{
    size_t i;
    for (i = 0; i < n; i++) c = cr_table[(c ^ p[i]) & 0xff] ^ (c >> 8);
    return c;
}

struct cr_ent { char name[64]; uint32_t crc, size, off; };
static struct cr_ent cr_ent[MAX_ENTRIES];
static int cr_nent;
static uint32_t cr_pos;

static void cr_w(int fd, const void *p, size_t n)
{
    const unsigned char *b = (const unsigned char *)p;
    size_t total = n;
    while (n) {
        ssize_t k = write(fd, b, n);
        if (k <= 0) break;
        b += k; n -= (size_t)k;
    }
    cr_pos += (uint32_t)total;
}

static void cr_w32(int fd, uint32_t v) { cr_w(fd, &v, 4); }
static void cr_w16(int fd, uint16_t v) { cr_w(fd, &v, 2); }

static void cr_entry_write(int fd, const char *name, const unsigned char *data,
                       uint32_t size)
{
    uint32_t crc;
    size_t ln;
    if (cr_nent >= MAX_ENTRIES) return;
    crc = cr_crc(0xffffffffu, data, size) ^ 0xffffffffu;
    ln = cr_len(name);
    if (ln > 60) return;
    cr_ent[cr_nent].crc = crc;
    cr_ent[cr_nent].size = size;
    cr_ent[cr_nent].off = cr_pos;
    memcpy(cr_ent[cr_nent].name, name, ln + 1);
    cr_nent++;
    cr_w32(fd, 0x04034b50u);
    cr_w16(fd, 20); cr_w16(fd, 0); cr_w16(fd, 0);
    cr_w16(fd, 0); cr_w16(fd, 0);
    cr_w32(fd, crc); cr_w32(fd, size); cr_w32(fd, size);
    cr_w16(fd, (uint16_t)ln); cr_w16(fd, 0);
    cr_w(fd, name, ln);
    cr_w(fd, data, size);
}

static int cr_file(int fd, const char *name, const char *path, uint32_t cap)
{
    int f = open(path, O_RDONLY);
    ssize_t n;
    if (f < 0) return 0;
    n = read(f, cr_arena, cap > ARENA_SIZE ? ARENA_SIZE : cap);
    close(f);
    if (n <= 0) return 0;
    cr_entry_write(fd, name, cr_arena, (uint32_t)n);
    return 1;
}

static int cr_log_tail(int fd)
{
    off_t end, start;
    ssize_t n;
    if (cr_fd_log < 0) return 0;
    end = lseek(cr_fd_log, 0, SEEK_END);
    if (end <= 0) return 0;
    start = end > (off_t)ARENA_SIZE ? end - (off_t)ARENA_SIZE : 0;
    if (lseek(cr_fd_log, start, SEEK_SET) < 0) return 0;
    n = read(cr_fd_log, cr_arena, (size_t)(end - start));
    if (n <= 0) return 0;
    cr_entry_write(fd, "SeedlessDS.log", cr_arena, (uint32_t)n);
    return 1;
}

static void cr_close(int fd)
{
    uint32_t start = cr_pos, cd_size;
    int i;
    for (i = 0; i < cr_nent; i++) {
        size_t ln = cr_len(cr_ent[i].name);
        cr_w32(fd, 0x02014b50u);
        cr_w16(fd, 20); cr_w16(fd, 20); cr_w16(fd, 0); cr_w16(fd, 0);
        cr_w16(fd, 0); cr_w16(fd, 0);
        cr_w32(fd, cr_ent[i].crc); cr_w32(fd, cr_ent[i].size); cr_w32(fd, cr_ent[i].size);
        cr_w16(fd, (uint16_t)ln); cr_w16(fd, 0); cr_w16(fd, 0);
        cr_w16(fd, 0); cr_w16(fd, 0); cr_w32(fd, 0);
        cr_w32(fd, cr_ent[i].off);
        cr_w(fd, cr_ent[i].name, ln);
    }

    cd_size = cr_pos - start;
    cr_w32(fd, 0x06054b50u);
    cr_w16(fd, 0); cr_w16(fd, 0);
    cr_w16(fd, (uint16_t)cr_nent); cr_w16(fd, (uint16_t)cr_nent);
    cr_w32(fd, cd_size); cr_w32(fd, start); cr_w16(fd, 0);
}

static int cr_find_rom(char *out, size_t max)
{
    int i;
    char path[64];
    for (i = 0; i < 512; i++) {
        size_t o = 0;
        ssize_t n;
        cr_put(path, &o, sizeof path, "/proc/self/fd/");
        cr_put_u(path, &o, sizeof path, (uint64_t)i, 0);
        n = readlink(path, out, max - 1);
        if (n <= 4) continue;
        out[n] = 0;
        if (out[n - 4] == 46 && (out[n - 3] | 32) == 110 &&
            (out[n - 2] | 32) == 100 && (out[n - 1] | 32) == 115)
            return 1;
    }
    out[0] = 0;
    return 0;
}

static void cr_package(char *out, size_t max)
{
    int f = open("/proc/self/cmdline", O_RDONLY);
    ssize_t n;
    size_t i;
    out[0] = 0;
    if (f < 0) return;
    n = read(f, out, max - 1);
    close(f);
    if (n <= 0) { out[0] = 0; return; }
    out[n] = 0;
    for (i = 0; i < (size_t)n; i++) if (out[i] == 0) break;
    out[i] = 0;
}

unsigned long nds_module_base(void);
unsigned long nds_output_frame_index(void);
unsigned long nds_output_read_count(void);

static void cr_handle(int sig, siginfo_t *si, void *uc)
{
    ucontext_t *u = (ucontext_t *)uc;
    static char txt[8192];
    static char aux[512];
    size_t o = 0, ao;
    int fd, i, has_rom, sav_ok, log_ok, in_folder_rom = 1;

    if (cr_installed != 1) _exit(128 + sig);
    cr_installed = 2;

    {
        unsigned long long pc = (unsigned long long)u->uc_mcontext.pc;
        unsigned long long lr = (unsigned long long)u->uc_mcontext.regs[30];
        unsigned long long sp = (unsigned long long)u->uc_mcontext.sp;
        __android_log_print(6, "reconDS",
            "CRASH signal %d  pc=0x%llx  lr=0x%llx  sp=0x%llx  addr=%p",
            sig, pc, lr, sp, si ? si->si_addr : 0);
    }

    __android_log_print(6, "reconDS", "CRASH: signal %d, writing report", sig);

    has_rom = cr_find_rom(cr_rom, sizeof cr_rom);
    if (!has_rom && cr_rom_given[0]) {
        size_t k = 0;
        while (cr_rom_given[k] && k + 1 < sizeof cr_rom) { cr_rom[k] = cr_rom_given[k]; k++; }
        cr_rom[k] = 0;
        has_rom = 2;
    }

    cr_package(aux, sizeof aux);
    fd = -1;
    if (cr_fd_saf >= 0) {
        fd = cr_fd_saf;
        ao = 0;
        cr_put(cr_zip, &ao, sizeof cr_zip, "(ROM folder, via SAF)");
        lseek(fd, 0, SEEK_SET);
        ftruncate(fd, 0);
    }
    if (fd < 0 && has_rom == 1) {
        size_t l = cr_len(cr_rom);
        memcpy(cr_zip, cr_rom, l - 4);
        cr_zip[l - 4] = 0;
        ao = l - 4;
        cr_put(cr_zip, &ao, sizeof cr_zip, "-crash.zip");
        fd = open(cr_zip, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    }
    if (fd < 0 && aux[0]) {
        in_folder_rom = 0;
        ao = 0;
        cr_put(cr_zip, &ao, sizeof cr_zip, "/sdcard/Android/data/");
        cr_put(cr_zip, &ao, sizeof cr_zip, aux);
        cr_put(cr_zip, &ao, sizeof cr_zip, "/files/crash.zip");
        fd = open(cr_zip, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    }
    if (fd < 0 && aux[0]) {
        in_folder_rom = 0;
        ao = 0;
        cr_put(cr_zip, &ao, sizeof cr_zip, "/data/data/");
        cr_put(cr_zip, &ao, sizeof cr_zip, aux);
        cr_put(cr_zip, &ao, sizeof cr_zip, "/files/crash.zip");
        fd = open(cr_zip, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    }
    if (fd < 0) {
        __android_log_print(6, "reconDS",
                            "CRASH: could not write report to any location"
                            " (last attempt: %s)", cr_zip);
        _exit(128 + sig);
    }
    __android_log_print(4, "reconDS", "CRASH: report at %s", cr_zip);
    cr_pos = 0; cr_nent = 0;

    cr_put(txt, &o, sizeof txt, "SeedlessDS crash report\n\nsignal: ");
    cr_put_u(txt, &o, sizeof txt, (uint64_t)sig, 0);
    cr_put(txt, &o, sizeof txt, "  code: ");
    cr_put_u(txt, &o, sizeof txt, (uint64_t)si->si_code, 0);
    cr_put(txt, &o, sizeof txt, "  address: 0x");
    cr_put_u(txt, &o, sizeof txt, (uint64_t)(uintptr_t)si->si_addr, 1);
    cr_put(txt, &o, sizeof txt, "\nframe: ");
    cr_put_u(txt, &o, sizeof txt, nds_output_frame_index(), 0);
    cr_put(txt, &o, sizeof txt, " (flip counter: ");
    cr_put_u(txt, &o, sizeof txt, nds_output_read_count(), 0);
    cr_put(txt, &o, sizeof txt, ")");
    cr_put(txt, &o, sizeof txt, "\nrom: ");
    cr_put(txt, &o, sizeof txt, has_rom ? cr_rom : "unknown");
    {
        unsigned long base = nds_module_base();
        cr_put(txt, &o, sizeof txt, "\n\nbase: 0x");
        cr_put_u(txt, &o, sizeof txt, (uint64_t)base, 1);
        cr_put(txt, &o, sizeof txt,
               "\n\npc:   0x");
    }
    cr_put_u(txt, &o, sizeof txt, (uint64_t)u->uc_mcontext.pc, 1);

    {
        unsigned long base = nds_module_base();
        unsigned long rel = (unsigned long)u->uc_mcontext.pc - base;
        if (base && (unsigned long)u->uc_mcontext.pc > base && rel < (64UL << 20)) {
            cr_put(txt, &o, sizeof txt, "   (librecon_fn.so + 0x");
            cr_put_u(txt, &o, sizeof txt, (uint64_t)rel, 1);
            cr_put(txt, &o, sizeof txt, ")");
        } else {
            cr_put(txt, &o, sizeof txt,
                   "   (outside librecon_fn.so)");
        }
    }
    cr_put(txt, &o, sizeof txt, "\nsp:   0x");
    cr_put_u(txt, &o, sizeof txt, (uint64_t)u->uc_mcontext.sp, 1);
    cr_put(txt, &o, sizeof txt, "\nlr:   0x");
    cr_put_u(txt, &o, sizeof txt, (uint64_t)u->uc_mcontext.regs[30], 1);
    cr_put(txt, &o, sizeof txt, "\n\nregisters:\n");
    for (i = 0; i < 31; i++) {
        cr_put(txt, &o, sizeof txt, "  x");
        cr_put_u(txt, &o, sizeof txt, (uint64_t)i, 0);
        cr_put(txt, &o, sizeof txt, " = 0x");
        cr_put_u(txt, &o, sizeof txt, (uint64_t)u->uc_mcontext.regs[i], 1);
        cr_put(txt, &o, sizeof txt, (i % 2) ? "\n" : "    ");
    }

    cr_entry_write(fd, "REPORT.txt", (const unsigned char *)txt, (uint32_t)o);

    o = 0;
    cr_put(txt, &o, sizeof txt, "model: ");
    aux[0] = 0; __system_property_get("ro.product.model", aux);
    cr_put(txt, &o, sizeof txt, aux);
    cr_put(txt, &o, sizeof txt, "\nmanufacturer: ");
    aux[0] = 0; __system_property_get("ro.product.manufacturer", aux);
    cr_put(txt, &o, sizeof txt, aux);
    cr_put(txt, &o, sizeof txt, "\nandroid: ");
    aux[0] = 0; __system_property_get("ro.build.version.release", aux);
    cr_put(txt, &o, sizeof txt, aux);
    cr_put(txt, &o, sizeof txt, "\nabi: ");
    aux[0] = 0; __system_property_get("ro.product.cpu.abi", aux);
    cr_put(txt, &o, sizeof txt, aux);
    cr_put(txt, &o, sizeof txt, "\n");
    cr_entry_write(fd, "environment.txt", (const unsigned char *)txt, (uint32_t)o);

    log_ok = cr_log_tail(fd);
    sav_ok = 0;
    if (has_rom == 1) {
        size_t l = cr_len(cr_rom);
        memcpy(aux, cr_rom, l - 4); aux[l - 4] = 0; ao = l - 4;
        cr_put(aux, &ao, sizeof aux, ".sav");
        sav_ok = cr_file(fd, "save.sav", aux, ARENA_SIZE);
    }

    o = 0;
    cr_put(txt, &o, sizeof txt, log_ok ? "SeedlessDS.log: yes\n" : "SeedlessDS.log: no\n");
    cr_put(txt, &o, sizeof txt, sav_ok ? "save.sav: yes\n" : "save.sav: no\n");
    if (!in_folder_rom) {
        cr_put(txt, &o, sizeof txt, "report path: ");
        cr_put(txt, &o, sizeof txt, cr_zip);
        cr_put(txt, &o, sizeof txt, "\n");
    }
    if (!has_rom)
        cr_put(txt, &o, sizeof txt, "rom: unknown\n");
    cr_entry_write(fd, "contents.txt", (const unsigned char *)txt, (uint32_t)o);

    cr_close(fd);
    close(fd);
    _exit(128 + sig);
}

__attribute__((constructor)) static void cr_auto(void);
void recon_crash_install(void);

__attribute__((constructor)) static void cr_auto(void) { recon_crash_install(); }

void recon_crash_stack_thread(void);
void recon_crash_stack_thread(void)
{

    stack_t already;
    void *p;
    if (sigaltstack(0, &already) == 0 && already.ss_sp && !(already.ss_flags & SS_DISABLE))
        return;
    p = mmap(0, (size_t)SIGSTKSZ * 4, PROT_READ | PROT_WRITE,
             MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p != MAP_FAILED) {
        stack_t ss;
        ss.ss_sp = p;
        ss.ss_size = (size_t)SIGSTKSZ * 4;
        ss.ss_flags = 0;
        sigaltstack(&ss, 0);
    }
}

void recon_crash_install(void)
{
    static char stack[SIGSTKSZ * 2];
    stack_t ss;
    struct sigaction sa;
    int i;
    static const int signals[] = { SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT };

    if (cr_installed) return;

    cr_table_init();

    ss.ss_sp = stack;
    ss.ss_size = sizeof stack;
    ss.ss_flags = 0;
    sigaltstack(&ss, 0);

    memset(&sa, 0, sizeof sa);
    sa.sa_sigaction = cr_handle;
    sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
    sigemptyset(&sa.sa_mask);
    for (i = 0; i < (int)(sizeof signals / sizeof signals[0]); i++)
        sigaction(signals[i], &sa, 0);
    cr_installed = 1;

    __android_log_print(4, "reconDS", "CRASH REPORT installed (%d signals)",
                        (int)(sizeof signals / sizeof signals[0]));
}

#endif
