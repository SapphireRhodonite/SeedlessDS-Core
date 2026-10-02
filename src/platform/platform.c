#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include "hires_runtime.h"
#include "blob_symbols.h"
#include "seedlessds/platform.h"
#include "core_internals.h"

typedef int (*fn_gettimeofday)(struct timeval *, void *);
typedef long (*fn_time)(long *);
typedef struct tm *(*fn_localtime)(const long *);
typedef int (*fn_usleep)(uint32_t);

static uint64_t default_now_us(void *user)
{
    (void)user;
    static fn_gettimeofday gtod;
    if (!gtod) gtod = (fn_gettimeofday)sym_libc_gettimeofday;
    struct timeval tv;
    gtod(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000ULL + (uint64_t)tv.tv_usec;
}

static int64_t default_now_seconds(void *user)
{
    (void)user;
    static fn_time core_time;
    if (!core_time) core_time = (fn_time)sym_libc_time;
    return (int64_t)core_time(0);
}

static void default_wall_clock(void *user, nds_wall_clock_t *out)
{
    (void)user;
    static fn_time core_time;
    static fn_localtime core_localtime;
    if (!core_time) {
        core_time = (fn_time)sym_libc_time;
        core_localtime = (fn_localtime)sym_libc_localtime;
    }
    long now = core_time(0);
    const struct tm *t = core_localtime(&now);
    out->year = t->tm_year + 1900;
    out->month = t->tm_mon + 1;
    out->day = t->tm_mday;
    out->weekday = t->tm_wday;
    out->hour = t->tm_hour;
    out->minute = t->tm_min;
    out->second = t->tm_sec;
}

typedef int (*fn_mutex1)(void *);
typedef int (*fn_mutex2)(void *, void *);
typedef int (*fn_create)(void *, const void *, void *(*)(void *), void *);
typedef int (*fn_join)(void *, void **);

static int default_thread_create(void *user, void *thread, nds_thread_fn fn, void *arg)
{
    (void)user;
    return ((fn_create)sym_libc_pthread_create)(thread, 0, fn, arg);
}

static int default_thread_join(void *user, void *thread)
{
    (void)user;
    return ((fn_join)sym_libc_pthread_join)(thread, 0);
}

static int default_mutex_init(void *mutex)
{
    return ((fn_mutex2)sym_libc_pthread_mutex_init)(mutex, 0);
}

static int default_mutex_destroy(void *mutex)
{
    return ((fn_mutex1)sym_libc_pthread_mutex_destroy)(mutex);
}

static int default_mutex_lock(void *mutex)
{
    return ((fn_mutex1)sym_libc_pthread_mutex_lock)(mutex);
}

static int default_mutex_unlock(void *mutex)
{
    return ((fn_mutex1)sym_libc_pthread_mutex_unlock)(mutex);
}

static int default_cond_init(void *cond)
{
    return ((fn_mutex2)sym_libc_pthread_cond_init)(cond, 0);
}

static int default_cond_destroy(void *cond)
{
    return ((fn_mutex1)sym_libc_pthread_cond_destroy)(cond);
}

static int default_cond_wait(void *cond, void *mutex)
{
    return ((fn_mutex2)sym_libc_pthread_cond_wait)(cond, mutex);
}

static int default_cond_signal(void *cond)
{
    return ((fn_mutex1)sym_libc_pthread_cond_signal)(cond);
}

static int default_cond_broadcast(void *cond)
{
    return ((fn_mutex1)sym_libc_pthread_cond_broadcast)(cond);
}

static void default_sleep_us(uint64_t microseconds)
{
    static fn_usleep core_usleep;
    if (!core_usleep) core_usleep = (fn_usleep)sym_libc_usleep;
    core_usleep((uint32_t)microseconds);
}

typedef int (*fn_remove)(const char *);

static void *default_file_open(void *user, const char *path, const char *mode)
{
    (void)user;
    return files_fopen_resolved(path, mode);
}

static int default_file_open_fd(void *user, const char *path, int flags)
{
    (void)user;
    return files_open_translate_flags(path, flags);
}

static int default_file_stat(void *user, const char *path, void *out)
{
    (void)user;
    return files_stat_resolved(path, out);
}

static int default_file_remove(void *user, const char *path)
{
    (void)user;
    static fn_remove core_remove;
    if (!core_remove) core_remove = (fn_remove)sym_libc_remove;
    return core_remove(path);
}

static const nds_platform_t platform_default = {
    NULL,
    "posix",
    { default_file_open, default_file_open_fd, default_file_stat, default_file_remove, NULL },
    { default_thread_create, default_thread_join,
      default_mutex_init, default_mutex_destroy, default_mutex_lock, default_mutex_unlock,
      default_cond_init, default_cond_destroy, default_cond_wait, default_cond_signal,
      default_cond_broadcast, default_sleep_us, NULL },
    { default_now_us, default_now_seconds, default_wall_clock },
    { NULL, NULL, NULL, NULL, NULL },
    { NULL, NULL, NULL, NULL, NULL, NULL },
    { NULL, NULL, NULL },
    NULL,
};

static const nds_platform_t *platform_active;

const nds_platform_t *nds_platform_default(void)
{
    return platform_active ? platform_active : &platform_default;
}

void *platform_file_open(const char *path, const char *mode)
{
    const nds_platform_t *p = nds_platform_default();
    return p->files.open(p->user, path, mode);
}

int platform_file_open_fd(const char *path, int flags)
{
    const nds_platform_t *p = nds_platform_default();
    return p->files.open_fd(p->user, path, flags);
}

int platform_file_stat(const char *path, void *out)
{
    const nds_platform_t *p = nds_platform_default();
    return p->files.stat(p->user, path, out);
}

int platform_mutex_init(void *mutex, void *attr)
{
    (void)attr;
    return nds_platform_default()->threads.mutex_init(mutex);
}

int platform_cond_init(void *cond, void *attr)
{
    (void)attr;
    return nds_platform_default()->threads.cond_init(cond);
}

int platform_thread_create(void *thread, const void *attr, void *(*fn)(void *), void *arg)
{
    (void)attr;
    const nds_platform_t *p = nds_platform_default();
    return p->threads.thread_create(p->user, thread, fn, arg);
}

void platform_set(const nds_platform_t *platform)
{
    platform_active = platform;
}
