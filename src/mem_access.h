#ifndef MEM_ACCESS_H
#define MEM_ACCESS_H

#include <stdint.h>
#include <string.h>

static inline uint8_t rd8(const void *p) { uint8_t v; memcpy(&v, p, 1); return v; }
static inline uint16_t rd16(const void *p) { uint16_t v; memcpy(&v, p, 2); return v; }
static inline int16_t rd16s(const void *p) { int16_t v; memcpy(&v, p, 2); return v; }
static inline uint32_t rd32(const void *p) { uint32_t v; memcpy(&v, p, 4); return v; }
static inline int32_t rd32s(const void *p) { int32_t v; memcpy(&v, p, 4); return v; }
static inline uint64_t rd64(const void *p) { uint64_t v; memcpy(&v, p, 8); return v; }
static inline float rd_f32(const void *p) { float v; memcpy(&v, p, 4); return v; }
static inline double rd_f64(const void *p) { double v; memcpy(&v, p, 8); return v; }
static inline void *rd_ptr(const void *p) { void *v; memcpy(&v, p, 8); return v; }
static inline uint8_t *rd_ptr_u8(const void *p) { uint8_t *v; memcpy(&v, p, 8); return v; }
static inline void wr8(void *p, uint8_t v) { memcpy(p, &v, 1); }
static inline void wr16(void *p, uint16_t v) { memcpy(p, &v, 2); }
static inline void wr32(void *p, uint32_t v) { memcpy(p, &v, 4); }
static inline void wr64(void *p, uint64_t v) { memcpy(p, &v, 8); }
static inline void wr_f32(void *p, float v) { memcpy(p, &v, 4); }
static inline void wr_f64(void *p, double v) { memcpy(p, &v, 8); }
static inline void wr_ptr(void *p, const void *v) { memcpy(p, &v, 8); }
static inline uint8_t rd8_at(const void *p, size_t off) { uint8_t v; memcpy(&v, (const uint8_t *)p + off, 1); return v; }
static inline uint16_t rd16_at(const void *p, size_t off) { uint16_t v; memcpy(&v, (const uint8_t *)p + off, 2); return v; }
static inline int16_t rd16s_at(const void *p, size_t off) { int16_t v; memcpy(&v, (const uint8_t *)p + off, 2); return v; }
static inline uint32_t rd32_at(const void *p, size_t off) { uint32_t v; memcpy(&v, (const uint8_t *)p + off, 4); return v; }
static inline int32_t rd32s_at(const void *p, size_t off) { int32_t v; memcpy(&v, (const uint8_t *)p + off, 4); return v; }
static inline uint64_t rd64_at(const void *p, size_t off) { uint64_t v; memcpy(&v, (const uint8_t *)p + off, 8); return v; }
static inline void *rd_ptr_at(const void *p, size_t off) { void *v; memcpy(&v, (const uint8_t *)p + off, 8); return v; }
static inline void wr8_at(void *p, size_t off, uint8_t v) { memcpy((uint8_t *)p + off, &v, 1); }
static inline void wr16_at(void *p, size_t off, uint16_t v) { memcpy((uint8_t *)p + off, &v, 2); }
static inline void wr32_at(void *p, size_t off, uint32_t v) { memcpy((uint8_t *)p + off, &v, 4); }
static inline void wr64_at(void *p, size_t off, uint64_t v) { memcpy((uint8_t *)p + off, &v, 8); }
static inline void wr_ptr_at(void *p, size_t off, const void *v) { memcpy((uint8_t *)p + off, &v, 8); }

#endif
