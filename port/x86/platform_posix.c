#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include "seedlessds/platform.h"

static void *f_open(void *user, const char *path, const char *mode)
{
    (void)user;
    return fopen(path, mode);
}

static int f_open_fd(void *user, const char *path, int flags)
{
    (void)user;
    int b = O_RDONLY;
    if (flags & NDS_OPEN_READ_WRITE) b = O_RDWR;
    else if (flags & NDS_OPEN_WRITE) b = O_WRONLY | O_CREAT | O_TRUNC;
    else if (flags & NDS_OPEN_APPEND) b = O_WRONLY | O_CREAT | O_APPEND;
    return open(path, b, 0644);
}

static int f_stat(void *user, const char *path, void *out)
{
    (void)user;
    return stat(path, (struct stat *)out);
}

static int f_remove(void *user, const char *path)
{
    (void)user;
    return remove(path);
}

static int f_resolve(void *user, const char *name, char *path, size_t size)
{
    (void)user;
    snprintf(path, size, "%s", name ? name : "");
    return 0;
}

static int h_create(void *user, void *thread, nds_thread_fn fn, void *arg)
{
    (void)user;
    return pthread_create((pthread_t *)thread, NULL, fn, arg);
}

static int h_join(void *user, void *thread)
{
    (void)user;
    return pthread_join(*(pthread_t *)thread, NULL);
}

static int m_init(void *m)      { return pthread_mutex_init((pthread_mutex_t *)m, NULL); }
static int m_destroy(void *m)   { return pthread_mutex_destroy((pthread_mutex_t *)m); }
static int m_lock(void *m)      { return pthread_mutex_lock((pthread_mutex_t *)m); }
static int m_unlock(void *m)    { return pthread_mutex_unlock((pthread_mutex_t *)m); }
static int c_init(void *c)      { return pthread_cond_init((pthread_cond_t *)c, NULL); }
static int c_destroy(void *c)   { return pthread_cond_destroy((pthread_cond_t *)c); }
static int c_wait(void *c, void *m) { return pthread_cond_wait((pthread_cond_t *)c, (pthread_mutex_t *)m); }
static int c_signal(void *c)    { return pthread_cond_signal((pthread_cond_t *)c); }
static int c_broadcast(void *c) { return pthread_cond_broadcast((pthread_cond_t *)c); }

static void h_sleep(uint64_t us)
{
    struct timespec t;
    t.tv_sec  = (time_t)(us / 1000000ull);
    t.tv_nsec = (long)((us % 1000000ull) * 1000ull);
    nanosleep(&t, NULL);
}

static unsigned h_cpus(void *user)
{
    (void)user;
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    return n > 0 ? (unsigned)n : 1u;
}

static uint64_t t_now_us(void *user)
{
    (void)user;
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint64_t)t.tv_sec * 1000000ull + (uint64_t)(t.tv_nsec / 1000);
}

static int64_t t_now_seconds(void *user)
{
    (void)user;
    return (int64_t)time(NULL);
}

static void t_wall(void *user, nds_wall_clock_t *out)
{
    (void)user;
    time_t now = time(NULL);
    struct tm d;
    localtime_r(&now, &d);
    out->year = d.tm_year + 1900;
    out->month = d.tm_mon + 1;
    out->day = d.tm_mday;
    out->weekday = d.tm_wday;
    out->hour = d.tm_hour;
    out->minute = d.tm_min;
    out->second = d.tm_sec;
}

static int a_open(void *user, unsigned rate, unsigned channels, nds_audio_fill_fn fill, void *fill_user)
{
    (void)user; (void)rate; (void)channels; (void)fill; (void)fill_user;
    return 0;
}
static void a_close(void *user) { (void)user; }
static int a_mic_open(void *user, unsigned rate) { (void)user; (void)rate; return 0; }
static int64_t a_mic_read(void *user, void *samples, int64_t frames)
{
    (void)user;
    if (samples && frames > 0) memset(samples, 0, (size_t)frames * 2u);
    return frames;
}
static void a_mic_close(void *user) { (void)user; }

static void *x_alloc(void *user, size_t size)
{
    (void)user;
    void *p = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return p == MAP_FAILED ? NULL : p;
}

static int x_protect(void *user, void *address, size_t size, int mode)
{
    (void)user;
    int p = (mode == NDS_EXEC_READ_EXEC) ? (PROT_READ | PROT_EXEC) : (PROT_READ | PROT_WRITE);
    return mprotect(address, size, p);
}

static void x_free(void *user, void *address, size_t size)
{
    (void)user;
    munmap(address, size);
}

static void *x_reserve(void *user, size_t size)
{
    (void)user;
    void *p = mmap(NULL, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
    return p == MAP_FAILED ? NULL : p;
}

static int x_alias(void *user, void *address, size_t size, int64_t offset)
{
    (void)user; (void)address; (void)size; (void)offset;
    return -1;
}

static int x_unmap(void *user, void *address, size_t size)
{
    (void)user;
    return munmap(address, size);
}

static void v_present(void *user, const void *frame, unsigned page)
{
    (void)user; (void)frame; (void)page;
}
static void v_signal(void *user) { (void)user; }
static void v_wait(void *user) { (void)user; }

static void p_log(void *user, int level, const char *message)
{
    (void)user;
    fprintf(stderr, "[%d] %s\n", level, message ? message : "");
}

static const nds_platform_t platform = {
    NULL,
    "posix-x86",
    { f_open, f_open_fd, f_stat, f_remove, f_resolve },
    { h_create, h_join, m_init, m_destroy, m_lock, m_unlock,
      c_init, c_destroy, c_wait, c_signal, c_broadcast, h_sleep, h_cpus },
    { t_now_us, t_now_seconds, t_wall },
    { a_open, a_close, a_mic_open, a_mic_read, a_mic_close },
    { x_alloc, x_protect, x_free, x_reserve, x_alias, x_unmap },
    { v_present, v_signal, v_wait },
    p_log,
};

const nds_platform_t *port_platform(void)
{
    return &platform;
}
