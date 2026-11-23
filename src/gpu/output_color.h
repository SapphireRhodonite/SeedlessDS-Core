#ifndef SEEDLESSDS_GPU_OUTPUT_COLOR_H
#define SEEDLESSDS_GPU_OUTPUT_COLOR_H

#include <stdint.h>
#include "frontend/frontend.h"

extern const uint8_t curve_5_from_6[64];
extern const uint8_t curve_6_from_6[64];
extern const uint8_t curve_5_from_8[256];
extern const uint8_t curve_6_from_8[256];

static inline int output_uses_color_curve(void)
{
    return (FRONTEND->config_bits & FRONTEND_CONFIG_POINT_COLOR_CURVE) != 0;
}

static inline uint16_t output_rgb565_from_planes(uint8_t red, uint8_t green, uint8_t blue, int curve)
{
    if (curve) {
        return (uint16_t)((curve_5_from_6[red & 63u] << 11) |
                          (curve_6_from_6[green & 63u] << 5) | curve_5_from_6[blue & 63u]);
    }
    unsigned high = ((red << 2) & 0xe0u) | (green >> 3);
    return (uint16_t)((high << 8) | ((green << 5) & 0xffu) | ((blue >> 1) & 31u));
}

static inline uint32_t output_curve_rgb888(uint16_t value)
{
    unsigned r = value >> 11, g = (value >> 5) & 63u, b = value & 31u;
    r = (r * r * 255u + 480u) / 961u;
    g = (g * g * 255u + 1984u) / 3969u;
    b = (b * b * 255u + 480u) / 961u;
    return r | (g << 8) | (b << 16);
}

static inline uint16_t output_curve_to_rgb565(uint16_t value)
{
    unsigned r = value >> 11, g = (value >> 5) & 63u, b = value & 31u;
    return (uint16_t)((((r * r + 15u) / 31u) << 11) |
                      (((g * g + 31u) / 63u) << 5) | ((b * b + 15u) / 31u));
}

#endif
