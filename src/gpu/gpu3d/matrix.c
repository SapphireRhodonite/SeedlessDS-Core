#include <stdint.h>
#include "hires_runtime.h"

__asm__(
"    .text\n"
"    .align 2\n"
"    .type  gpu3d_matrix_mult_4x4_jit_body, %function\n"
"gpu3d_matrix_mult_4x4_jit_body:\n"
"    sub sp, sp, #0xf0         \n"
"    stp d15, d14, [sp, #80]   \n"
"    stp d13, d12, [sp, #96]   \n"
"    stp d11, d10, [sp, #112]  \n"
"    stp d9, d8, [sp, #128]    \n"
"    stp x28, x27, [sp, #144]  \n"
"    stp x26, x25, [sp, #160]  \n"
"    stp x24, x23, [sp, #176]  \n"
"    stp x22, x21, [sp, #192]  \n"
"    stp x20, x19, [sp, #208]  \n"
"    stp x29, x30, [sp, #224]  \n"
"    str x0, [sp, #72]         \n"
"    ldp q1, q2, [x1]          \n"
"    ldp q3, q0, [x1, #32]     \n"
"    ldpsw x0, x9, [x2]        \n"
"    ldpsw x8, x1, [x2, #8]    \n"
"    sxtl2 v4.2d, v1.4s        \n"
"    ldpsw x21, x4, [x2, #16]  \n"
"    sxtl2 v5.2d, v2.4s        \n"
"    sxtl v2.2d, v2.2s         \n"
"    sxtl2 v17.2d, v3.4s       \n"
"    fmov x27, d4              \n"
"    sxtl2 v18.2d, v0.4s       \n"
"    mul x7, x0, x27           \n"
"    fmov x26, d2              \n"
"    fmov x25, d5              \n"
"    fmov x20, d17             \n"
"    ldpsw x11, x12, [x2, #24] \n"
"    ldpsw x17, x14, [x2, #32] \n"
"    ldpsw x23, x5, [x2, #40]  \n"
"    ldpsw x15, x13, [x2, #48] \n"
"    sxtl v1.2d, v1.2s         \n"
"    mov x10, v2.d[1]          \n"
"    stp x9, x8, [sp, #48]     \n"
"    mul x29, x9, x26          \n"
"    mul x30, x9, x25          \n"
"    mov x9, x8                \n"
"    mul x8, x8, x20           \n"
"    fmov d2, x7               \n"
"    fmov x7, d18              \n"
"    mov x16, v1.d[1]          \n"
"    fmov x28, d1              \n"
"    fmov d1, x8               \n"
"    mul x8, x1, x7            \n"
"    sxtl v7.2d, v3.2s         \n"
"    fmov d16, x8              \n"
"    mul x8, x21, x27          \n"
"    fmov x19, d7              \n"
"    fmov d21, x8              \n"
"    mul x8, x4, x25           \n"
"    fmov d23, x8              \n"
"    mul x8, x11, x19          \n"
"    fmov d25, x8              \n"
"    mul x8, x12, x7           \n"
"    fmov d27, x8              \n"
"    mul x8, x17, x27          \n"
"    fmov d29, x8              \n"
"    mul x8, x14, x25          \n"
"    fmov d31, x8              \n"
"    mul x8, x23, x19          \n"
"    fmov d9, x8               \n"
"    mul x8, x5, x7            \n"
"    fmov d11, x8              \n"
"    mul x8, x21, x16          \n"
"    mov x24, v4.d[1]          \n"
"    str x8, [sp, #8]          \n"
"    mul x8, x17, x16          \n"
"    str x8, [sp, #24]         \n"
"    mul x8, x17, x24          \n"
"    str x8, [sp, #16]         \n"
"    mul x8, x15, x16          \n"
"    mul x9, x9, x19           \n"
"    str x8, [sp, #40]         \n"
"    mul x8, x15, x24          \n"
"    sxtl v19.2d, v0.2s        \n"
"    mul x6, x0, x28           \n"
"    fmov d6, x9               \n"
"    mul x9, x21, x28          \n"
"    str x8, [sp, #32]         \n"
"    ldr x8, [sp, #48]         \n"
"    fmov d0, x6               \n"
"    fmov x6, d19              \n"
"    fmov d20, x9              \n"
"    mul x9, x4, x26           \n"
"    fmov d4, x29              \n"
"    str x1, [sp, #64]         \n"
"    mul x29, x1, x6           \n"
"    fmov d22, x9              \n"
"    mul x9, x11, x20          \n"
"    mov x22, v5.d[1]          \n"
"    fmov d3, x29              \n"
"    fmov d24, x9              \n"
"    mul x9, x12, x6           \n"
"    ldpsw x29, x2, [x2, #56]  \n"
"    fmov d26, x9              \n"
"    mul x9, x17, x28          \n"
"    mul x3, x0, x24           \n"
"    mul x21, x21, x24         \n"
"    mul x24, x8, x10          \n"
"    mul x17, x8, x22          \n"
"    mul x8, x13, x22          \n"
"    str x8, [sp, #48]         \n"
"    ldr x8, [sp, #56]         \n"
"    fmov d28, x9              \n"
"    mul x9, x14, x26          \n"
"    fmov d30, x9              \n"
"    mul x9, x23, x20          \n"
"    mul x27, x15, x27         \n"
"    mul x20, x29, x20         \n"
"    mul x28, x15, x28         \n"
"    mul x25, x13, x25         \n"
"    mul x1, x0, x16           \n"
"    fmov d13, x27             \n"
"    mul x27, x4, x10          \n"
"    mul x0, x4, x22           \n"
"    mul x4, x14, x22          \n"
"    mov x22, v17.d[1]         \n"
"    fmov d17, x20             \n"
"    mov x20, v7.d[1]          \n"
"    mul x26, x13, x26         \n"
"    fmov d12, x28             \n"
"    mul x28, x13, x10         \n"
"    fmov d15, x25             \n"
"    mul x25, x8, x22          \n"
"    mul x13, x8, x20          \n"
"    ldr x8, [sp, #64]         \n"
"    fmov d8, x9               \n"
"    mul x9, x5, x6            \n"
"    mul x19, x29, x19         \n"
"    mul x6, x2, x6            \n"
"    fmov d7, x19              \n"
"    mov x19, v19.d[1]         \n"
"    fmov d19, x6              \n"
"    mul x6, x2, x7            \n"
"    mov x7, v18.d[1]          \n"
"    fmov d5, x30              \n"
"    fmov d14, x26             \n"
"    mul x26, x14, x10         \n"
"    mul x30, x11, x22         \n"
"    mul x14, x11, x20         \n"
"    mul x16, x23, x22         \n"
"    mul x15, x23, x20         \n"
"    mul x22, x29, x22         \n"
"    mul x20, x29, x20         \n"
"    mul x29, x8, x19          \n"
"    mul x8, x8, x7            \n"
"    mov v16.d[1], x8          \n"
"    ldr x8, [sp, #8]          \n"
"    mov v0.d[1], x1           \n"
"    mov v4.d[1], x24          \n"
"    mov v2.d[1], x3           \n"
"    mov v20.d[1], x8          \n"
"    ldr x8, [sp, #24]         \n"
"    mov v5.d[1], x17          \n"
"    mov v6.d[1], x13          \n"
"    mov v22.d[1], x27         \n"
"    mov v28.d[1], x8          \n"
"    ldr x8, [sp, #16]         \n"
"    add v0.2d, v4.2d, v0.2d   \n"
"    fmov d10, x9              \n"
"    mul x9, x12, x19          \n"
"    mov v29.d[1], x8          \n"
"    ldr x8, [sp, #40]         \n"
"    mov v1.d[1], x25          \n"
"    mov v3.d[1], x29          \n"
"    mov v21.d[1], x21         \n"
"    mov v12.d[1], x8          \n"
"    ldr x8, [sp, #32]         \n"
"    mov v23.d[1], x0          \n"
"    mov v25.d[1], x14         \n"
"    mov v30.d[1], x26         \n"
"    mov v13.d[1], x8          \n"
"    ldr x8, [sp, #48]         \n"
"    mov v14.d[1], x28         \n"
"    add v2.2d, v5.2d, v2.2d   \n"
"    add v5.2d, v22.2d, v20.2d \n"
"    add v0.2d, v0.2d, v6.2d   \n"
"    mul x10, x12, x7          \n"
"    mul x11, x5, x19          \n"
"    mul x19, x2, x19          \n"
"    mov v24.d[1], x30         \n"
"    mov v26.d[1], x9          \n"
"    mov v31.d[1], x4          \n"
"    mov v9.d[1], x15          \n"
"    mov v15.d[1], x8          \n"
"    mov v7.d[1], x20          \n"
"    add v4.2d, v23.2d, v21.2d \n"
"    add v21.2d, v30.2d, v28.2d\n"
"    add v23.2d, v14.2d, v12.2d\n"
"    add v1.2d, v2.2d, v1.2d   \n"
"    add v2.2d, v5.2d, v25.2d  \n"
"    add v0.2d, v0.2d, v3.2d   \n"
"    mul x12, x5, x7           \n"
"    mul x2, x2, x7            \n"
"    fmov d18, x6              \n"
"    mov v27.d[1], x10         \n"
"    mov v8.d[1], x16          \n"
"    mov v10.d[1], x11         \n"
"    mov v17.d[1], x22         \n"
"    mov v19.d[1], x19         \n"
"    add v20.2d, v31.2d, v29.2d\n"
"    add v22.2d, v15.2d, v13.2d\n"
"    add v4.2d, v4.2d, v24.2d  \n"
"    add v5.2d, v21.2d, v9.2d  \n"
"    add v7.2d, v23.2d, v7.2d  \n"
"    add v1.2d, v1.2d, v16.2d  \n"
"    add v2.2d, v2.2d, v26.2d  \n"
"    shrn v0.2s, v0.2d, #12    \n"
"    ldr x8, [sp, #72]         \n"
"    mov v11.d[1], x12         \n"
"    mov v18.d[1], x2          \n"
"    add v6.2d, v20.2d, v8.2d  \n"
"    add v17.2d, v22.2d, v17.2d\n"
"    add v3.2d, v4.2d, v27.2d  \n"
"    add v5.2d, v5.2d, v10.2d  \n"
"    add v7.2d, v7.2d, v19.2d  \n"
"    shrn2 v0.4s, v1.2d, #12   \n"
"    shrn v1.2s, v2.2d, #12    \n"
"    add v4.2d, v6.2d, v11.2d  \n"
"    add v6.2d, v17.2d, v18.2d \n"
"    shrn2 v1.4s, v3.2d, #12   \n"
"    shrn v2.2s, v5.2d, #12    \n"
"    shrn v3.2s, v7.2d, #12    \n"
"    shrn2 v2.4s, v4.2d, #12   \n"
"    shrn2 v3.4s, v6.2d, #12   \n"
"    stp q0, q1, [x8]          \n"
"    stp q2, q3, [x8, #32]     \n"
"    ldp x29, x30, [sp, #224]  \n"
"    ldp x20, x19, [sp, #208]  \n"
"    ldp x22, x21, [sp, #192]  \n"
"    ldp x24, x23, [sp, #176]  \n"
"    ldp x26, x25, [sp, #160]  \n"
"    ldp x28, x27, [sp, #144]  \n"
"    ldp d9, d8, [sp, #128]    \n"
"    ldp d11, d10, [sp, #112]  \n"
"    ldp d13, d12, [sp, #96]   \n"
"    ldp d15, d14, [sp, #80]   \n"
"    add sp, sp, #0xf0         \n"
"    ret                       \n"
"    .size gpu3d_matrix_mult_4x4_jit_body, .-gpu3d_matrix_mult_4x4_jit_body\n");
uint64_t gpu3d_matrix_mult_4x4_jit_body(void *dest, const void *matrix,
                                   const void *origin);

uint64_t gpu3d_matrix_mult_4x4_jit(void *dest, const void *matrix,
                            const void *origin)
{

    return gpu3d_matrix_mult_4x4_jit_body(dest, matrix, origin);
}

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif

void gpu3d_matrix_mult_4x4_neon(int32_t *output, const int32_t *a, const int32_t *b) {
#ifdef __ARM_NEON

    {
        const int32x4_t a0 = vld1q_s32(a), a1 = vld1q_s32(a + 4), a2 = vld1q_s32(a + 8), a3 = vld1q_s32(a + 12);
        int32x4x4_t o;
#define M4E8_ROW(BJ) vcombine_s32(             vshrn_n_s64(vmlal_laneq_s32(vmlal_laneq_s32(vmlal_laneq_s32(vmull_laneq_s32(vget_low_s32(a0), BJ, 0), vget_low_s32(a1), BJ, 1), vget_low_s32(a2), BJ, 2), vget_low_s32(a3), BJ, 3), 12),             vshrn_n_s64(vmlal_high_laneq_s32(vmlal_high_laneq_s32(vmlal_high_laneq_s32(vmull_high_laneq_s32(a0, BJ, 0), a1, BJ, 1), a2, BJ, 2), a3, BJ, 3), 12))
        { const int32x4_t bj = vld1q_s32(b);      o.val[0] = M4E8_ROW(bj); }
        { const int32x4_t bj = vld1q_s32(b + 4);  o.val[1] = M4E8_ROW(bj); }
        { const int32x4_t bj = vld1q_s32(b + 8);  o.val[2] = M4E8_ROW(bj); }
        { const int32x4_t bj = vld1q_s32(b + 12); o.val[3] = M4E8_ROW(bj); }
#undef M4E8_ROW
        vst1q_s32_x4(output, o);
        return;
    }
#endif
    int32_t r[16];

    for (int j = 0; j < 4; j++) {
        for (int k = 0; k < 4; k++) {
            int64_t acc = 0;
            for (int i = 0; i < 4; i++)
                acc += (int64_t)a[i * 4 + k] * (int64_t)b[j * 4 + i];
            r[j * 4 + k] = (int32_t)(uint32_t)((uint64_t)acc >> 12);
        }
    }

    for (int n = 0; n < 16; n++) output[n] = r[n];
}

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif
#include <stdint.h>
#include <string.h>

void gpu3d_matrix_mult_4x3(int32_t *output, const int32_t *a, const int32_t *b) {
#ifdef __ARM_NEON

    {
        const int32x4_t a0 = vld1q_s32(a), a1 = vld1q_s32(a + 4), a2 = vld1q_s32(a + 8), a3 = vld1q_s32(a + 12);
        const int32x4_t b0 = vld1q_s32(b), b1 = vld1q_s32(b + 4), b2 = vld1q_s32(b + 8);
        int32x4x4_t o;
#define M598_ROW(V0, L0, V1, L1, V2, L2) vcombine_s32(             vshrn_n_s64(vmlal_laneq_s32(vmlal_laneq_s32(vmull_laneq_s32(vget_low_s32(a0), V0, L0), vget_low_s32(a1), V1, L1), vget_low_s32(a2), V2, L2), 12),             vshrn_n_s64(vmlal_high_laneq_s32(vmlal_high_laneq_s32(vmull_high_laneq_s32(a0, V0, L0), a1, V1, L1), a2, V2, L2), 12))
        o.val[0] = M598_ROW(b0, 0, b0, 1, b0, 2);
        o.val[1] = M598_ROW(b0, 3, b1, 0, b1, 1);
        o.val[2] = M598_ROW(b1, 2, b1, 3, b2, 0);
        o.val[3] = vaddq_s32(M598_ROW(b2, 1, b2, 2, b2, 3), a3);
#undef M598_ROW
        vst1q_s32_x4(output, o);
        return;
    }
#endif
    int32_t r[16];

    for (int j = 0; j < 4; j++) {
        for (int k = 0; k < 4; k++) {
            int64_t acc = 0;
            for (int i = 0; i < 3; i++)
                acc += (int64_t)a[i * 4 + k] * (int64_t)b[j * 3 + i];
            int32_t v = (int32_t)(uint32_t)((uint64_t)acc >> 12);
            if (j == 3)
                v += a[3 * 4 + k];
            r[j * 4 + k] = v;
        }
    }

    memcpy(output, r, sizeof(r));
}
