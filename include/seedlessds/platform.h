#ifndef SEEDLESSDS_PLATFORM_H
#define SEEDLESSDS_PLATFORM_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct nds_platform nds_platform_t;

#define NDS_OPEN_READ 0
#define NDS_OPEN_WRITE 1
#define NDS_OPEN_APPEND 2
#define NDS_OPEN_READ_WRITE 3

#define NDS_SEEK_SET 0
#define NDS_SEEK_CUR 1
#define NDS_SEEK_END 2

#define NDS_LOG_DEBUG 0
#define NDS_LOG_INFO 1
#define NDS_LOG_WARN 2
#define NDS_LOG_ERROR 3

#define NDS_EXEC_READ_WRITE 0
#define NDS_EXEC_READ_EXEC 1

typedef struct nds_wall_clock {
    int year;
    int month;
    int day;
    int weekday;
    int hour;
    int minute;
    int second;
} nds_wall_clock_t;

typedef void (*nds_audio_fill_fn)(void *user, void *samples, size_t frames);
typedef void *(*nds_thread_fn)(void *user);

typedef struct nds_platform_files {
    void *(*open)(void *user, const char *path, const char *mode);
    int (*open_fd)(void *user, const char *path, int flags);
    int (*stat)(void *user, const char *path, void *out);
    int (*remove)(void *user, const char *path);
    int (*resolve)(void *user, const char *name, char *path, size_t size);
} nds_platform_files_t;

typedef struct nds_platform_threads {
    int (*thread_create)(void *user, void *thread, nds_thread_fn fn, void *arg);
    int (*thread_join)(void *user, void *thread);
    int (*mutex_init)(void *mutex);
    int (*mutex_destroy)(void *mutex);
    int (*mutex_lock)(void *mutex);
    int (*mutex_unlock)(void *mutex);
    int (*cond_init)(void *cond);
    int (*cond_destroy)(void *cond);
    int (*cond_wait)(void *cond, void *mutex);
    int (*cond_signal)(void *cond);
    int (*cond_broadcast)(void *cond);
    void (*sleep_us)(uint64_t microseconds);
    unsigned (*cpu_count)(void *user);
} nds_platform_threads_t;

typedef struct nds_platform_time {
    uint64_t (*now_us)(void *user);
    int64_t (*now_seconds)(void *user);
    void (*wall_clock)(void *user, nds_wall_clock_t *out);
} nds_platform_time_t;

typedef struct nds_platform_audio {
    int (*open)(void *user, unsigned rate, unsigned channels, nds_audio_fill_fn fill, void *fill_user);
    void (*close)(void *user);
    int (*mic_open)(void *user, unsigned rate);
    int64_t (*mic_read)(void *user, void *samples, int64_t frames);
    void (*mic_close)(void *user);
} nds_platform_audio_t;

typedef struct nds_platform_memory {
    void *(*exec_alloc)(void *user, size_t size);
    int (*exec_protect)(void *user, void *address, size_t size, int mode);
    void (*exec_free)(void *user, void *address, size_t size);
    void *(*reserve)(void *user, size_t size);
    int (*alias)(void *user, void *address, size_t size, int64_t offset);
    int (*unmap)(void *user, void *address, size_t size);
} nds_platform_memory_t;

typedef struct nds_platform_video {
    void (*present)(void *user, const void *frame, unsigned page);
    void (*screen_signal)(void *user);
    void (*screen_wait)(void *user);
} nds_platform_video_t;

struct nds_platform {
    void *user;
    const char *name;
    nds_platform_files_t files;
    nds_platform_threads_t threads;
    nds_platform_time_t time;
    nds_platform_audio_t audio;
    nds_platform_memory_t memory;
    nds_platform_video_t video;
    void (*log)(void *user, int level, const char *message);
};

const nds_platform_t *nds_platform_default(void);

void *platform_file_open(const char *path, const char *mode);
int platform_file_open_fd(const char *path, int flags);
int platform_file_stat(const char *path, void *out);

int platform_mutex_init(void *mutex, void *attr);
int platform_cond_init(void *cond, void *attr);
int platform_thread_create(void *thread, const void *attr, void *(*fn)(void *), void *arg);

#ifdef __cplusplus
}
#endif

#endif
