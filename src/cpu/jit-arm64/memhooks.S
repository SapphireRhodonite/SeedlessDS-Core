    .text
    .align 2
    .globl jit_dispatch_thumb
    .type  jit_dispatch_thumb, %function
jit_dispatch_thumb:
    ldr  x1, [x28, #8824]
    ubfx w2, w0, #1, #14
    ldr  w1, [x1, w2, uxtw #2]
    cbz  w1, L_jit_dispatch_thumb_exit_1
    add  x1, x11, w1, uxtw
    br   x1
L_jit_dispatch_thumb_exit_1:
    b    jit_dispatch_lookup_or_compile
    .size jit_dispatch_thumb, .-jit_dispatch_thumb

    .text
    .align 2
    .globl jit_mem_read8_generic
    .type  jit_mem_read8_generic, %function
jit_mem_read8_generic:
    orr  w1, wzr, w0, lsr #11
    ldr  x1, [x9, w1, uxtw #3]
    orr  x1, xzr, x1, lsl #2
    cbz  x1, 1f
    ldrb w0, [x1, w0, uxtw]
    ret
1:
    mov  w1, w0
    mov  x0, x9
    stp  w12, w13, [x9, #-224]
    stp  w14, w15, [x9, #-216]
    stp  w16, w17, [x9, #-208]
    str  w18, [x28, #8968]
    str  x30, [x28, #9064]
    mrs  x2, nzcv
    str  w2, [x28, #9044]
    str  w12, [x28, #8848]
    bl   bus_read8_slow
    add  x9, x28, #0x3d0
    add  x9, x9, #0x2, lsl #12
    ldp  x10, x11, [x9, #-120]
    ldp  w12, w13, [x9, #-224]
    ldp  w14, w15, [x9, #-216]
    ldp  w16, w17, [x9, #-208]
    ldr  w18, [x28, #8968]
    ldr  x30, [x28, #9064]
    ldr  w2, [x28, #9044]
    msr  nzcv, x2
    uxtb w0, w0
    ret
    .size jit_mem_read8_generic, .-jit_mem_read8_generic

    .text
    .align 2
    .globl jit_mem_read8s_generic
    .type  jit_mem_read8s_generic, %function
jit_mem_read8s_generic:
    orr   w1, wzr, w0, lsr #11
    ldr   x1, [x9, w1, uxtw #3]
    orr   x1, xzr, x1, lsl #2
    cbz   x1, 1f
    ldrsb w0, [x1, w0, uxtw]
    ret
1:
    mov   w1, w0
    mov   x0, x9
    stp   w12, w13, [x9, #-224]
    stp   w14, w15, [x9, #-216]
    stp   w16, w17, [x9, #-208]
    str   w18, [x28, #8968]
    str   x30, [x28, #9064]
    mrs   x2, nzcv
    str   w2, [x28, #9044]
    str   w12, [x28, #8848]
    adrp x16, :got:bus_read8_slow
    ldr  x16, [x16, :got_lo12:bus_read8_slow]
    blr   x16
    add   x9, x28, #0x3d0
    add   x9, x9, #0x2, lsl #12
    ldp   x10, x11, [x9, #-120]
    ldp   w12, w13, [x9, #-224]
    ldp   w14, w15, [x9, #-216]
    ldp   w16, w17, [x9, #-208]
    ldr   w18, [x28, #8968]
    ldr   x30, [x28, #9064]
    ldr   w2, [x28, #9044]
    msr   nzcv, x2
    sxtb  w0, w0
    ret
    .size jit_mem_read8s_generic, .-jit_mem_read8s_generic

    .text
    .align 2
    .globl jit_mem_read16_generic
    .type  jit_mem_read16_generic, %function
jit_mem_read16_generic:
    orr  w1, wzr, w0, lsr #11
    and  w0, w0, #0xfffffffe
    ldr  x1, [x9, w1, uxtw #3]
    orr  x1, xzr, x1, lsl #2
    cbz  x1, 1f
    ldrh w0, [x1, w0, uxtw]
    ret
1:
    mov  w1, w0
    mov  x0, x9
    stp  w12, w13, [x9, #-224]
    stp  w14, w15, [x9, #-216]
    stp  w16, w17, [x9, #-208]
    str  w18, [x28, #8968]
    str  x30, [x28, #9064]
    mrs  x2, nzcv
    str  w2, [x28, #9044]
    str  w12, [x28, #8848]
    bl   bus_read16_slow
    add  x9, x28, #0x3d0
    add  x9, x9, #0x2, lsl #12
    ldp  x10, x11, [x9, #-120]
    ldp  w12, w13, [x9, #-224]
    ldp  w14, w15, [x9, #-216]
    ldp  w16, w17, [x9, #-208]
    ldr  w18, [x28, #8968]
    ldr  x30, [x28, #9064]
    ldr  w2, [x28, #9044]
    msr  nzcv, x2
    uxth w0, w0
    ret
    .size jit_mem_read16_generic, .-jit_mem_read16_generic

    .text
    .align 2
    .globl jit_mem_read16s_generic
    .type  jit_mem_read16s_generic, %function
jit_mem_read16s_generic:
    orr   w1, wzr, w0, lsr #11
    and   w0, w0, #0xfffffffe
    ldr   x1, [x9, w1, uxtw #3]
    orr   x1, xzr, x1, lsl #2
    cbz   x1, 1f
    ldrsh w0, [x1, w0, uxtw]
    ret
1:
    mov   w1, w0
    mov   x0, x9
    stp   w12, w13, [x9, #-224]
    stp   w14, w15, [x9, #-216]
    stp   w16, w17, [x9, #-208]
    str   w18, [x28, #8968]
    str   x30, [x28, #9064]
    mrs   x2, nzcv
    str   w2, [x28, #9044]
    str   w12, [x28, #8848]
    adrp x16, :got:bus_read16_slow
    ldr  x16, [x16, :got_lo12:bus_read16_slow]
    blr   x16
    add   x9, x28, #0x3d0
    add   x9, x9, #0x2, lsl #12
    ldp   x10, x11, [x9, #-120]
    ldp   w12, w13, [x9, #-224]
    ldp   w14, w15, [x9, #-216]
    ldp   w16, w17, [x9, #-208]
    ldr   w18, [x28, #8968]
    ldr   x30, [x28, #9064]
    ldr   w2, [x28, #9044]
    msr   nzcv, x2
    sxth  w0, w0
    ret
    .size jit_mem_read16s_generic, .-jit_mem_read16s_generic

    .text
    .align 2
    .globl jit_mem_read32x2_hook
    .type  jit_mem_read32x2_hook, %function
jit_mem_read32x2_hook:
    add  w1, w0, #0x4
    str  x30, [x28, #9016]
    str  w1, [x28, #9012]
    adrp x1, :got:jit_hook_mem_read32
    ldr  x1, [x1, :got_lo12:jit_hook_mem_read32]
    blr  x1
    str  w0, [x28, #9024]
    ldr  w0, [x28, #9012]
    adrp x1, :got:jit_hook_mem_read32
    ldr  x1, [x1, :got_lo12:jit_hook_mem_read32]
    blr  x1
    ldr  w1, [x28, #9024]
    ldr  x30, [x28, #9016]
    ret
    .size jit_mem_read32x2_hook, .-jit_mem_read32x2_hook

    .text
    .align 2
    .globl jit_mem_read8_hook
    .type  jit_mem_read8_hook, %function
jit_mem_read8_hook:
    orr  w1, wzr, w0, lsr #24
    sub  w1, w1, #0x4
    cbnz w1, L_jit_mem_read8_hook_exit_1
    ubfx w1, w0, #0, #23
    stp  w12, w13, [x9, #-224]
    stp  w14, w15, [x9, #-216]
    stp  w16, w17, [x9, #-208]
    str  w18, [x28, #8968]
    str  x30, [x28, #9064]
    mrs  x2, nzcv
    str  w2, [x28, #9044]
    str  w12, [x28, #8848]
    ldr  x0, [x28, #8800]
    bl   io9_read8
    add  x9, x28, #0x3d0
    add  x9, x9, #0x2, lsl #12
    ldp  x10, x11, [x9, #-120]
    ldp  w12, w13, [x9, #-224]
    ldp  w14, w15, [x9, #-216]
    ldp  w16, w17, [x9, #-208]
    ldr  w18, [x28, #8968]
    ldr  x30, [x28, #9064]
    ldr  w2, [x28, #9044]
    msr  nzcv, x2
    uxtb w0, w0
    ret
L_jit_mem_read8_hook_exit_1:
    b    jit_mem_read8_generic
    .size jit_mem_read8_hook, .-jit_mem_read8_hook

    .text
    .align 2
    .globl jit_mem_read8s_hook
    .type  jit_mem_read8s_hook, %function
jit_mem_read8s_hook:
    orr  w1, wzr, w0, lsr #24
    sub  w1, w1, #0x4
    cbnz w1, L_jit_mem_read8s_hook_exit_1
    ubfx w1, w0, #0, #23
    stp  w12, w13, [x9, #-224]
    stp  w14, w15, [x9, #-216]
    stp  w16, w17, [x9, #-208]
    str  w18, [x28, #8968]
    str  x30, [x28, #9064]
    mrs  x2, nzcv
    str  w2, [x28, #9044]
    str  w12, [x28, #8848]
    ldr  x0, [x28, #8800]
    bl   io9_read8
    add  x9, x28, #0x3d0
    add  x9, x9, #0x2, lsl #12
    ldp  x10, x11, [x9, #-120]
    ldp  w12, w13, [x9, #-224]
    ldp  w14, w15, [x9, #-216]
    ldp  w16, w17, [x9, #-208]
    ldr  w18, [x28, #8968]
    ldr  x30, [x28, #9064]
    ldr  w2, [x28, #9044]
    msr  nzcv, x2
    sxtb w0, w0
    ret
L_jit_mem_read8s_hook_exit_1:
    b    jit_mem_read8s_generic
    .size jit_mem_read8s_hook, .-jit_mem_read8s_hook

    .text
    .align 2
    .globl jit_mem_read16_hook
    .type  jit_mem_read16_hook, %function
jit_mem_read16_hook:
    and  w0, w0, #0xfffffffe
    orr  w1, wzr, w0, lsr #24
    sub  w1, w1, #0x4
    cbnz w1, L_jit_mem_read16_hook_exit_1
    ldr  x2, [x28, #8320]
    ubfx w0, w0, #0, #15
    and  w1, w0, #0x7f00
    cbnz w1, 2f
    ldrh w0, [x2, w0, uxtw]
    ret
2:
    ubfx w1, w0, #0, #23
    stp  w12, w13, [x9, #-224]
    stp  w14, w15, [x9, #-216]
    stp  w16, w17, [x9, #-208]
    str  w18, [x28, #8968]
    str  x30, [x28, #9064]
    mrs  x2, nzcv
    str  w2, [x28, #9044]
    str  w12, [x28, #8848]
    ldr  x0, [x28, #8800]
    bl   io9_read16
    add  x9, x28, #0x3d0
    add  x9, x9, #0x2, lsl #12
    ldp  x10, x11, [x9, #-120]
    ldp  w12, w13, [x9, #-224]
    ldp  w14, w15, [x9, #-216]
    ldp  w16, w17, [x9, #-208]
    ldr  w18, [x28, #8968]
    ldr  x30, [x28, #9064]
    ldr  w2, [x28, #9044]
    msr  nzcv, x2
    uxth w0, w0
    ret
L_jit_mem_read16_hook_exit_1:
    b    jit_mem_read16_generic
    .size jit_mem_read16_hook, .-jit_mem_read16_hook

    .text
    .align 2
    .globl jit_mem_read16s_hook
    .type  jit_mem_read16s_hook, %function
jit_mem_read16s_hook:
    and   w0, w0, #0xfffffffe
    orr   w1, wzr, w0, lsr #24
    sub   w1, w1, #0x4
    cbnz  w1, L_jit_mem_read16s_hook_exit_1
    ldr   x2, [x28, #8320]
    ubfx  w0, w0, #0, #15
    and   w1, w0, #0x7f00
    cbnz  w1, 2f
    ldrsh w0, [x2, w0, uxtw]
    ret
2:
    ubfx  w1, w0, #0, #23
    stp   w12, w13, [x9, #-224]
    stp   w14, w15, [x9, #-216]
    stp   w16, w17, [x9, #-208]
    str   w18, [x28, #8968]
    str   x30, [x28, #9064]
    mrs   x2, nzcv
    str   w2, [x28, #9044]
    str   w12, [x28, #8848]
    ldr   x0, [x28, #8800]
    adrp x16, :got:io9_read16
    ldr  x16, [x16, :got_lo12:io9_read16]
    blr   x16
    add   x9, x28, #0x3d0
    add   x9, x9, #0x2, lsl #12
    ldp   x10, x11, [x9, #-120]
    ldp   w12, w13, [x9, #-224]
    ldp   w14, w15, [x9, #-216]
    ldp   w16, w17, [x9, #-208]
    ldr   w18, [x28, #8968]
    ldr   x30, [x28, #9064]
    ldr   w2, [x28, #9044]
    msr   nzcv, x2
    sxth  w0, w0
    ret
L_jit_mem_read16s_hook_exit_1:
    adrp x1, :got:jit_mem_read16s_generic
    ldr  x1, [x1, :got_lo12:jit_mem_read16s_generic]
    br    x1
    .size jit_mem_read16s_hook, .-jit_mem_read16s_hook

    .text
    .align 2
    .globl jit_mem_read32_arm9
    .type  jit_mem_read32_arm9, %function
jit_mem_read32_arm9:
    and  w2, w0, #0x3
    cbnz w2, 3f
    orr  w1, wzr, w0, lsr #24
    sub  w1, w1, #0x4
    cbnz w1, L_jit_mem_read32_arm9_exit_1
    ubfx w1, w0, #0, #23
    stp  w12, w13, [x9, #-224]
    stp  w14, w15, [x9, #-216]
    stp  w16, w17, [x9, #-208]
    str  w18, [x28, #8968]
    str  x30, [x28, #9064]
    mrs  x2, nzcv
    str  w2, [x28, #9044]
    str  w12, [x28, #8848]
    ldr  x0, [x28, #8800]
    bl   io9_read32
    add  x9, x28, #0x3d0
    add  x9, x9, #0x2, lsl #12
    ldp  x10, x11, [x9, #-120]
    ldp  w12, w13, [x9, #-224]
    ldp  w14, w15, [x9, #-216]
    ldp  w16, w17, [x9, #-208]
    ldr  w18, [x28, #8968]
    ldr  x30, [x28, #9064]
    ldr  w2, [x28, #9044]
    msr  nzcv, x2
    ret
3:
    str  x30, [x28, #9008]
    str  w19, [x28, #9016]
    lsl  w19, w2, #3
    and  w0, w0, #0xfffffffc
    adrp x1, :got:jit_hook_mem_read32
    ldr  x1, [x1, :got_lo12:jit_hook_mem_read32]
    blr  x1
    ror  w0, w0, w19
    ldr  w19, [x28, #9016]
    ldr  x30, [x28, #9008]
    ret
L_jit_mem_read32_arm9_exit_1:
    b    jit_hook_mem_read32
    .size jit_mem_read32_arm9, .-jit_mem_read32_arm9

    .text
    .align 2
    .globl jit_mem_write8_arm9
    .type  jit_mem_write8_arm9, %function
jit_mem_write8_arm9:
    orr  w2, wzr, w0, lsr #11
    ldr  x2, [x9, w2, uxtw #3]
    orr  x3, xzr, x2, lsl #2
    tbnz x2, #62, L_jit_mem_write8_arm9_1_no_ram
    strb w1, [x3, w0, uxtw]
    ret
L_jit_mem_write8_arm9_1_no_ram:
    orr  w4, wzr, w0, lsr #24
    sub  w4, w4, #0x4
    cbnz w4, L_jit_mem_write8_arm9_2_not_io
    uxtb w2, w1
    ubfx w1, w0, #0, #24
    stp  w12, w13, [x9, #-224]
    stp  w14, w15, [x9, #-216]
    stp  w16, w17, [x9, #-208]
    str  w18, [x28, #8968]
    str  x30, [x28, #9064]
    mrs  x3, nzcv
    str  w3, [x28, #9044]
    ldr  x0, [x28, #8800]
    str  w12, [x28, #8848]
    bl   io9_write8
    ldr  w12, [x28, #8848]
    add  x9, x28, #0x3d0
    add  x9, x9, #0x2, lsl #12
    ldp  x10, x11, [x9, #-120]
    ldp  w12, w13, [x9, #-224]
    ldp  w14, w15, [x9, #-216]
    ldp  w16, w17, [x9, #-208]
    ldr  w18, [x28, #8968]
    ldr  x30, [x28, #9064]
    ldr  w0, [x28, #9044]
    msr  nzcv, x0
    ldr  w0, [x28, #8872]
    cbnz w0, L_jit_mem_write8_arm9_exit_1
    ret
L_jit_mem_write8_arm9_2_not_io:
    tbnz x2, #63, L_jit_mem_write8_arm9_4_watched
    mov  w2, w1
    mov  w1, w0
    mov  x0, x9
    stp  w12, w13, [x9, #-224]
    stp  w14, w15, [x9, #-216]
    stp  w16, w17, [x9, #-208]
    str  w18, [x28, #8968]
    str  x30, [x28, #9064]
    mrs  x3, nzcv
    str  w3, [x28, #9044]
    bl   bus_write8_slow
L_jit_mem_write8_arm9_3:
    add  x9, x28, #0x3d0
    add  x9, x9, #0x2, lsl #12
    ldp  x10, x11, [x9, #-120]
    ldp  w12, w13, [x9, #-224]
    ldp  w14, w15, [x9, #-216]
    ldp  w16, w17, [x9, #-208]
    ldr  w18, [x28, #8968]
    ldr  x30, [x28, #9064]
    ldr  w3, [x28, #9044]
    msr  nzcv, x3
    ret
L_jit_mem_write8_arm9_4_watched:
    ldrb w2, [x3, w0, uxtw]
    uxtb w1, w1
    sub  w4, w1, w2
    cbnz w4, L_jit_mem_write8_arm9_5_changed
    ret
L_jit_mem_write8_arm9_5_changed:
    strb w1, [x3, w0, uxtw]
    mov  w1, w0
    str  w0, [x28, #9012]
    mov  x0, x28
    stp  w12, w13, [x9, #-224]
    stp  w14, w15, [x9, #-216]
    stp  w16, w17, [x9, #-208]
    str  w18, [x28, #8968]
    str  x30, [x28, #9064]
    mrs  x4, nzcv
    str  w4, [x28, #9044]
    bl   jit_watch_write8
    cbz  w0, L_jit_mem_write8_arm9_3
    ldr  x0, [x28, #9064]
    ldr  x1, [x28, #9056]
    bl   jit_host_to_ds_addr
    str  w0, [x28, #9148]
    mov  x0, x28
    ldr  w1, [x28, #9012]
    bl   jit_cache_flush
    ldr  w1, [x28, #9148]
    mov  x0, x28
    bl   jit_cache_lookup_or_compile
    add  x9, x28, #0x3d0
    add  x9, x9, #0x2, lsl #12
    ldp  x10, x11, [x9, #-120]
    ldp  w12, w13, [x9, #-224]
    ldp  w14, w15, [x9, #-216]
    ldp  w16, w17, [x9, #-208]
    ldr  w18, [x28, #8968]
    ldr  w1, [x28, #9044]
    msr  nzcv, x1
    br   x0
L_jit_mem_write8_arm9_exit_1:
    b    jit_mem_write_result_dispatch
    .size jit_mem_write8_arm9, .-jit_mem_write8_arm9

    .text
    .align 2
    .globl jit_mem_write16_arm9
    .type  jit_mem_write16_arm9, %function
jit_mem_write16_arm9:
    orr  w2, wzr, w0, lsr #11
    and  w0, w0, #0xfffffffe
    ldr  x2, [x9, w2, uxtw #3]
    orr  x3, xzr, x2, lsl #2
    tbnz x2, #62, L_jit_mem_write16_arm9_1_no_ram
    strh w1, [x3, w0, uxtw]
    ret
L_jit_mem_write16_arm9_1_no_ram:
    orr  w4, wzr, w0, lsr #24
    sub  w4, w4, #0x4
    cbnz w4, L_jit_mem_write16_arm9_2_not_io
    uxth w2, w1
    ubfx w1, w0, #0, #24
    stp  w12, w13, [x9, #-224]
    stp  w14, w15, [x9, #-216]
    stp  w16, w17, [x9, #-208]
    str  w18, [x28, #8968]
    str  x30, [x28, #9064]
    mrs  x3, nzcv
    str  w3, [x28, #9044]
    ldr  x0, [x28, #8800]
    str  w12, [x28, #8848]
    bl   io9_write16
    ldr  w12, [x28, #8848]
    add  x9, x28, #0x3d0
    add  x9, x9, #0x2, lsl #12
    ldp  x10, x11, [x9, #-120]
    ldp  w12, w13, [x9, #-224]
    ldp  w14, w15, [x9, #-216]
    ldp  w16, w17, [x9, #-208]
    ldr  w18, [x28, #8968]
    ldr  x30, [x28, #9064]
    ldr  w0, [x28, #9044]
    msr  nzcv, x0
    ldr  w0, [x28, #8872]
    cbnz w0, L_jit_mem_write16_arm9_exit_1
    ret
L_jit_mem_write16_arm9_2_not_io:
    tbnz x2, #63, L_jit_mem_write16_arm9_4_watched
    mov  w2, w1
    mov  w1, w0
    mov  x0, x9
    stp  w12, w13, [x9, #-224]
    stp  w14, w15, [x9, #-216]
    stp  w16, w17, [x9, #-208]
    str  w18, [x28, #8968]
    str  x30, [x28, #9064]
    mrs  x3, nzcv
    str  w3, [x28, #9044]
    bl   bus_write16_slow
L_jit_mem_write16_arm9_3:
    add  x9, x28, #0x3d0
    add  x9, x9, #0x2, lsl #12
    ldp  x10, x11, [x9, #-120]
    ldp  w12, w13, [x9, #-224]
    ldp  w14, w15, [x9, #-216]
    ldp  w16, w17, [x9, #-208]
    ldr  w18, [x28, #8968]
    ldr  x30, [x28, #9064]
    ldr  w3, [x28, #9044]
    msr  nzcv, x3
    ret
L_jit_mem_write16_arm9_4_watched:
    ldrh w2, [x3, w0, uxtw]
    uxth w1, w1
    sub  w4, w1, w2
    cbnz w4, L_jit_mem_write16_arm9_5_changed
    ret
L_jit_mem_write16_arm9_5_changed:
    strh w1, [x3, w0, uxtw]
    mov  w1, w0
    str  w0, [x28, #9012]
    mov  x0, x28
    stp  w12, w13, [x9, #-224]
    stp  w14, w15, [x9, #-216]
    stp  w16, w17, [x9, #-208]
    str  w18, [x28, #8968]
    str  x30, [x28, #9064]
    mrs  x4, nzcv
    str  w4, [x28, #9044]
    bl   jit_watch_write16
    cbz  w0, L_jit_mem_write16_arm9_3
    ldr  x0, [x28, #9064]
    ldr  x1, [x28, #9056]
    bl   jit_host_to_ds_addr
    str  w0, [x28, #9148]
    mov  x0, x28
    ldr  w1, [x28, #9012]
    bl   jit_cache_flush
    ldr  w1, [x28, #9148]
    mov  x0, x28
    bl   jit_cache_lookup_or_compile
    add  x9, x28, #0x3d0
    add  x9, x9, #0x2, lsl #12
    ldp  x10, x11, [x9, #-120]
    ldp  w12, w13, [x9, #-224]
    ldp  w14, w15, [x9, #-216]
    ldp  w16, w17, [x9, #-208]
    ldr  w18, [x28, #8968]
    ldr  w1, [x28, #9044]
    msr  nzcv, x1
    br   x0
L_jit_mem_write16_arm9_exit_1:
    b    jit_mem_write_result_dispatch
    .size jit_mem_write16_arm9, .-jit_mem_write16_arm9

    .text
    .align 2
    .globl jit_mem_write32_arm9
    .type  jit_mem_write32_arm9, %function
jit_mem_write32_arm9:
    orr  w2, wzr, w0, lsr #11
    and  w0, w0, #0xfffffffc
    ldr  x2, [x9, w2, uxtw #3]
    orr  x3, xzr, x2, lsl #2
    tbnz x2, #62, 1f
    str  w1, [x3, w0, uxtw]
    ret
1:
    orr  w4, wzr, w0, lsr #24
    sub  w4, w4, #0x4
    cbnz w4, 4f
    mov  w2, w1
    ubfx w1, w0, #0, #24
    stp  w12, w13, [x9, #-224]
    stp  w14, w15, [x9, #-216]
    stp  w16, w17, [x9, #-208]
    str  w18, [x28, #8968]
    str  x30, [x28, #9064]
    mrs  x3, nzcv
    str  w3, [x28, #9044]
    sub  w3, w1, #0x400
    cmp  w3, #0x1fc
    b.hi 3f
    ldr  x0, [x28, #8808]
    cmp  w3, #0x40
    b.ge 2f
    mov  w1, w2
    bl   gpu3d_gxfifo_push_word
    add  x9, x28, #0x3d0
    add  x9, x9, #0x2, lsl #12
    ldp  x10, x11, [x9, #-120]
    ldp  w12, w13, [x9, #-224]
    ldp  w14, w15, [x9, #-216]
    ldp  w16, w17, [x9, #-208]
    ldr  w18, [x28, #8968]
    ldr  x30, [x28, #9064]
    ldr  w3, [x28, #9044]
    msr  nzcv, x3
    ret
2:
    orr  w1, wzr, w1, lsr #2
    bl   gpu3d_gxfifo_pack_command
    add  x9, x28, #0x3d0
    add  x9, x9, #0x2, lsl #12
    ldp  x10, x11, [x9, #-120]
    ldp  w12, w13, [x9, #-224]
    ldp  w14, w15, [x9, #-216]
    ldp  w16, w17, [x9, #-208]
    ldr  w18, [x28, #8968]
    ldr  x30, [x28, #9064]
    ldr  w3, [x28, #9044]
    msr  nzcv, x3
    ldr  w0, [x28, #8872]
    cbnz w0, L_jit_mem_write32_arm9_exit_1
    ret
3:
    ldr  x0, [x28, #8800]
    str  w12, [x28, #8848]
    bl   io9_write32
    ldr  w12, [x28, #8848]
    add  x9, x28, #0x3d0
    add  x9, x9, #0x2, lsl #12
    ldp  x10, x11, [x9, #-120]
    ldp  w12, w13, [x9, #-224]
    ldp  w14, w15, [x9, #-216]
    ldp  w16, w17, [x9, #-208]
    ldr  w18, [x28, #8968]
    ldr  x30, [x28, #9064]
    ldr  w0, [x28, #9044]
    msr  nzcv, x0
    ldr  w0, [x28, #8872]
    cbnz w0, L_jit_mem_write32_arm9_exit_1
    ret
4:
    tbnz x2, #63, 5f
    mov  w2, w1
    mov  w1, w0
    mov  x0, x9
    stp  w12, w13, [x9, #-224]
    stp  w14, w15, [x9, #-216]
    stp  w16, w17, [x9, #-208]
    str  w18, [x28, #8968]
    str  x30, [x28, #9064]
    mrs  x3, nzcv
    str  w3, [x28, #9044]
    bl   bus_write32_slow
L_jit_mem_write32_arm9_1:
    add  x9, x28, #0x3d0
    add  x9, x9, #0x2, lsl #12
    ldp  x10, x11, [x9, #-120]
    ldp  w12, w13, [x9, #-224]
    ldp  w14, w15, [x9, #-216]
    ldp  w16, w17, [x9, #-208]
    ldr  w18, [x28, #8968]
    ldr  x30, [x28, #9064]
    ldr  w3, [x28, #9044]
    msr  nzcv, x3
    ret
5:
    ldr  w2, [x3, w0, uxtw]
    lsr  w1, w1, #0
    sub  w4, w1, w2
    cbnz w4, 6f
    ret
6:
    str  w1, [x3, w0, uxtw]
    mov  w1, w0
    str  w0, [x28, #9012]
    mov  x0, x28
    stp  w12, w13, [x9, #-224]
    stp  w14, w15, [x9, #-216]
    stp  w16, w17, [x9, #-208]
    str  w18, [x28, #8968]
    str  x30, [x28, #9064]
    mrs  x4, nzcv
    str  w4, [x28, #9044]
    bl   jit_watch_write32
    cbz  w0, L_jit_mem_write32_arm9_1
    ldr  x0, [x28, #9064]
    ldr  x1, [x28, #9056]
    bl   jit_host_to_ds_addr
    str  w0, [x28, #9148]
    mov  x0, x28
    ldr  w1, [x28, #9012]
    bl   jit_cache_flush
    ldr  w1, [x28, #9148]
    mov  x0, x28
    bl   jit_cache_lookup_or_compile
    add  x9, x28, #0x3d0
    add  x9, x9, #0x2, lsl #12
    ldp  x10, x11, [x9, #-120]
    ldp  w12, w13, [x9, #-224]
    ldp  w14, w15, [x9, #-216]
    ldp  w16, w17, [x9, #-208]
    ldr  w18, [x28, #8968]
    ldr  w1, [x28, #9044]
    msr  nzcv, x1
    br   x0
L_jit_mem_write32_arm9_exit_1:
    b    jit_mem_write_result_dispatch
    .size jit_mem_write32_arm9, .-jit_mem_write32_arm9

    .text
    .align 2
    .globl jit_mem_write8_arm7
    .type  jit_mem_write8_arm7, %function
jit_mem_write8_arm7:
    orr  w2, wzr, w0, lsr #11
    ldr  x2, [x9, w2, uxtw #3]
    orr  x3, xzr, x2, lsl #2
    tbnz x2, #62, L_jit_mem_write8_arm7_1_no_ram
    strb w1, [x3, w0, uxtw]
    ret
L_jit_mem_write8_arm7_1_no_ram:
    orr  w4, wzr, w0, lsr #24
    sub  w4, w4, #0x4
    cbnz w4, L_jit_mem_write8_arm7_2_not_io
    uxtb w2, w1
    ubfx w1, w0, #0, #24
    stp  w12, w13, [x9, #-224]
    stp  w14, w15, [x9, #-216]
    stp  w16, w17, [x9, #-208]
    str  w18, [x28, #8968]
    str  x30, [x28, #9064]
    mrs  x3, nzcv
    str  w3, [x28, #9044]
    ldr  x0, [x28, #8800]
    str  w12, [x28, #8848]
    bl   io7_write8
    ldr  w12, [x28, #8848]
    add  x9, x28, #0x3d0
    add  x9, x9, #0x2, lsl #12
    ldp  x10, x11, [x9, #-120]
    ldp  w12, w13, [x9, #-224]
    ldp  w14, w15, [x9, #-216]
    ldp  w16, w17, [x9, #-208]
    ldr  w18, [x28, #8968]
    ldr  x30, [x28, #9064]
    ldr  w0, [x28, #9044]
    msr  nzcv, x0
    ldr  w0, [x28, #8872]
    cbnz w0, L_jit_mem_write8_arm7_exit_1
    ret
L_jit_mem_write8_arm7_2_not_io:
    tbnz x2, #63, L_jit_mem_write8_arm7_4_watched
    mov  w2, w1
    mov  w1, w0
    mov  x0, x9
    stp  w12, w13, [x9, #-224]
    stp  w14, w15, [x9, #-216]
    stp  w16, w17, [x9, #-208]
    str  w18, [x28, #8968]
    str  x30, [x28, #9064]
    mrs  x3, nzcv
    str  w3, [x28, #9044]
    bl   bus_write8_slow
L_jit_mem_write8_arm7_3:
    add  x9, x28, #0x3d0
    add  x9, x9, #0x2, lsl #12
    ldp  x10, x11, [x9, #-120]
    ldp  w12, w13, [x9, #-224]
    ldp  w14, w15, [x9, #-216]
    ldp  w16, w17, [x9, #-208]
    ldr  w18, [x28, #8968]
    ldr  x30, [x28, #9064]
    ldr  w3, [x28, #9044]
    msr  nzcv, x3
    ret
L_jit_mem_write8_arm7_4_watched:
    ldrb w2, [x3, w0, uxtw]
    uxtb w1, w1
    sub  w4, w1, w2
    cbnz w4, L_jit_mem_write8_arm7_5_changed
    ret
L_jit_mem_write8_arm7_5_changed:
    strb w1, [x3, w0, uxtw]
    mov  w1, w0
    str  w0, [x28, #9012]
    mov  x0, x28
    stp  w12, w13, [x9, #-224]
    stp  w14, w15, [x9, #-216]
    stp  w16, w17, [x9, #-208]
    str  w18, [x28, #8968]
    str  x30, [x28, #9064]
    mrs  x4, nzcv
    str  w4, [x28, #9044]
    bl   jit_watch_write8
    cbz  w0, L_jit_mem_write8_arm7_3
    ldr  x0, [x28, #9064]
    ldr  x1, [x28, #9056]
    bl   jit_host_to_ds_addr
    str  w0, [x28, #9148]
    mov  x0, x28
    ldr  w1, [x28, #9012]
    bl   jit_cache_flush
    ldr  w1, [x28, #9148]
    mov  x0, x28
    bl   jit_cache_lookup_or_compile
    add  x9, x28, #0x3d0
    add  x9, x9, #0x2, lsl #12
    ldp  x10, x11, [x9, #-120]
    ldp  w12, w13, [x9, #-224]
    ldp  w14, w15, [x9, #-216]
    ldp  w16, w17, [x9, #-208]
    ldr  w18, [x28, #8968]
    ldr  w1, [x28, #9044]
    msr  nzcv, x1
    br   x0
L_jit_mem_write8_arm7_exit_1:
    b    jit_mem_write_result_dispatch
    .size jit_mem_write8_arm7, .-jit_mem_write8_arm7

    .text
    .align 2
    .globl jit_mem_write16_arm7
    .type  jit_mem_write16_arm7, %function
jit_mem_write16_arm7:
    orr  w2, wzr, w0, lsr #11
    and  w0, w0, #0xfffffffe
    ldr  x2, [x9, w2, uxtw #3]
    orr  x3, xzr, x2, lsl #2
    tbnz x2, #62, L_jit_mem_write16_arm7_1_no_ram
    strh w1, [x3, w0, uxtw]
    ret
L_jit_mem_write16_arm7_1_no_ram:
    orr  w4, wzr, w0, lsr #24
    sub  w4, w4, #0x4
    cbnz w4, L_jit_mem_write16_arm7_2_not_io
    uxth w2, w1
    ubfx w1, w0, #0, #24
    stp  w12, w13, [x9, #-224]
    stp  w14, w15, [x9, #-216]
    stp  w16, w17, [x9, #-208]
    str  w18, [x28, #8968]
    str  x30, [x28, #9064]
    mrs  x3, nzcv
    str  w3, [x28, #9044]
    ldr  x0, [x28, #8800]
    str  w12, [x28, #8848]
    bl   io7_write16
    ldr  w12, [x28, #8848]
    add  x9, x28, #0x3d0
    add  x9, x9, #0x2, lsl #12
    ldp  x10, x11, [x9, #-120]
    ldp  w12, w13, [x9, #-224]
    ldp  w14, w15, [x9, #-216]
    ldp  w16, w17, [x9, #-208]
    ldr  w18, [x28, #8968]
    ldr  x30, [x28, #9064]
    ldr  w0, [x28, #9044]
    msr  nzcv, x0
    ldr  w0, [x28, #8872]
    cbnz w0, L_jit_mem_write16_arm7_exit_1
    ret
L_jit_mem_write16_arm7_2_not_io:
    tbnz x2, #63, L_jit_mem_write16_arm7_4_watched
    mov  w2, w1
    mov  w1, w0
    mov  x0, x9
    stp  w12, w13, [x9, #-224]
    stp  w14, w15, [x9, #-216]
    stp  w16, w17, [x9, #-208]
    str  w18, [x28, #8968]
    str  x30, [x28, #9064]
    mrs  x3, nzcv
    str  w3, [x28, #9044]
    bl   bus_write16_slow
L_jit_mem_write16_arm7_3:
    add  x9, x28, #0x3d0
    add  x9, x9, #0x2, lsl #12
    ldp  x10, x11, [x9, #-120]
    ldp  w12, w13, [x9, #-224]
    ldp  w14, w15, [x9, #-216]
    ldp  w16, w17, [x9, #-208]
    ldr  w18, [x28, #8968]
    ldr  x30, [x28, #9064]
    ldr  w3, [x28, #9044]
    msr  nzcv, x3
    ret
L_jit_mem_write16_arm7_4_watched:
    ldrh w2, [x3, w0, uxtw]
    uxth w1, w1
    sub  w4, w1, w2
    cbnz w4, L_jit_mem_write16_arm7_5_changed
    ret
L_jit_mem_write16_arm7_5_changed:
    strh w1, [x3, w0, uxtw]
    mov  w1, w0
    str  w0, [x28, #9012]
    mov  x0, x28
    stp  w12, w13, [x9, #-224]
    stp  w14, w15, [x9, #-216]
    stp  w16, w17, [x9, #-208]
    str  w18, [x28, #8968]
    str  x30, [x28, #9064]
    mrs  x4, nzcv
    str  w4, [x28, #9044]
    bl   jit_watch_write16
    cbz  w0, L_jit_mem_write16_arm7_3
    ldr  x0, [x28, #9064]
    ldr  x1, [x28, #9056]
    bl   jit_host_to_ds_addr
    str  w0, [x28, #9148]
    mov  x0, x28
    ldr  w1, [x28, #9012]
    bl   jit_cache_flush
    ldr  w1, [x28, #9148]
    mov  x0, x28
    bl   jit_cache_lookup_or_compile
    add  x9, x28, #0x3d0
    add  x9, x9, #0x2, lsl #12
    ldp  x10, x11, [x9, #-120]
    ldp  w12, w13, [x9, #-224]
    ldp  w14, w15, [x9, #-216]
    ldp  w16, w17, [x9, #-208]
    ldr  w18, [x28, #8968]
    ldr  w1, [x28, #9044]
    msr  nzcv, x1
    br   x0
L_jit_mem_write16_arm7_exit_1:
    b    jit_mem_write_result_dispatch
    .size jit_mem_write16_arm7, .-jit_mem_write16_arm7

    .text
    .align 2
    .globl jit_mem_write32_arm7
    .type  jit_mem_write32_arm7, %function
    .globl jit_mem_write_result_dispatch
    .type  jit_mem_write_result_dispatch, %function
    .globl jit_mem_write_irq_check
    .type  jit_mem_write_irq_check, %function
    .globl jit_ldm_read_words_slow
    .type  jit_ldm_read_words_slow, %function
jit_mem_write32_arm7:
    orr  w2, wzr, w0, lsr #11
    and  w0, w0, #0xfffffffc
    ldr  x2, [x9, w2, uxtw #3]
    orr  x3, xzr, x2, lsl #2
    tbnz x2, #62, L_jit_mem_write32_arm7_1
    str  w1, [x3, w0, uxtw]
    ret
L_jit_mem_write32_arm7_1:
    orr  w4, wzr, w0, lsr #24
    sub  w4, w4, #0x4
    cbnz w4, L_jit_mem_write32_arm7_2
    mov  w2, w1
    ubfx w1, w0, #0, #24
    stp  w12, w13, [x9, #-224]
    stp  w14, w15, [x9, #-216]
    stp  w16, w17, [x9, #-208]
    str  w18, [x28, #8968]
    str  x30, [x28, #9064]
    mrs  x3, nzcv
    str  w3, [x28, #9044]
    ldr  x0, [x28, #8800]
    str  w12, [x28, #8848]
    bl   io7_write32
    ldr  w12, [x28, #8848]
    add  x9, x28, #0x3d0
    add  x9, x9, #0x2, lsl #12
    ldp  x10, x11, [x9, #-120]
    ldp  w12, w13, [x9, #-224]
    ldp  w14, w15, [x9, #-216]
    ldp  w16, w17, [x9, #-208]
    ldr  w18, [x28, #8968]
    ldr  x30, [x28, #9064]
    ldr  w0, [x28, #9044]
    msr  nzcv, x0
    ldr  w0, [x28, #8872]
    cbnz w0, L_jit_mem_write_result_dispatch_1
    ret
L_jit_mem_write32_arm7_2:
    tbnz x2, #63, L_jit_mem_write32_arm7_4
    mov  w2, w1
    mov  w1, w0
    mov  x0, x9
    stp  w12, w13, [x9, #-224]
    stp  w14, w15, [x9, #-216]
    stp  w16, w17, [x9, #-208]
    str  w18, [x28, #8968]
    str  x30, [x28, #9064]
    mrs  x3, nzcv
    str  w3, [x28, #9044]
    bl   bus_write32_slow
L_jit_mem_write32_arm7_3:
    add  x9, x28, #0x3d0
    add  x9, x9, #0x2, lsl #12
    ldp  x10, x11, [x9, #-120]
    ldp  w12, w13, [x9, #-224]
    ldp  w14, w15, [x9, #-216]
    ldp  w16, w17, [x9, #-208]
    ldr  w18, [x28, #8968]
    ldr  x30, [x28, #9064]
    ldr  w3, [x28, #9044]
    msr  nzcv, x3
    ret
L_jit_mem_write32_arm7_4:
    ldr  w2, [x3, w0, uxtw]
    lsr  w1, w1, #0
    sub  w4, w1, w2
    cbnz w4, L_jit_mem_write32_arm7_5
    ret
L_jit_mem_write32_arm7_5:
    str  w1, [x3, w0, uxtw]
    mov  w1, w0
    str  w0, [x28, #9012]
    mov  x0, x28
    stp  w12, w13, [x9, #-224]
    stp  w14, w15, [x9, #-216]
    stp  w16, w17, [x9, #-208]
    str  w18, [x28, #8968]
    str  x30, [x28, #9064]
    mrs  x4, nzcv
    str  w4, [x28, #9044]
    bl   jit_watch_write32
    cbz  w0, L_jit_mem_write32_arm7_3
    ldr  x0, [x28, #9064]
    ldr  x1, [x28, #9056]
    bl   jit_host_to_ds_addr
    str  w0, [x28, #9148]
    mov  x0, x28
    ldr  w1, [x28, #9012]
    bl   jit_cache_flush
    ldr  w1, [x28, #9148]
    mov  x0, x28
    bl   jit_cache_lookup_or_compile
    add  x9, x28, #0x3d0
    add  x9, x9, #0x2, lsl #12
    ldp  x10, x11, [x9, #-120]
    ldp  w12, w13, [x9, #-224]
    ldp  w14, w15, [x9, #-216]
    ldp  w16, w17, [x9, #-208]
    ldr  w18, [x28, #8968]
    ldr  w1, [x28, #9044]
    msr  nzcv, x1
    br   x0
jit_mem_write_result_dispatch:
L_jit_mem_write_result_dispatch_1:
    str  wzr, [x28, #8872]
    stp  w12, w13, [x9, #-224]
    stp  w14, w15, [x9, #-216]
    stp  w16, w17, [x9, #-208]
    str  w18, [x28, #8968]
    str  x30, [x28, #9064]
    mrs  x1, nzcv
    str  w1, [x28, #9044]
    tbz  w0, #4, L_jit_mem_write_result_dispatch_2
    mov  x0, x30
    mov  x1, x11
    bl   jit_host_to_ds_addr
    add  x9, x28, #0x3d0
    add  x9, x9, #0x2, lsl #12
    ldp  x10, x11, [x9, #-120]
    ldp  w12, w13, [x9, #-224]
    ldp  w14, w15, [x9, #-216]
    ldp  w16, w17, [x9, #-208]
    ldr  w18, [x28, #8968]
    ldr  x30, [x28, #9064]
    ldr  w1, [x28, #9044]
    msr  nzcv, x1
    b    L_jit_ldm_read_words_slow_exit_1
L_jit_mem_write_result_dispatch_2:
    tbz  w0, #2, L_jit_mem_write_result_dispatch_3
    tbnz w12, #31, L_jit_mem_write_result_dispatch_3
    ldr  x1, [x28, #8792]
    ldr  w2, [x1, #16]
    sub  w2, w2, w12
    str  wzr, [x28, #8944]
    str  w2, [x1, #16]
L_jit_mem_write_result_dispatch_3:
    and  w0, w0, #0x3
    adr  x1, L_jit_mem_write_irq_check_6
    add  x1, x1, w0, uxtw #2
    br   x1
L_jit_mem_write_result_dispatch_4:
    add  x9, x28, #0x3d0
    add  x9, x9, #0x2, lsl #12
    ldp  x10, x11, [x9, #-120]
    ldp  w12, w13, [x9, #-224]
    ldp  w14, w15, [x9, #-216]
    ldp  w16, w17, [x9, #-208]
    ldr  w18, [x28, #8968]
    ldr  x30, [x28, #9064]
    ldr  w0, [x28, #9044]
    msr  nzcv, x0
    ret
L_jit_mem_write_result_dispatch_5:
    mov  x0, x30
    mov  x1, x11
    bl   jit_host_to_ds_addr
    str  w0, [x28, #9148]
    mov  x0, x28
    mov  w1, #0xffffffff
    bl   jit_cache_flush
    ldr  w1, [x28, #9148]
    mov  x0, x28
    bl   jit_cache_lookup_or_compile
    add  x9, x28, #0x3d0
    add  x9, x9, #0x2, lsl #12
    ldp  x10, x11, [x9, #-120]
    ldp  w12, w13, [x9, #-224]
    ldp  w14, w15, [x9, #-216]
    ldp  w16, w17, [x9, #-208]
    ldr  w18, [x28, #8968]
    ldr  w1, [x28, #9044]
    msr  nzcv, x1
    br   x0
jit_mem_write_irq_check:
L_jit_mem_write_irq_check_1:
    ldr  w0, [x28, #8456]
    ldr  w1, [x28, #9152]
    add  x9, x28, #0x3d0
    add  x9, x9, #0x2, lsl #12
    ldp  x10, x11, [x9, #-120]
    ldp  w12, w13, [x9, #-224]
    ldp  w14, w15, [x9, #-216]
    ldp  w16, w17, [x9, #-208]
    ldr  w18, [x28, #8968]
    ldr  x30, [x28, #9064]
    ldr  w2, [x28, #9044]
    msr  nzcv, x2
    cbnz w0, L_jit_mem_write_irq_check_2
    ret
L_jit_mem_write_irq_check_2:
    tbz  w1, #7, L_jit_mem_write_irq_check_3
    ret
L_jit_mem_write_irq_check_3:
    ldr  w2, [x28, #9144]
    mov  x0, x28
    bfxil w2, w1, #0, #28
    str  w2, [x28, #9152]
    mrs  x0, nzcv
    stp  w13, w14, [x9, #-96]
    stp  w15, w16, [x9, #-88]
    stp  w17, w18, [x9, #-80]
    stp  w19, w20, [x9, #-72]
    stp  w21, w22, [x9, #-64]
    stp  w23, w24, [x9, #-56]
    stp  w25, w26, [x9, #-48]
    stur w27, [x9, #-40]
    str  w0, [x28, #9144]
    str  w12, [x28, #8848]
    mov  x0, x30
    mov  x1, x11
    bl   jit_host_to_ds_addr
    str  w0, [x28, #9148]
    mov  x0, x28
    bl   arm_irq_raise
    ldr  x0, [x28, #8832]
    add  x9, x28, #0x3d0
    add  x9, x9, #0x2, lsl #12
    ldp  x10, x11, [x9, #-120]
    ldr  w1, [x28, #9144]
    ldp  w13, w14, [x9, #-96]
    ldp  w15, w16, [x9, #-88]
    ldp  w17, w18, [x9, #-80]
    ldp  w19, w20, [x9, #-72]
    ldp  w21, w22, [x9, #-64]
    ldp  w23, w24, [x9, #-56]
    ldp  w25, w26, [x9, #-48]
    ldur w27, [x9, #-40]
    msr  nzcv, x1
    ldr  w12, [x28, #8848]
    br   x0
L_jit_mem_write_irq_check_4:
    mov  x0, x30
    mov  x1, x11
    bl   jit_host_to_ds_addr
    str  w0, [x28, #9148]
    ldr  w0, [x28, #8456]
    ldr  w1, [x28, #9152]
    add  x9, x28, #0x3d0
    add  x9, x9, #0x2, lsl #12
    ldp  x10, x11, [x9, #-120]
    ldp  w12, w13, [x9, #-224]
    ldp  w14, w15, [x9, #-216]
    ldp  w16, w17, [x9, #-208]
    ldr  w18, [x28, #8968]
    ldr  x30, [x28, #9064]
    ldr  w2, [x28, #9044]
    msr  nzcv, x2
    cbz  w0, L_jit_mem_write_irq_check_5
    tbnz w1, #7, L_jit_mem_write_irq_check_5
    ldr  w2, [x28, #9144]
    mov  x0, x28
    bfxil w2, w1, #0, #28
    str  w2, [x28, #9152]
    mrs  x2, nzcv
    stp  w13, w14, [x9, #-96]
    stp  w15, w16, [x9, #-88]
    stp  w17, w18, [x9, #-80]
    stp  w19, w20, [x9, #-72]
    stp  w21, w22, [x9, #-64]
    stp  w23, w24, [x9, #-56]
    stp  w25, w26, [x9, #-48]
    stur w27, [x9, #-40]
    str  w2, [x28, #9144]
    str  w12, [x28, #8848]
    bl   arm_irq_raise
    mov  x0, x28
    mov  w1, #0xffffffff
    bl   jit_cache_flush
    ldr  x0, [x28, #8832]
    add  x9, x28, #0x3d0
    add  x9, x9, #0x2, lsl #12
    ldp  x10, x11, [x9, #-120]
    ldr  w2, [x28, #9144]
    ldp  w13, w14, [x9, #-96]
    ldp  w15, w16, [x9, #-88]
    ldp  w17, w18, [x9, #-80]
    ldp  w19, w20, [x9, #-72]
    ldp  w21, w22, [x9, #-64]
    ldp  w23, w24, [x9, #-56]
    ldp  w25, w26, [x9, #-48]
    ldur w27, [x9, #-40]
    msr  nzcv, x2
    ldr  w12, [x28, #8848]
    br   x0
L_jit_mem_write_irq_check_5:
    mov  x0, x28
    mov  w1, #0xffffffff
    adrp x2, :got:jit_cache_flush
    ldr  x2, [x2, :got_lo12:jit_cache_flush]
    blr  x2
    mov  x0, x28
    ldr  w1, [x28, #9148]
    adrp x2, :got:jit_cache_lookup_or_compile
    ldr  x2, [x2, :got_lo12:jit_cache_lookup_or_compile]
    blr  x2
    add  x9, x28, #0x3d0
    add  x9, x9, #0x2, lsl #12
    ldp  x10, x11, [x9, #-120]
    ldp  w12, w13, [x9, #-224]
    ldp  w14, w15, [x9, #-216]
    ldp  w16, w17, [x9, #-208]
    ldr  w18, [x28, #8968]
    ldr  w1, [x28, #9044]
    msr  nzcv, x1
    br   x0
L_jit_mem_write_irq_check_6:
    b    L_jit_mem_write_result_dispatch_4
    b    L_jit_mem_write_result_dispatch_5
    b    L_jit_mem_write_irq_check_1
    b    L_jit_mem_write_irq_check_4
jit_ldm_read_words_slow:
    str  w12, [x28, #8848]
    str  w19, [x28, #9012]
    str  x20, [x28, #9016]
    mov  w19, w0
    sub  x20, x9, #0x120
    stp  w12, w13, [x9, #-224]
    stp  w14, w15, [x9, #-216]
    stp  w16, w17, [x9, #-208]
    str  w18, [x28, #8968]
    str  x30, [x28, #9064]
    mrs  x0, nzcv
    str  w0, [x28, #9044]
L_jit_ldm_read_words_slow_1:
    mov  w1, w19
    add  x0, x28, #0x3d0
    add  x0, x0, #0x2, lsl #12
    add  w19, w19, #0x4
    bl   bus_read32_slow
    str  w0, [x20], #4
    subs w21, w21, #0x1
    b.ne L_jit_ldm_read_words_slow_1
    add  x9, x28, #0x3d0
    add  x9, x9, #0x2, lsl #12
    ldp  x10, x11, [x9, #-120]
    ldp  w12, w13, [x9, #-224]
    ldp  w14, w15, [x9, #-216]
    ldp  w16, w17, [x9, #-208]
    ldr  w18, [x28, #8968]
    ldr  x30, [x28, #9064]
    ldr  w0, [x28, #9044]
    msr  nzcv, x0
    ldr  w19, [x28, #9012]
    ldr  x20, [x28, #9016]
    ldr  w21, [x28, #9024]
    sub  x0, x9, #0x120
    ret
L_jit_ldm_read_words_slow_exit_1:
    b    jit_exit_store_pc_flags
    .size jit_mem_write32_arm7, .-jit_mem_write32_arm7
    .size jit_mem_write_result_dispatch, .-jit_mem_write_result_dispatch
    .size jit_mem_write_irq_check, .-jit_mem_write_irq_check
    .size jit_ldm_read_words_slow, .-jit_ldm_read_words_slow

    .text
    .align 2
    .globl jit_ldm_1
    .type  jit_ldm_1, %function
jit_ldm_1:
    orr  w1, wzr, w0, lsr #11
    and  w0, w0, #0xfffffffc
    ldr  x1, [x9, w1, uxtw #3]
    orr  x1, xzr, x1, lsl #2
    cbz  x1, L_jit_ldm_1_1
    add  x0, x1, w0, uxtw
    ret
L_jit_ldm_1_1:
    str  w21, [x28, #9024]
    mov  w21, #0x1
    adrp x1, :got:jit_ldm_read_words_slow
    ldr  x1, [x1, :got_lo12:jit_ldm_read_words_slow]
    br   x1
    .size jit_ldm_1, .-jit_ldm_1

    .text
    .align 2
    .globl jit_ldm_2
    .type  jit_ldm_2, %function
jit_ldm_2:
    orr  w1, wzr, w0, lsr #11
    and  w0, w0, #0xfffffffc
    ldr  x1, [x9, w1, uxtw #3]
    orr  x1, xzr, x1, lsl #2
    cbz  x1, L_jit_ldm_2_1
    add  x0, x1, w0, uxtw
    ret
L_jit_ldm_2_1:
    str  w21, [x28, #9024]
    mov  w21, #0x2
    adrp x1, :got:jit_ldm_read_words_slow
    ldr  x1, [x1, :got_lo12:jit_ldm_read_words_slow]
    br   x1
    .size jit_ldm_2, .-jit_ldm_2

    .text
    .align 2
    .globl jit_ldm_3
    .type  jit_ldm_3, %function
jit_ldm_3:
    orr  w1, wzr, w0, lsr #11
    and  w0, w0, #0xfffffffc
    ldr  x1, [x9, w1, uxtw #3]
    orr  x1, xzr, x1, lsl #2
    cbz  x1, L_jit_ldm_3_1
    add  x0, x1, w0, uxtw
    ret
L_jit_ldm_3_1:
    str  w21, [x28, #9024]
    mov  w21, #0x3
    adrp x1, :got:jit_ldm_read_words_slow
    ldr  x1, [x1, :got_lo12:jit_ldm_read_words_slow]
    br   x1
    .size jit_ldm_3, .-jit_ldm_3

    .text
    .align 2
    .globl jit_ldm_4
    .type  jit_ldm_4, %function
jit_ldm_4:
    orr  w1, wzr, w0, lsr #11
    and  w0, w0, #0xfffffffc
    ldr  x1, [x9, w1, uxtw #3]
    orr  x1, xzr, x1, lsl #2
    cbz  x1, L_jit_ldm_4_1
    add  x0, x1, w0, uxtw
    ret
L_jit_ldm_4_1:
    str  w21, [x28, #9024]
    mov  w21, #0x4
    adrp x1, :got:jit_ldm_read_words_slow
    ldr  x1, [x1, :got_lo12:jit_ldm_read_words_slow]
    br   x1
    .size jit_ldm_4, .-jit_ldm_4

    .text
    .align 2
    .globl jit_ldm_5
    .type  jit_ldm_5, %function
jit_ldm_5:
    orr  w1, wzr, w0, lsr #11
    and  w0, w0, #0xfffffffc
    ldr  x1, [x9, w1, uxtw #3]
    orr  x1, xzr, x1, lsl #2
    cbz  x1, L_jit_ldm_5_1
    add  x0, x1, w0, uxtw
    ret
L_jit_ldm_5_1:
    str  w21, [x28, #9024]
    mov  w21, #0x5
    adrp x1, :got:jit_ldm_read_words_slow
    ldr  x1, [x1, :got_lo12:jit_ldm_read_words_slow]
    br   x1
    .size jit_ldm_5, .-jit_ldm_5

    .text
    .align 2
    .globl jit_ldm_6
    .type  jit_ldm_6, %function
jit_ldm_6:
    orr  w1, wzr, w0, lsr #11
    and  w0, w0, #0xfffffffc
    ldr  x1, [x9, w1, uxtw #3]
    orr  x1, xzr, x1, lsl #2
    cbz  x1, L_jit_ldm_6_1
    add  x0, x1, w0, uxtw
    ret
L_jit_ldm_6_1:
    str  w21, [x28, #9024]
    mov  w21, #0x6
    adrp x1, :got:jit_ldm_read_words_slow
    ldr  x1, [x1, :got_lo12:jit_ldm_read_words_slow]
    br   x1
    .size jit_ldm_6, .-jit_ldm_6

    .text
    .align 2
    .globl jit_ldm_7
    .type  jit_ldm_7, %function
jit_ldm_7:
    orr  w1, wzr, w0, lsr #11
    and  w0, w0, #0xfffffffc
    ldr  x1, [x9, w1, uxtw #3]
    orr  x1, xzr, x1, lsl #2
    cbz  x1, L_jit_ldm_7_1
    add  x0, x1, w0, uxtw
    ret
L_jit_ldm_7_1:
    str  w21, [x28, #9024]
    mov  w21, #0x7
    adrp x1, :got:jit_ldm_read_words_slow
    ldr  x1, [x1, :got_lo12:jit_ldm_read_words_slow]
    br   x1
    .size jit_ldm_7, .-jit_ldm_7

    .text
    .align 2
    .globl jit_ldm_8
    .type  jit_ldm_8, %function
jit_ldm_8:
    orr  w1, wzr, w0, lsr #11
    and  w0, w0, #0xfffffffc
    ldr  x1, [x9, w1, uxtw #3]
    orr  x1, xzr, x1, lsl #2
    cbz  x1, L_jit_ldm_8_1
    add  x0, x1, w0, uxtw
    ret
L_jit_ldm_8_1:
    str  w21, [x28, #9024]
    mov  w21, #0x8
    adrp x1, :got:jit_ldm_read_words_slow
    ldr  x1, [x1, :got_lo12:jit_ldm_read_words_slow]
    br   x1
    .size jit_ldm_8, .-jit_ldm_8

    .text
    .align 2
    .globl jit_ldm_9
    .type  jit_ldm_9, %function
jit_ldm_9:
    orr  w1, wzr, w0, lsr #11
    and  w0, w0, #0xfffffffc
    ldr  x1, [x9, w1, uxtw #3]
    orr  x1, xzr, x1, lsl #2
    cbz  x1, L_jit_ldm_9_1
    add  x0, x1, w0, uxtw
    ret
L_jit_ldm_9_1:
    str  w21, [x28, #9024]
    mov  w21, #0x9
    adrp x1, :got:jit_ldm_read_words_slow
    ldr  x1, [x1, :got_lo12:jit_ldm_read_words_slow]
    br   x1
    .size jit_ldm_9, .-jit_ldm_9

    .text
    .align 2
    .globl jit_ldm_10
    .type  jit_ldm_10, %function
jit_ldm_10:
    orr  w1, wzr, w0, lsr #11
    and  w0, w0, #0xfffffffc
    ldr  x1, [x9, w1, uxtw #3]
    orr  x1, xzr, x1, lsl #2
    cbz  x1, L_jit_ldm_10_1
    add  x0, x1, w0, uxtw
    ret
L_jit_ldm_10_1:
    str  w21, [x28, #9024]
    mov  w21, #0xa
    adrp x1, :got:jit_ldm_read_words_slow
    ldr  x1, [x1, :got_lo12:jit_ldm_read_words_slow]
    br   x1
    .size jit_ldm_10, .-jit_ldm_10

    .text
    .align 2
    .globl jit_ldm_11
    .type  jit_ldm_11, %function
jit_ldm_11:
    orr  w1, wzr, w0, lsr #11
    and  w0, w0, #0xfffffffc
    ldr  x1, [x9, w1, uxtw #3]
    orr  x1, xzr, x1, lsl #2
    cbz  x1, L_jit_ldm_11_1
    add  x0, x1, w0, uxtw
    ret
L_jit_ldm_11_1:
    str  w21, [x28, #9024]
    mov  w21, #0xb
    adrp x1, :got:jit_ldm_read_words_slow
    ldr  x1, [x1, :got_lo12:jit_ldm_read_words_slow]
    br   x1
    .size jit_ldm_11, .-jit_ldm_11

    .text
    .align 2
    .globl jit_ldm_12
    .type  jit_ldm_12, %function
jit_ldm_12:
    orr  w1, wzr, w0, lsr #11
    and  w0, w0, #0xfffffffc
    ldr  x1, [x9, w1, uxtw #3]
    orr  x1, xzr, x1, lsl #2
    cbz  x1, L_jit_ldm_12_1
    add  x0, x1, w0, uxtw
    ret
L_jit_ldm_12_1:
    str  w21, [x28, #9024]
    mov  w21, #0xc
    adrp x1, :got:jit_ldm_read_words_slow
    ldr  x1, [x1, :got_lo12:jit_ldm_read_words_slow]
    br   x1
    .size jit_ldm_12, .-jit_ldm_12

    .text
    .align 2
    .globl jit_ldm_13
    .type  jit_ldm_13, %function
jit_ldm_13:
    orr  w1, wzr, w0, lsr #11
    and  w0, w0, #0xfffffffc
    ldr  x1, [x9, w1, uxtw #3]
    orr  x1, xzr, x1, lsl #2
    cbz  x1, L_jit_ldm_13_1
    add  x0, x1, w0, uxtw
    ret
L_jit_ldm_13_1:
    str  w21, [x28, #9024]
    mov  w21, #0xd
    adrp x1, :got:jit_ldm_read_words_slow
    ldr  x1, [x1, :got_lo12:jit_ldm_read_words_slow]
    br   x1
    .size jit_ldm_13, .-jit_ldm_13

    .text
    .align 2
    .globl jit_ldm_14
    .type  jit_ldm_14, %function
jit_ldm_14:
    orr  w1, wzr, w0, lsr #11
    and  w0, w0, #0xfffffffc
    ldr  x1, [x9, w1, uxtw #3]
    orr  x1, xzr, x1, lsl #2
    cbz  x1, L_jit_ldm_14_1
    add  x0, x1, w0, uxtw
    ret
L_jit_ldm_14_1:
    str  w21, [x28, #9024]
    mov  w21, #0xe
    adrp x1, :got:jit_ldm_read_words_slow
    ldr  x1, [x1, :got_lo12:jit_ldm_read_words_slow]
    br   x1
    .size jit_ldm_14, .-jit_ldm_14

    .text
    .align 2
    .globl jit_ldm_15
    .type  jit_ldm_15, %function
jit_ldm_15:
    orr  w1, wzr, w0, lsr #11
    and  w0, w0, #0xfffffffc
    ldr  x1, [x9, w1, uxtw #3]
    orr  x1, xzr, x1, lsl #2
    cbz  x1, L_jit_ldm_15_1
    add  x0, x1, w0, uxtw
    ret
L_jit_ldm_15_1:
    str  w21, [x28, #9024]
    mov  w21, #0xf
    adrp x1, :got:jit_ldm_read_words_slow
    ldr  x1, [x1, :got_lo12:jit_ldm_read_words_slow]
    br   x1
    .size jit_ldm_15, .-jit_ldm_15

    .text
    .align 2
    .globl jit_ldm_16
    .type  jit_ldm_16, %function
    .globl jit_stm_write_words_slow
    .type  jit_stm_write_words_slow, %function
jit_ldm_16:
    orr  w1, wzr, w0, lsr #11
    and  w0, w0, #0xfffffffc
    ldr  x1, [x9, w1, uxtw #3]
    orr  x1, xzr, x1, lsl #2
    cbz  x1, L_jit_ldm_16_1
    add  x0, x1, w0, uxtw
    ret
L_jit_ldm_16_1:
    str  w21, [x28, #9024]
    mov  w21, #0x10
    b    L_jit_stm_write_words_slow_exit_1
jit_stm_write_words_slow:
L_jit_stm_write_words_slow_1:
    str  w12, [x28, #8848]
    str  w19, [x28, #9012]
    str  x20, [x28, #9016]
    mov  w19, w0
    sub  x20, x9, #0x120
    stp  w12, w13, [x9, #-224]
    stp  w14, w15, [x9, #-216]
    stp  w16, w17, [x9, #-208]
    str  w18, [x28, #8968]
    str  x30, [x28, #9064]
    mrs  x0, nzcv
    str  w0, [x28, #9044]
L_jit_stm_write_words_slow_2:
    ldr  w2, [x20], #4
    mov  w1, w19
    add  x0, x28, #0x3d0
    add  x0, x0, #0x2, lsl #12
    add  w19, w19, #0x4
    bl   bus_write32_slow
    subs w21, w21, #0x1
    b.ne L_jit_stm_write_words_slow_2
    add  x9, x28, #0x3d0
    add  x9, x9, #0x2, lsl #12
    ldp  x10, x11, [x9, #-120]
    ldp  w12, w13, [x9, #-224]
    ldp  w14, w15, [x9, #-216]
    ldp  w16, w17, [x9, #-208]
    ldr  w18, [x28, #8968]
    ldr  x30, [x28, #9064]
    ldr  w0, [x28, #9044]
    msr  nzcv, x0
    ldr  w0, [x28, #8872]
    ldr  w19, [x28, #9012]
    ldr  x20, [x28, #9016]
    ldr  w21, [x28, #9024]
    cbnz w0, L_jit_stm_write_words_slow_exit_2
    ret
L_jit_stm_write_words_slow_exit_1:
    b    jit_ldm_read_words_slow
L_jit_stm_write_words_slow_exit_2:
    b    jit_mem_write_result_dispatch
    .size jit_ldm_16, .-jit_ldm_16
    .size jit_stm_write_words_slow, .-jit_stm_write_words_slow

    .text
    .align 2
    .globl jit_stm_1
    .type  jit_stm_1, %function
jit_stm_1:
    orr  w1, wzr, w0, lsr #11
    and  w0, w0, #0xfffffffc
    ldr  x1, [x9, w1, uxtw #3]
    orr  x2, xzr, x1, lsl #2
    tbnz x1, #62, L_jit_stm_1_1
    add  x1, x2, w0, uxtw
    sub  x2, x9, #0x120
    ldr  w3, [x2]
    str  w3, [x1]
    ret
L_jit_stm_1_1:
    tbnz x1, #63, L_jit_stm_1_2
    str  w21, [x28, #9024]
    mov  w21, #0x1
    b    L_jit_stm_1_exit_1
L_jit_stm_1_2:
    add  x1, x2, w0, uxtw
    sub  x2, x9, #0x120
    ldr  w3, [x2]
    str  w3, [x1]
    stp  w12, w13, [x9, #-224]
    stp  w14, w15, [x9, #-216]
    stp  w16, w17, [x9, #-208]
    str  w18, [x28, #8968]
    str  x30, [x28, #9064]
    mrs  x3, nzcv
    str  w3, [x28, #9044]
    mov  w1, w0
    str  w1, [x28, #9012]
    mov  x0, x28
    bl   jit_watch_words_hit_1
    cbnz w0, L_jit_stm_1_3
    add  x9, x28, #0x3d0
    add  x9, x9, #0x2, lsl #12
    ldp  x10, x11, [x9, #-120]
    ldp  w12, w13, [x9, #-224]
    ldp  w14, w15, [x9, #-216]
    ldp  w16, w17, [x9, #-208]
    ldr  w18, [x28, #8968]
    ldr  x30, [x28, #9064]
    ldr  w3, [x28, #9044]
    msr  nzcv, x3
    ret
L_jit_stm_1_3:
    ldr  x0, [x28, #9064]
    ldr  x1, [x28, #9056]
    bl   jit_host_to_ds_addr
    str  w0, [x28, #9148]
    mov  x0, x28
    ldr  w1, [x28, #9012]
    bl   jit_cache_flush
    ldr  w1, [x28, #9148]
    mov  x0, x28
    bl   jit_cache_lookup_or_compile
    add  x9, x28, #0x3d0
    add  x9, x9, #0x2, lsl #12
    ldp  x10, x11, [x9, #-120]
    ldp  w12, w13, [x9, #-224]
    ldp  w14, w15, [x9, #-216]
    ldp  w16, w17, [x9, #-208]
    ldr  w18, [x28, #8968]
    ldr  w3, [x28, #9044]
    msr  nzcv, x3
    br   x0
L_jit_stm_1_exit_1:
    b    jit_stm_write_words_slow
    .size jit_stm_1, .-jit_stm_1

    .text
    .align 2
    .globl jit_stm_2
    .type  jit_stm_2, %function
jit_stm_2:
    orr  w1, wzr, w0, lsr #11
    and  w0, w0, #0xfffffffc
    ldr  x1, [x9, w1, uxtw #3]
    orr  x2, xzr, x1, lsl #2
    tbnz x1, #62, L_jit_stm_2_1
    add  x1, x2, w0, uxtw
    sub  x2, x9, #0x120
    ldp  w3, w4, [x2]
    stp  w3, w4, [x1]
    ret
L_jit_stm_2_1:
    tbnz x1, #63, L_jit_stm_2_2
    str  w21, [x28, #9024]
    mov  w21, #0x2
    b    L_jit_stm_2_exit_1
L_jit_stm_2_2:
    add  x1, x2, w0, uxtw
    sub  x2, x9, #0x120
    ldp  w3, w4, [x2]
    stp  w3, w4, [x1]
    stp  w12, w13, [x9, #-224]
    stp  w14, w15, [x9, #-216]
    stp  w16, w17, [x9, #-208]
    str  w18, [x28, #8968]
    str  x30, [x28, #9064]
    mrs  x3, nzcv
    str  w3, [x28, #9044]
    mov  w1, w0
    str  w1, [x28, #9012]
    mov  x0, x28
    bl   jit_watch_words_hit_2
    cbnz w0, L_jit_stm_2_3
    add  x9, x28, #0x3d0
    add  x9, x9, #0x2, lsl #12
    ldp  x10, x11, [x9, #-120]
    ldp  w12, w13, [x9, #-224]
    ldp  w14, w15, [x9, #-216]
    ldp  w16, w17, [x9, #-208]
    ldr  w18, [x28, #8968]
    ldr  x30, [x28, #9064]
    ldr  w3, [x28, #9044]
    msr  nzcv, x3
    ret
L_jit_stm_2_3:
    ldr  x0, [x28, #9064]
    ldr  x1, [x28, #9056]
    bl   jit_host_to_ds_addr
    str  w0, [x28, #9148]
    mov  x0, x28
    ldr  w1, [x28, #9012]
    bl   jit_cache_flush
    ldr  w1, [x28, #9148]
    mov  x0, x28
    bl   jit_cache_lookup_or_compile
    add  x9, x28, #0x3d0
    add  x9, x9, #0x2, lsl #12
    ldp  x10, x11, [x9, #-120]
    ldp  w12, w13, [x9, #-224]
    ldp  w14, w15, [x9, #-216]
    ldp  w16, w17, [x9, #-208]
    ldr  w18, [x28, #8968]
    ldr  w3, [x28, #9044]
    msr  nzcv, x3
    br   x0
L_jit_stm_2_exit_1:
    b    jit_stm_write_words_slow
    .size jit_stm_2, .-jit_stm_2

    .text
    .align 2
    .globl jit_stm_3
    .type  jit_stm_3, %function
jit_stm_3:
    orr  w1, wzr, w0, lsr #11
    and  w0, w0, #0xfffffffc
    ldr  x1, [x9, w1, uxtw #3]
    orr  x2, xzr, x1, lsl #2
    tbnz x1, #62, 1f
    add  x1, x2, w0, uxtw
    sub  x2, x9, #0x120
    ldp  w3, w4, [x2]
    stp  w3, w4, [x1]
    ldr  w3, [x2, #8]
    str  w3, [x1, #8]
    ret
1:
    tbnz x1, #63, 2f
    str  w21, [x28, #9024]
    mov  w21, #0x3
    b    jit_stm_write_words_slow
2:
    add  x1, x2, w0, uxtw
    sub  x2, x9, #0x120
    ldp  w3, w4, [x2]
    stp  w3, w4, [x1]
    ldr  w3, [x2, #8]
    str  w3, [x1, #8]
    stp  w12, w13, [x9, #-224]
    stp  w14, w15, [x9, #-216]
    stp  w16, w17, [x9, #-208]
    str  w18, [x28, #8968]
    str  x30, [x28, #9064]
    mrs  x3, nzcv
    str  w3, [x28, #9044]
    mov  w1, w0
    str  w1, [x28, #9012]
    mov  x0, x28
    bl   jit_watch_words_hit_3
    cbnz w0, 3f
    add  x9, x28, #0x3d0
    add  x9, x9, #0x2, lsl #12
    ldp  x10, x11, [x9, #-120]
    ldp  w12, w13, [x9, #-224]
    ldp  w14, w15, [x9, #-216]
    ldp  w16, w17, [x9, #-208]
    ldr  w18, [x28, #8968]
    ldr  x30, [x28, #9064]
    ldr  w3, [x28, #9044]
    msr  nzcv, x3
    ret
3:
    ldr  x0, [x28, #9064]
    ldr  x1, [x28, #9056]
    bl   jit_host_to_ds_addr
    str  w0, [x28, #9148]
    mov  x0, x28
    ldr  w1, [x28, #9012]
    bl   jit_cache_flush
    ldr  w1, [x28, #9148]
    mov  x0, x28
    bl   jit_cache_lookup_or_compile
    add  x9, x28, #0x3d0
    add  x9, x9, #0x2, lsl #12
    ldp  x10, x11, [x9, #-120]
    ldp  w12, w13, [x9, #-224]
    ldp  w14, w15, [x9, #-216]
    ldp  w16, w17, [x9, #-208]
    ldr  w18, [x28, #8968]
    ldr  w3, [x28, #9044]
    msr  nzcv, x3
    br   x0
    .size jit_stm_3, .-jit_stm_3

    .text
    .align 2
    .globl jit_stm_4
    .type  jit_stm_4, %function
jit_stm_4:
    orr  w1, wzr, w0, lsr #11
    and  w0, w0, #0xfffffffc
    ldr  x1, [x9, w1, uxtw #3]
    orr  x2, xzr, x1, lsl #2
    tbnz x1, #62, 1f
    add  x1, x2, w0, uxtw
    sub  x2, x9, #0x120
    ldp  x3, x4, [x2]
    stp  x3, x4, [x1]
    ret
1:
    tbnz x1, #63, 2f
    str  w21, [x28, #9024]
    mov  w21, #0x4
    b    jit_stm_write_words_slow
2:
    add  x1, x2, w0, uxtw
    sub  x2, x9, #0x120
    ldp  x3, x4, [x2]
    stp  x3, x4, [x1]
    stp  w12, w13, [x9, #-224]
    stp  w14, w15, [x9, #-216]
    stp  w16, w17, [x9, #-208]
    str  w18, [x28, #8968]
    str  x30, [x28, #9064]
    mrs  x3, nzcv
    str  w3, [x28, #9044]
    mov  w1, w0
    str  w1, [x28, #9012]
    mov  x0, x28
    bl   jit_watch_words_hit_4
    cbnz w0, 3f
    add  x9, x28, #0x3d0
    add  x9, x9, #0x2, lsl #12
    ldp  x10, x11, [x9, #-120]
    ldp  w12, w13, [x9, #-224]
    ldp  w14, w15, [x9, #-216]
    ldp  w16, w17, [x9, #-208]
    ldr  w18, [x28, #8968]
    ldr  x30, [x28, #9064]
    ldr  w3, [x28, #9044]
    msr  nzcv, x3
    ret
3:
    ldr  x0, [x28, #9064]
    ldr  x1, [x28, #9056]
    bl   jit_host_to_ds_addr
    str  w0, [x28, #9148]
    mov  x0, x28
    ldr  w1, [x28, #9012]
    bl   jit_cache_flush
    ldr  w1, [x28, #9148]
    mov  x0, x28
    bl   jit_cache_lookup_or_compile
    add  x9, x28, #0x3d0
    add  x9, x9, #0x2, lsl #12
    ldp  x10, x11, [x9, #-120]
    ldp  w12, w13, [x9, #-224]
    ldp  w14, w15, [x9, #-216]
    ldp  w16, w17, [x9, #-208]
    ldr  w18, [x28, #8968]
    ldr  w3, [x28, #9044]
    msr  nzcv, x3
    br   x0
    .size jit_stm_4, .-jit_stm_4

    .text
    .align 2
    .globl jit_stm_5
    .type  jit_stm_5, %function
jit_stm_5:
    orr  w1, wzr, w0, lsr #11
    and  w0, w0, #0xfffffffc
    ldr  x1, [x9, w1, uxtw #3]
    orr  x2, xzr, x1, lsl #2
    tbnz x1, #62, 1f
    add  x1, x2, w0, uxtw
    sub  x2, x9, #0x120
    ldp  x3, x4, [x2]
    stp  x3, x4, [x1]
    ldr  w3, [x2, #16]
    str  w3, [x1, #16]
    ret
1:
    tbnz x1, #63, 2f
    str  w21, [x28, #9024]
    mov  w21, #0x5
    b    jit_stm_write_words_slow
2:
    add  x1, x2, w0, uxtw
    sub  x2, x9, #0x120
    ldp  x3, x4, [x2]
    stp  x3, x4, [x1]
    ldr  w3, [x2, #16]
    str  w3, [x1, #16]
    stp  w12, w13, [x9, #-224]
    stp  w14, w15, [x9, #-216]
    stp  w16, w17, [x9, #-208]
    str  w18, [x28, #8968]
    str  x30, [x28, #9064]
    mrs  x3, nzcv
    str  w3, [x28, #9044]
    mov  w1, w0
    str  w1, [x28, #9012]
    mov  x0, x28
    bl   jit_watch_words_hit_5
    cbnz w0, 3f
    add  x9, x28, #0x3d0
    add  x9, x9, #0x2, lsl #12
    ldp  x10, x11, [x9, #-120]
    ldp  w12, w13, [x9, #-224]
    ldp  w14, w15, [x9, #-216]
    ldp  w16, w17, [x9, #-208]
    ldr  w18, [x28, #8968]
    ldr  x30, [x28, #9064]
    ldr  w3, [x28, #9044]
    msr  nzcv, x3
    ret
3:
    ldr  x0, [x28, #9064]
    ldr  x1, [x28, #9056]
    bl   jit_host_to_ds_addr
    str  w0, [x28, #9148]
    mov  x0, x28
    ldr  w1, [x28, #9012]
    bl   jit_cache_flush
    ldr  w1, [x28, #9148]
    mov  x0, x28
    bl   jit_cache_lookup_or_compile
    add  x9, x28, #0x3d0
    add  x9, x9, #0x2, lsl #12
    ldp  x10, x11, [x9, #-120]
    ldp  w12, w13, [x9, #-224]
    ldp  w14, w15, [x9, #-216]
    ldp  w16, w17, [x9, #-208]
    ldr  w18, [x28, #8968]
    ldr  w3, [x28, #9044]
    msr  nzcv, x3
    br   x0
    .size jit_stm_5, .-jit_stm_5

    .text
    .align 2
    .globl jit_stm_6
    .type  jit_stm_6, %function
jit_stm_6:
    orr  w1, wzr, w0, lsr #11
    and  w0, w0, #0xfffffffc
    ldr  x1, [x9, w1, uxtw #3]
    orr  x2, xzr, x1, lsl #2
    tbnz x1, #62, 1f
    add  x1, x2, w0, uxtw
    sub  x2, x9, #0x120
    ldp  x3, x4, [x2]
    stp  x3, x4, [x1]
    ldp  w3, w4, [x2, #16]
    stp  w3, w4, [x1, #16]
    ret
1:
    tbnz x1, #63, 2f
    str  w21, [x28, #9024]
    mov  w21, #0x6
    b    jit_stm_write_words_slow
2:
    add  x1, x2, w0, uxtw
    sub  x2, x9, #0x120
    ldp  x3, x4, [x2]
    stp  x3, x4, [x1]
    ldp  w3, w4, [x2, #16]
    stp  w3, w4, [x1, #16]
    stp  w12, w13, [x9, #-224]
    stp  w14, w15, [x9, #-216]
    stp  w16, w17, [x9, #-208]
    str  w18, [x28, #8968]
    str  x30, [x28, #9064]
    mrs  x3, nzcv
    str  w3, [x28, #9044]
    mov  w1, w0
    str  w1, [x28, #9012]
    mov  x0, x28
    bl   jit_watch_words_hit_6
    cbnz w0, 3f
    add  x9, x28, #0x3d0
    add  x9, x9, #0x2, lsl #12
    ldp  x10, x11, [x9, #-120]
    ldp  w12, w13, [x9, #-224]
    ldp  w14, w15, [x9, #-216]
    ldp  w16, w17, [x9, #-208]
    ldr  w18, [x28, #8968]
    ldr  x30, [x28, #9064]
    ldr  w3, [x28, #9044]
    msr  nzcv, x3
    ret
3:
    ldr  x0, [x28, #9064]
    ldr  x1, [x28, #9056]
    bl   jit_host_to_ds_addr
    str  w0, [x28, #9148]
    mov  x0, x28
    ldr  w1, [x28, #9012]
    bl   jit_cache_flush
    ldr  w1, [x28, #9148]
    mov  x0, x28
    bl   jit_cache_lookup_or_compile
    add  x9, x28, #0x3d0
    add  x9, x9, #0x2, lsl #12
    ldp  x10, x11, [x9, #-120]
    ldp  w12, w13, [x9, #-224]
    ldp  w14, w15, [x9, #-216]
    ldp  w16, w17, [x9, #-208]
    ldr  w18, [x28, #8968]
    ldr  w3, [x28, #9044]
    msr  nzcv, x3
    br   x0
    .size jit_stm_6, .-jit_stm_6

    .text
    .align 2
    .globl jit_stm_7
    .type  jit_stm_7, %function
jit_stm_7:
    orr  w1, wzr, w0, lsr #11
    and  w0, w0, #0xfffffffc
    ldr  x1, [x9, w1, uxtw #3]
    orr  x2, xzr, x1, lsl #2
    tbnz x1, #62, L_jit_stm_7_1
    add  x1, x2, w0, uxtw
    sub  x2, x9, #0x120
    ldp  x3, x4, [x2]
    stp  x3, x4, [x1]
    ldp  w3, w4, [x2, #16]
    stp  w3, w4, [x1, #16]
    ldr  w3, [x2, #24]
    str  w3, [x1, #24]
    ret
L_jit_stm_7_1:
    tbnz x1, #63, L_jit_stm_7_2
    str  w21, [x28, #9024]
    mov  w21, #0x7
    b    L_jit_stm_7_exit_1
L_jit_stm_7_2:
    add  x1, x2, w0, uxtw
    sub  x2, x9, #0x120
    ldp  x3, x4, [x2]
    stp  x3, x4, [x1]
    ldp  w3, w4, [x2, #16]
    stp  w3, w4, [x1, #16]
    ldr  w3, [x2, #24]
    str  w3, [x1, #24]
    stp  w12, w13, [x9, #-224]
    stp  w14, w15, [x9, #-216]
    stp  w16, w17, [x9, #-208]
    str  w18, [x28, #8968]
    str  x30, [x28, #9064]
    mrs  x3, nzcv
    str  w3, [x28, #9044]
    mov  w1, w0
    str  w1, [x28, #9012]
    mov  x0, x28
    bl   jit_watch_words_hit_7
    cbnz w0, L_jit_stm_7_3
    add  x9, x28, #0x3d0
    add  x9, x9, #0x2, lsl #12
    ldp  x10, x11, [x9, #-120]
    ldp  w12, w13, [x9, #-224]
    ldp  w14, w15, [x9, #-216]
    ldp  w16, w17, [x9, #-208]
    ldr  w18, [x28, #8968]
    ldr  x30, [x28, #9064]
    ldr  w3, [x28, #9044]
    msr  nzcv, x3
    ret
L_jit_stm_7_3:
    ldr  x0, [x28, #9064]
    ldr  x1, [x28, #9056]
    bl   jit_host_to_ds_addr
    str  w0, [x28, #9148]
    mov  x0, x28
    ldr  w1, [x28, #9012]
    bl   jit_cache_flush
    ldr  w1, [x28, #9148]
    mov  x0, x28
    bl   jit_cache_lookup_or_compile
    add  x9, x28, #0x3d0
    add  x9, x9, #0x2, lsl #12
    ldp  x10, x11, [x9, #-120]
    ldp  w12, w13, [x9, #-224]
    ldp  w14, w15, [x9, #-216]
    ldp  w16, w17, [x9, #-208]
    ldr  w18, [x28, #8968]
    ldr  w3, [x28, #9044]
    msr  nzcv, x3
    br   x0
L_jit_stm_7_exit_1:
    b    jit_stm_write_words_slow
    .size jit_stm_7, .-jit_stm_7

    .text
    .align 2
    .globl jit_stm_8
    .type  jit_stm_8, %function
jit_stm_8:
    orr  w1, wzr, w0, lsr #11
    and  w0, w0, #0xfffffffc
    ldr  x1, [x9, w1, uxtw #3]
    orr  x2, xzr, x1, lsl #2
    tbnz x1, #62, L_jit_stm_8_1
    add  x1, x2, w0, uxtw
    sub  x2, x9, #0x120
    ldp  x3, x4, [x2]
    ldp  x5, x6, [x2, #16]
    stp  x3, x4, [x1]
    stp  x5, x6, [x1, #16]
    ret
L_jit_stm_8_1:
    tbnz x1, #63, L_jit_stm_8_2
    str  w21, [x28, #9024]
    mov  w21, #0x8
    b    L_jit_stm_8_exit_1
L_jit_stm_8_2:
    add  x1, x2, w0, uxtw
    sub  x2, x9, #0x120
    ldp  x3, x4, [x2]
    ldp  x5, x6, [x2, #16]
    stp  x3, x4, [x1]
    stp  x5, x6, [x1, #16]
    stp  w12, w13, [x9, #-224]
    stp  w14, w15, [x9, #-216]
    stp  w16, w17, [x9, #-208]
    str  w18, [x28, #8968]
    str  x30, [x28, #9064]
    mrs  x3, nzcv
    str  w3, [x28, #9044]
    mov  w1, w0
    str  w1, [x28, #9012]
    mov  x0, x28
    bl   jit_watch_words_hit_8
    cbnz w0, L_jit_stm_8_3
    add  x9, x28, #0x3d0
    add  x9, x9, #0x2, lsl #12
    ldp  x10, x11, [x9, #-120]
    ldp  w12, w13, [x9, #-224]
    ldp  w14, w15, [x9, #-216]
    ldp  w16, w17, [x9, #-208]
    ldr  w18, [x28, #8968]
    ldr  x30, [x28, #9064]
    ldr  w3, [x28, #9044]
    msr  nzcv, x3
    ret
L_jit_stm_8_3:
    ldr  x0, [x28, #9064]
    ldr  x1, [x28, #9056]
    bl   jit_host_to_ds_addr
    str  w0, [x28, #9148]
    mov  x0, x28
    ldr  w1, [x28, #9012]
    bl   jit_cache_flush
    ldr  w1, [x28, #9148]
    mov  x0, x28
    bl   jit_cache_lookup_or_compile
    add  x9, x28, #0x3d0
    add  x9, x9, #0x2, lsl #12
    ldp  x10, x11, [x9, #-120]
    ldp  w12, w13, [x9, #-224]
    ldp  w14, w15, [x9, #-216]
    ldp  w16, w17, [x9, #-208]
    ldr  w18, [x28, #8968]
    ldr  w3, [x28, #9044]
    msr  nzcv, x3
    br   x0
L_jit_stm_8_exit_1:
    b    jit_stm_write_words_slow
    .size jit_stm_8, .-jit_stm_8

    .text
    .align 2
    .globl jit_stm_9
    .type  jit_stm_9, %function
jit_stm_9:
    orr  w1, wzr, w0, lsr #11
    and  w0, w0, #0xfffffffc
    ldr  x1, [x9, w1, uxtw #3]
    orr  x2, xzr, x1, lsl #2
    tbnz x1, #62, L_jit_stm_9_1
    add  x1, x2, w0, uxtw
    sub  x2, x9, #0x120
    ldp  x3, x4, [x2]
    ldp  x5, x6, [x2, #16]
    stp  x3, x4, [x1]
    stp  x5, x6, [x1, #16]
    ldr  w3, [x2, #32]
    str  w3, [x1, #32]
    ret
L_jit_stm_9_1:
    tbnz x1, #63, L_jit_stm_9_2
    str  w21, [x28, #9024]
    mov  w21, #0x9
    b    L_jit_stm_9_exit_1
L_jit_stm_9_2:
    add  x1, x2, w0, uxtw
    sub  x2, x9, #0x120
    ldp  x3, x4, [x2]
    ldp  x5, x6, [x2, #16]
    stp  x3, x4, [x1]
    stp  x5, x6, [x1, #16]
    ldr  w3, [x2, #32]
    str  w3, [x1, #32]
    stp  w12, w13, [x9, #-224]
    stp  w14, w15, [x9, #-216]
    stp  w16, w17, [x9, #-208]
    str  w18, [x28, #8968]
    str  x30, [x28, #9064]
    mrs  x3, nzcv
    str  w3, [x28, #9044]
    mov  w1, w0
    str  w1, [x28, #9012]
    mov  x0, x28
    bl   jit_watch_words_hit_9
    cbnz w0, L_jit_stm_9_3
    add  x9, x28, #0x3d0
    add  x9, x9, #0x2, lsl #12
    ldp  x10, x11, [x9, #-120]
    ldp  w12, w13, [x9, #-224]
    ldp  w14, w15, [x9, #-216]
    ldp  w16, w17, [x9, #-208]
    ldr  w18, [x28, #8968]
    ldr  x30, [x28, #9064]
    ldr  w3, [x28, #9044]
    msr  nzcv, x3
    ret
L_jit_stm_9_3:
    ldr  x0, [x28, #9064]
    ldr  x1, [x28, #9056]
    bl   jit_host_to_ds_addr
    str  w0, [x28, #9148]
    mov  x0, x28
    ldr  w1, [x28, #9012]
    bl   jit_cache_flush
    ldr  w1, [x28, #9148]
    mov  x0, x28
    bl   jit_cache_lookup_or_compile
    add  x9, x28, #0x3d0
    add  x9, x9, #0x2, lsl #12
    ldp  x10, x11, [x9, #-120]
    ldp  w12, w13, [x9, #-224]
    ldp  w14, w15, [x9, #-216]
    ldp  w16, w17, [x9, #-208]
    ldr  w18, [x28, #8968]
    ldr  w3, [x28, #9044]
    msr  nzcv, x3
    br   x0
L_jit_stm_9_exit_1:
    b    jit_stm_write_words_slow
    .size jit_stm_9, .-jit_stm_9

    .text
    .align 2
    .globl jit_stm_10
    .type  jit_stm_10, %function
jit_stm_10:
    orr  w1, wzr, w0, lsr #11
    and  w0, w0, #0xfffffffc
    ldr  x1, [x9, w1, uxtw #3]
    orr  x2, xzr, x1, lsl #2
    tbnz x1, #62, L_jit_stm_10_1
    add  x1, x2, w0, uxtw
    sub  x2, x9, #0x120
    ldp  x3, x4, [x2]
    ldp  x5, x6, [x2, #16]
    stp  x3, x4, [x1]
    stp  x5, x6, [x1, #16]
    ldp  w3, w4, [x2, #32]
    stp  w3, w4, [x1, #32]
    ret
L_jit_stm_10_1:
    tbnz x1, #63, L_jit_stm_10_2
    str  w21, [x28, #9024]
    mov  w21, #0xa
    b    L_jit_stm_10_exit_1
L_jit_stm_10_2:
    add  x1, x2, w0, uxtw
    sub  x2, x9, #0x120
    ldp  x3, x4, [x2]
    ldp  x5, x6, [x2, #16]
    stp  x3, x4, [x1]
    stp  x5, x6, [x1, #16]
    ldp  w3, w4, [x2, #32]
    stp  w3, w4, [x1, #32]
    stp  w12, w13, [x9, #-224]
    stp  w14, w15, [x9, #-216]
    stp  w16, w17, [x9, #-208]
    str  w18, [x28, #8968]
    str  x30, [x28, #9064]
    mrs  x3, nzcv
    str  w3, [x28, #9044]
    mov  w1, w0
    str  w1, [x28, #9012]
    mov  x0, x28
    bl   jit_watch_words_hit_10
    cbnz w0, L_jit_stm_10_3
    add  x9, x28, #0x3d0
    add  x9, x9, #0x2, lsl #12
    ldp  x10, x11, [x9, #-120]
    ldp  w12, w13, [x9, #-224]
    ldp  w14, w15, [x9, #-216]
    ldp  w16, w17, [x9, #-208]
    ldr  w18, [x28, #8968]
    ldr  x30, [x28, #9064]
    ldr  w3, [x28, #9044]
    msr  nzcv, x3
    ret
L_jit_stm_10_3:
    ldr  x0, [x28, #9064]
    ldr  x1, [x28, #9056]
    bl   jit_host_to_ds_addr
    str  w0, [x28, #9148]
    mov  x0, x28
    ldr  w1, [x28, #9012]
    bl   jit_cache_flush
    ldr  w1, [x28, #9148]
    mov  x0, x28
    bl   jit_cache_lookup_or_compile
    add  x9, x28, #0x3d0
    add  x9, x9, #0x2, lsl #12
    ldp  x10, x11, [x9, #-120]
    ldp  w12, w13, [x9, #-224]
    ldp  w14, w15, [x9, #-216]
    ldp  w16, w17, [x9, #-208]
    ldr  w18, [x28, #8968]
    ldr  w3, [x28, #9044]
    msr  nzcv, x3
    br   x0
L_jit_stm_10_exit_1:
    b    jit_stm_write_words_slow
    .size jit_stm_10, .-jit_stm_10

    .text
    .align 2
    .globl jit_stm_11
    .type  jit_stm_11, %function
jit_stm_11:
    orr  w1, wzr, w0, lsr #11
    and  w0, w0, #0xfffffffc
    ldr  x1, [x9, w1, uxtw #3]
    orr  x2, xzr, x1, lsl #2
    tbnz x1, #62, 1f
    add  x1, x2, w0, uxtw
    sub  x2, x9, #0x120
    ldp  x3, x4, [x2]
    ldp  x5, x6, [x2, #16]
    stp  x3, x4, [x1]
    stp  x5, x6, [x1, #16]
    ldp  w3, w4, [x2, #32]
    stp  w3, w4, [x1, #32]
    ldr  w3, [x2, #40]
    str  w3, [x1, #40]
    ret
1:
    tbnz x1, #63, 2f
    str  w21, [x28, #9024]
    mov  w21, #0xb
    b    L_jit_stm_11_exit_1
2:
    add  x1, x2, w0, uxtw
    sub  x2, x9, #0x120
    ldp  x3, x4, [x2]
    ldp  x5, x6, [x2, #16]
    stp  x3, x4, [x1]
    stp  x5, x6, [x1, #16]
    ldp  w3, w4, [x2, #32]
    stp  w3, w4, [x1, #32]
    ldr  w3, [x2, #40]
    str  w3, [x1, #40]
    stp  w12, w13, [x9, #-224]
    stp  w14, w15, [x9, #-216]
    stp  w16, w17, [x9, #-208]
    str  w18, [x28, #8968]
    str  x30, [x28, #9064]
    mrs  x3, nzcv
    str  w3, [x28, #9044]
    mov  w1, w0
    str  w1, [x28, #9012]
    mov  x0, x28
    bl   jit_watch_words_hit_11
    cbnz w0, 3f
    add  x9, x28, #0x3d0
    add  x9, x9, #0x2, lsl #12
    ldp  x10, x11, [x9, #-120]
    ldp  w12, w13, [x9, #-224]
    ldp  w14, w15, [x9, #-216]
    ldp  w16, w17, [x9, #-208]
    ldr  w18, [x28, #8968]
    ldr  x30, [x28, #9064]
    ldr  w3, [x28, #9044]
    msr  nzcv, x3
    ret
3:
    ldr  x0, [x28, #9064]
    ldr  x1, [x28, #9056]
    bl   jit_host_to_ds_addr
    str  w0, [x28, #9148]
    mov  x0, x28
    ldr  w1, [x28, #9012]
    bl   jit_cache_flush
    ldr  w1, [x28, #9148]
    mov  x0, x28
    bl   jit_cache_lookup_or_compile
    add  x9, x28, #0x3d0
    add  x9, x9, #0x2, lsl #12
    ldp  x10, x11, [x9, #-120]
    ldp  w12, w13, [x9, #-224]
    ldp  w14, w15, [x9, #-216]
    ldp  w16, w17, [x9, #-208]
    ldr  w18, [x28, #8968]
    ldr  w3, [x28, #9044]
    msr  nzcv, x3
    br   x0
L_jit_stm_11_exit_1:
    b    jit_stm_write_words_slow
    .size jit_stm_11, .-jit_stm_11

    .text
    .align 2
    .globl jit_stm_12
    .type  jit_stm_12, %function
jit_stm_12:
    orr  w1, wzr, w0, lsr #11
    and  w0, w0, #0xfffffffc
    ldr  x1, [x9, w1, uxtw #3]
    orr  x2, xzr, x1, lsl #2
    tbnz x1, #62, 1f
    add  x1, x2, w0, uxtw
    sub  x2, x9, #0x120
    ldp  x3, x4, [x2]
    ldp  x5, x6, [x2, #16]
    stp  x3, x4, [x1]
    stp  x5, x6, [x1, #16]
    ldp  x3, x4, [x2, #32]
    stp  x3, x4, [x1, #32]
    ret
1:
    tbnz x1, #63, 2f
    str  w21, [x28, #9024]
    mov  w21, #0xc
    b    L_jit_stm_12_exit_1
2:
    add  x1, x2, w0, uxtw
    sub  x2, x9, #0x120
    ldp  x3, x4, [x2]
    ldp  x5, x6, [x2, #16]
    stp  x3, x4, [x1]
    stp  x5, x6, [x1, #16]
    ldp  x3, x4, [x2, #32]
    stp  x3, x4, [x1, #32]
    stp  w12, w13, [x9, #-224]
    stp  w14, w15, [x9, #-216]
    stp  w16, w17, [x9, #-208]
    str  w18, [x28, #8968]
    str  x30, [x28, #9064]
    mrs  x3, nzcv
    str  w3, [x28, #9044]
    mov  w1, w0
    str  w1, [x28, #9012]
    mov  x0, x28
    bl   jit_watch_words_hit_12
    cbnz w0, 3f
    add  x9, x28, #0x3d0
    add  x9, x9, #0x2, lsl #12
    ldp  x10, x11, [x9, #-120]
    ldp  w12, w13, [x9, #-224]
    ldp  w14, w15, [x9, #-216]
    ldp  w16, w17, [x9, #-208]
    ldr  w18, [x28, #8968]
    ldr  x30, [x28, #9064]
    ldr  w3, [x28, #9044]
    msr  nzcv, x3
    ret
3:
    ldr  x0, [x28, #9064]
    ldr  x1, [x28, #9056]
    bl   jit_host_to_ds_addr
    str  w0, [x28, #9148]
    mov  x0, x28
    ldr  w1, [x28, #9012]
    bl   jit_cache_flush
    ldr  w1, [x28, #9148]
    mov  x0, x28
    bl   jit_cache_lookup_or_compile
    add  x9, x28, #0x3d0
    add  x9, x9, #0x2, lsl #12
    ldp  x10, x11, [x9, #-120]
    ldp  w12, w13, [x9, #-224]
    ldp  w14, w15, [x9, #-216]
    ldp  w16, w17, [x9, #-208]
    ldr  w18, [x28, #8968]
    ldr  w3, [x28, #9044]
    msr  nzcv, x3
    br   x0
L_jit_stm_12_exit_1:
    b    jit_stm_write_words_slow
    .size jit_stm_12, .-jit_stm_12

    .text
    .align 2
    .globl jit_stm_13
    .type  jit_stm_13, %function
jit_stm_13:
    orr  w1, wzr, w0, lsr #11
    and  w0, w0, #0xfffffffc
    ldr  x1, [x9, w1, uxtw #3]
    orr  x2, xzr, x1, lsl #2
    tbnz x1, #62, 1f
    add  x1, x2, w0, uxtw
    sub  x2, x9, #0x120
    ldp  x3, x4, [x2]
    ldp  x5, x6, [x2, #16]
    stp  x3, x4, [x1]
    stp  x5, x6, [x1, #16]
    ldp  x3, x4, [x2, #32]
    stp  x3, x4, [x1, #32]
    ldr  w3, [x2, #48]
    str  w3, [x1, #48]
    ret
1:
    tbnz x1, #63, 2f
    str  w21, [x28, #9024]
    mov  w21, #0xd
    b    L_jit_stm_13_exit_1
2:
    add  x1, x2, w0, uxtw
    sub  x2, x9, #0x120
    ldp  x3, x4, [x2]
    ldp  x5, x6, [x2, #16]
    stp  x3, x4, [x1]
    stp  x5, x6, [x1, #16]
    ldp  x3, x4, [x2, #32]
    stp  x3, x4, [x1, #32]
    ldr  w3, [x2, #48]
    str  w3, [x1, #48]
    stp  w12, w13, [x9, #-224]
    stp  w14, w15, [x9, #-216]
    stp  w16, w17, [x9, #-208]
    str  w18, [x28, #8968]
    str  x30, [x28, #9064]
    mrs  x3, nzcv
    str  w3, [x28, #9044]
    mov  w1, w0
    str  w1, [x28, #9012]
    mov  x0, x28
    bl   jit_watch_words_hit_13
    cbnz w0, 3f
    add  x9, x28, #0x3d0
    add  x9, x9, #0x2, lsl #12
    ldp  x10, x11, [x9, #-120]
    ldp  w12, w13, [x9, #-224]
    ldp  w14, w15, [x9, #-216]
    ldp  w16, w17, [x9, #-208]
    ldr  w18, [x28, #8968]
    ldr  x30, [x28, #9064]
    ldr  w3, [x28, #9044]
    msr  nzcv, x3
    ret
3:
    ldr  x0, [x28, #9064]
    ldr  x1, [x28, #9056]
    bl   jit_host_to_ds_addr
    str  w0, [x28, #9148]
    mov  x0, x28
    ldr  w1, [x28, #9012]
    bl   jit_cache_flush
    ldr  w1, [x28, #9148]
    mov  x0, x28
    bl   jit_cache_lookup_or_compile
    add  x9, x28, #0x3d0
    add  x9, x9, #0x2, lsl #12
    ldp  x10, x11, [x9, #-120]
    ldp  w12, w13, [x9, #-224]
    ldp  w14, w15, [x9, #-216]
    ldp  w16, w17, [x9, #-208]
    ldr  w18, [x28, #8968]
    ldr  w3, [x28, #9044]
    msr  nzcv, x3
    br   x0
L_jit_stm_13_exit_1:
    b    jit_stm_write_words_slow
    .size jit_stm_13, .-jit_stm_13

    .text
    .align 2
    .globl jit_stm_14
    .type  jit_stm_14, %function
jit_stm_14:
    orr  w1, wzr, w0, lsr #11
    and  w0, w0, #0xfffffffc
    ldr  x1, [x9, w1, uxtw #3]
    orr  x2, xzr, x1, lsl #2
    tbnz x1, #62, 1f
    add  x1, x2, w0, uxtw
    sub  x2, x9, #0x120
    ldp  x3, x4, [x2]
    ldp  x5, x6, [x2, #16]
    stp  x3, x4, [x1]
    stp  x5, x6, [x1, #16]
    ldp  x3, x4, [x2, #32]
    stp  x3, x4, [x1, #32]
    ldp  w3, w4, [x2, #48]
    stp  w3, w4, [x1, #48]
    ret
1:
    tbnz x1, #63, 2f
    str  w21, [x28, #9024]
    mov  w21, #0xe
    b    L_jit_stm_14_exit_1
2:
    add  x1, x2, w0, uxtw
    sub  x2, x9, #0x120
    ldp  x3, x4, [x2]
    ldp  x5, x6, [x2, #16]
    stp  x3, x4, [x1]
    stp  x5, x6, [x1, #16]
    ldp  x3, x4, [x2, #32]
    stp  x3, x4, [x1, #32]
    ldp  w3, w4, [x2, #48]
    stp  w3, w4, [x1, #48]
    stp  w12, w13, [x9, #-224]
    stp  w14, w15, [x9, #-216]
    stp  w16, w17, [x9, #-208]
    str  w18, [x28, #8968]
    str  x30, [x28, #9064]
    mrs  x3, nzcv
    str  w3, [x28, #9044]
    mov  w1, w0
    str  w1, [x28, #9012]
    mov  x0, x28
    bl   jit_watch_words_hit_14
    cbnz w0, 3f
    add  x9, x28, #0x3d0
    add  x9, x9, #0x2, lsl #12
    ldp  x10, x11, [x9, #-120]
    ldp  w12, w13, [x9, #-224]
    ldp  w14, w15, [x9, #-216]
    ldp  w16, w17, [x9, #-208]
    ldr  w18, [x28, #8968]
    ldr  x30, [x28, #9064]
    ldr  w3, [x28, #9044]
    msr  nzcv, x3
    ret
3:
    ldr  x0, [x28, #9064]
    ldr  x1, [x28, #9056]
    bl   jit_host_to_ds_addr
    str  w0, [x28, #9148]
    mov  x0, x28
    ldr  w1, [x28, #9012]
    bl   jit_cache_flush
    ldr  w1, [x28, #9148]
    mov  x0, x28
    bl   jit_cache_lookup_or_compile
    add  x9, x28, #0x3d0
    add  x9, x9, #0x2, lsl #12
    ldp  x10, x11, [x9, #-120]
    ldp  w12, w13, [x9, #-224]
    ldp  w14, w15, [x9, #-216]
    ldp  w16, w17, [x9, #-208]
    ldr  w18, [x28, #8968]
    ldr  w3, [x28, #9044]
    msr  nzcv, x3
    br   x0
L_jit_stm_14_exit_1:
    b    jit_stm_write_words_slow
    .size jit_stm_14, .-jit_stm_14

    .text
    .align 2
    .globl jit_stm_15
    .type  jit_stm_15, %function
jit_stm_15:
    orr  w1, wzr, w0, lsr #11
    and  w0, w0, #0xfffffffc
    ldr  x1, [x9, w1, uxtw #3]
    orr  x2, xzr, x1, lsl #2
    tbnz x1, #62, 1f
    add  x1, x2, w0, uxtw
    sub  x2, x9, #0x120
    ldp  x3, x4, [x2]
    ldp  x5, x6, [x2, #16]
    stp  x3, x4, [x1]
    stp  x5, x6, [x1, #16]
    ldp  x3, x4, [x2, #32]
    stp  x3, x4, [x1, #32]
    ldp  w3, w4, [x2, #48]
    stp  w3, w4, [x1, #48]
    ldr  w3, [x2, #56]
    str  w3, [x1, #56]
    ret
1:
    tbnz x1, #63, 2f
    str  w21, [x28, #9024]
    mov  w21, #0xf
    b    L_jit_stm_15_exit_1
2:
    add  x1, x2, w0, uxtw
    sub  x2, x9, #0x120
    ldp  x3, x4, [x2]
    ldp  x5, x6, [x2, #16]
    stp  x3, x4, [x1]
    stp  x5, x6, [x1, #16]
    ldp  x3, x4, [x2, #32]
    stp  x3, x4, [x1, #32]
    ldp  w3, w4, [x2, #48]
    stp  w3, w4, [x1, #48]
    ldr  w3, [x2, #56]
    str  w3, [x1, #56]
    stp  w12, w13, [x9, #-224]
    stp  w14, w15, [x9, #-216]
    stp  w16, w17, [x9, #-208]
    str  w18, [x28, #8968]
    str  x30, [x28, #9064]
    mrs  x3, nzcv
    str  w3, [x28, #9044]
    mov  w1, w0
    str  w1, [x28, #9012]
    mov  x0, x28
    bl   jit_watch_words_hit_15
    cbnz w0, 3f
    add  x9, x28, #0x3d0
    add  x9, x9, #0x2, lsl #12
    ldp  x10, x11, [x9, #-120]
    ldp  w12, w13, [x9, #-224]
    ldp  w14, w15, [x9, #-216]
    ldp  w16, w17, [x9, #-208]
    ldr  w18, [x28, #8968]
    ldr  x30, [x28, #9064]
    ldr  w3, [x28, #9044]
    msr  nzcv, x3
    ret
3:
    ldr  x0, [x28, #9064]
    ldr  x1, [x28, #9056]
    bl   jit_host_to_ds_addr
    str  w0, [x28, #9148]
    mov  x0, x28
    ldr  w1, [x28, #9012]
    bl   jit_cache_flush
    ldr  w1, [x28, #9148]
    mov  x0, x28
    bl   jit_cache_lookup_or_compile
    add  x9, x28, #0x3d0
    add  x9, x9, #0x2, lsl #12
    ldp  x10, x11, [x9, #-120]
    ldp  w12, w13, [x9, #-224]
    ldp  w14, w15, [x9, #-216]
    ldp  w16, w17, [x9, #-208]
    ldr  w18, [x28, #8968]
    ldr  w3, [x28, #9044]
    msr  nzcv, x3
    br   x0
L_jit_stm_15_exit_1:
    b    jit_stm_write_words_slow
    .size jit_stm_15, .-jit_stm_15

    .text
    .align 2
    .globl jit_stm_16
    .type  jit_stm_16, %function
jit_stm_16:
    orr  w1, wzr, w0, lsr #11
    and  w0, w0, #0xfffffffc
    ldr  x1, [x9, w1, uxtw #3]
    orr  x2, xzr, x1, lsl #2
    tbnz x1, #62, 1f
    add  x1, x2, w0, uxtw
    sub  x2, x9, #0x120
    ldp  x3, x4, [x2]
    ldp  x5, x6, [x2, #16]
    stp  x3, x4, [x1]
    stp  x5, x6, [x1, #16]
    ldp  x3, x4, [x2, #32]
    ldp  x5, x6, [x2, #48]
    stp  x3, x4, [x1, #32]
    stp  x5, x6, [x1, #48]
    ret
1:
    tbnz x1, #63, 2f
    str  w21, [x28, #9024]
    mov  w21, #0x10
    b    L_jit_stm_16_exit_1
2:
    add  x1, x2, w0, uxtw
    sub  x2, x9, #0x120
    ldp  x3, x4, [x2]
    ldp  x5, x6, [x2, #16]
    stp  x3, x4, [x1]
    stp  x5, x6, [x1, #16]
    ldp  x3, x4, [x2, #32]
    ldp  x5, x6, [x2, #48]
    stp  x3, x4, [x1, #32]
    stp  x5, x6, [x1, #48]
    stp  w12, w13, [x9, #-224]
    stp  w14, w15, [x9, #-216]
    stp  w16, w17, [x9, #-208]
    str  w18, [x28, #8968]
    str  x30, [x28, #9064]
    mrs  x3, nzcv
    str  w3, [x28, #9044]
    mov  w1, w0
    str  w1, [x28, #9012]
    mov  x0, x28
    bl   jit_watch_words_hit_16
    cbnz w0, 3f
    add  x9, x28, #0x3d0
    add  x9, x9, #0x2, lsl #12
    ldp  x10, x11, [x9, #-120]
    ldp  w12, w13, [x9, #-224]
    ldp  w14, w15, [x9, #-216]
    ldp  w16, w17, [x9, #-208]
    ldr  w18, [x28, #8968]
    ldr  x30, [x28, #9064]
    ldr  w3, [x28, #9044]
    msr  nzcv, x3
    ret
3:
    ldr  x0, [x28, #9064]
    ldr  x1, [x28, #9056]
    bl   jit_host_to_ds_addr
    str  w0, [x28, #9148]
    mov  x0, x28
    ldr  w1, [x28, #9012]
    bl   jit_cache_flush
    ldr  w1, [x28, #9148]
    mov  x0, x28
    bl   jit_cache_lookup_or_compile
    add  x9, x28, #0x3d0
    add  x9, x9, #0x2, lsl #12
    ldp  x10, x11, [x9, #-120]
    ldp  w12, w13, [x9, #-224]
    ldp  w14, w15, [x9, #-216]
    ldp  w16, w17, [x9, #-208]
    ldr  w18, [x28, #8968]
    ldr  w3, [x28, #9044]
    msr  nzcv, x3
    br   x0
L_jit_stm_16_exit_1:
    b    jit_stm_write_words_slow
    .size jit_stm_16, .-jit_stm_16

    .text
    .align 2
    .globl jit_cpsr_write_mode
    .type  jit_cpsr_write_mode, %function
jit_cpsr_write_mode:
    ldr  w1, [x28, #9152]
    str  w0, [x28, #9152]
    eor  w2, w0, w1
    and  w3, w1, #0x1f
    and  w2, w1, w2
    cbnz w3, 2f
    tbnz w2, #7, 1f
    ret
1:
    stp  w12, w13, [x9, #-224]
    stp  w14, w15, [x9, #-216]
    stp  w16, w17, [x9, #-208]
    str  w18, [x28, #8968]
    str  x30, [x28, #9064]
    mrs  x2, nzcv
    str  w2, [x28, #9044]
    b    L_jit_cpsr_write_mode_exit_1
2:
    mov  w1, w0
    mov  x0, x28
    mrs  x2, nzcv
    stp  w13, w14, [x9, #-96]
    stp  w15, w16, [x9, #-88]
    stp  w17, w18, [x9, #-80]
    stp  w19, w20, [x9, #-72]
    stp  w21, w22, [x9, #-64]
    stp  w23, w24, [x9, #-56]
    stp  w25, w26, [x9, #-48]
    stur w27, [x9, #-40]
    str  w2, [x28, #9144]
    stp  w12, w13, [x9, #-224]
    stp  w14, w15, [x9, #-216]
    stp  w16, w17, [x9, #-208]
    str  w18, [x28, #8968]
    str  x30, [x28, #9064]
    mrs  x2, nzcv
    str  w2, [x28, #9044]
    bl   arm_bank_switch
    add  x9, x28, #0x3d0
    add  x9, x9, #0x2, lsl #12
    ldp  x10, x11, [x9, #-120]
    ldp  w12, w13, [x9, #-224]
    ldp  w14, w15, [x9, #-216]
    ldp  w16, w17, [x9, #-208]
    ldr  w18, [x28, #8968]
    ldr  x30, [x28, #9064]
    ldr  w0, [x28, #9044]
    msr  nzcv, x0
    ldr  w0, [x28, #9144]
    ldp  w13, w14, [x9, #-96]
    ldp  w15, w16, [x9, #-88]
    ldp  w17, w18, [x9, #-80]
    ldp  w19, w20, [x9, #-72]
    ldp  w21, w22, [x9, #-64]
    ldp  w23, w24, [x9, #-56]
    ldp  w25, w26, [x9, #-48]
    ldur w27, [x9, #-40]
    msr  nzcv, x0
    b    L_jit_cpsr_write_mode_exit_1
L_jit_cpsr_write_mode_exit_1:
    adrp x0, :got:jit_mem_write_irq_check
    ldr  x0, [x0, :got_lo12:jit_mem_write_irq_check]
    br   x0
    .size jit_cpsr_write_mode, .-jit_cpsr_write_mode

    .text
    .align 2
    .globl jit_spsr_write
    .type  jit_spsr_write, %function
jit_spsr_write:
    ldr  w2, [x28, #9152]
    and  w3, w2, #0xf
    cbz  w3, 1f
    ldr  w2, [x28, #8452]
    sub  x3, x9, #0x2e8
    ldr  w4, [x3, w2, uxtw #2]
    and  w4, w4, w1
    orr  w4, w4, w0
    str  w4, [x3, w2, uxtw #2]
1:
    ret
    .size jit_spsr_write, .-jit_spsr_write

    .text
    .align 2
    .globl jit_cpsr_read
    .type  jit_cpsr_read, %function
jit_cpsr_read:
    ldr  w0, [x28, #9152]
    mrs  x1, nzcv
    bfxil w0, w1, #0, #28
    ret
    .size jit_cpsr_read, .-jit_cpsr_read

    .text
    .align 2
    .globl jit_irq_enter
    .type  jit_irq_enter, %function
jit_irq_enter:
    mrs  x1, nzcv
    stp  w13, w14, [x9, #-96]
    stp  w15, w16, [x9, #-88]
    stp  w17, w18, [x9, #-80]
    stp  w19, w20, [x9, #-72]
    stp  w21, w22, [x9, #-64]
    stp  w23, w24, [x9, #-56]
    stp  w25, w26, [x9, #-48]
    stur w27, [x9, #-40]
    str  w1, [x28, #9144]
    str  w12, [x28, #8848]
    sub  x1, x9, #0x2e8
    ldr  w2, [x28, #8452]
    ldr  w4, [x28, #9152]
    ldr  w3, [x1, w2, uxtw #2]
    tbz  w3, #5, 1f
    and  w3, w3, #0xffffffdf
    orr  w0, w0, #0x1
1:
    str  w3, [x28, #9152]
    str  w3, [x28, #9144]
    str  w0, [x28, #9148]
    mov  x0, x28
    bl   arm_bank_switch
    ldr  w0, [x28, #8456]
    ldr  w1, [x28, #9152]
    cbz  w0, 2f
    tbnz w1, #7, 2f
    mov  x0, x28
    bl   arm_irq_raise
    ldr  x0, [x28, #8832]
    add  x9, x28, #0x3d0
    add  x9, x9, #0x2, lsl #12
    ldp  x10, x11, [x9, #-120]
    ldr  w1, [x28, #9144]
    ldp  w13, w14, [x9, #-96]
    ldp  w15, w16, [x9, #-88]
    ldp  w17, w18, [x9, #-80]
    ldp  w19, w20, [x9, #-72]
    ldp  w21, w22, [x9, #-64]
    ldp  w23, w24, [x9, #-56]
    ldp  w25, w26, [x9, #-48]
    ldur w27, [x9, #-40]
    msr  nzcv, x1
    ldr  w12, [x28, #8848]
    br   x0
2:
    ldr  w1, [x28, #9148]
    mov  x0, x28
    bl   jit_cache_lookup_or_compile
    add  x9, x28, #0x3d0
    add  x9, x9, #0x2, lsl #12
    ldp  x10, x11, [x9, #-120]
    ldr  w1, [x28, #9144]
    ldp  w13, w14, [x9, #-96]
    ldp  w15, w16, [x9, #-88]
    ldp  w17, w18, [x9, #-80]
    ldp  w19, w20, [x9, #-72]
    ldp  w21, w22, [x9, #-64]
    ldp  w23, w24, [x9, #-56]
    ldp  w25, w26, [x9, #-48]
    ldur w27, [x9, #-40]
    msr  nzcv, x1
    ldr  w12, [x28, #8848]
    br   x0
    .size jit_irq_enter, .-jit_irq_enter

    .text
    .align 2
    .globl jit_exit_store_pc_flags
    .type  jit_exit_store_pc_flags, %function
jit_exit_store_pc_flags:
    str  w0, [x28, #9148]
    ldr  w0, [x28, #9152]
    mrs  x1, nzcv
    bfxil w1, w0, #0, #28
    str  w1, [x28, #9152]
    mov  x30, #0x0
    ldr  x0, [x28, #9136]
    mov  w12, #0xffffffff
    br   x0
    .size jit_exit_store_pc_flags, .-jit_exit_store_pc_flags

    .text
    .align 2
    .globl jit_cp15_control_hook
    .type  jit_cp15_control_hook, %function
    .globl jit_vramcnt_write_flush_recompile
    .type  jit_vramcnt_write_flush_recompile, %function
jit_cp15_control_hook:
    stp  w12, w13, [x9, #-224]
    stp  w14, w15, [x9, #-216]
    stp  w16, w17, [x9, #-208]
    str  w18, [x28, #8968]
    str  x30, [x28, #9064]
    mrs  x0, nzcv
    str  w0, [x28, #9044]
    ldr  x0, [x28, #8784]
    mov  w2, #0x85
    ldr  w3, [x0, #20]
    orr  w2, w2, #0xff000
    and  w1, w1, w2
    bic  w3, w3, w2
    orr  w1, w1, w3
    str  w1, [x0, #20]
    bl   cp15_control_apply
    cbnz w0, L_jit_vramcnt_write_flush_recompile_1
    add  x9, x28, #0x3d0
    add  x9, x9, #0x2, lsl #12
    ldp  x10, x11, [x9, #-120]
    ldp  w12, w13, [x9, #-224]
    ldp  w14, w15, [x9, #-216]
    ldp  w16, w17, [x9, #-208]
    ldr  w18, [x28, #8968]
    ldr  x30, [x28, #9064]
    ldr  w0, [x28, #9044]
    msr  nzcv, x0
    ret
jit_vramcnt_write_flush_recompile:
L_jit_vramcnt_write_flush_recompile_1:
    add  x9, x28, #0x3d0
    add  x9, x9, #0x2, lsl #12
    ldr  x0, [x28, #9064]
    ldr  x1, [x28, #9056]
    bl   jit_host_to_ds_addr
    str  w0, [x28, #9148]
    mov  x0, x28
    mov  w1, #0xffffffff
    bl   jit_cache_flush
    ldr  w1, [x28, #9148]
    mov  x0, x28
    bl   jit_cache_lookup_or_compile
    add  x9, x28, #0x3d0
    add  x9, x9, #0x2, lsl #12
    ldp  x10, x11, [x9, #-120]
    ldp  w12, w13, [x9, #-224]
    ldp  w14, w15, [x9, #-216]
    ldp  w16, w17, [x9, #-208]
    ldr  w18, [x28, #8968]
    ldr  w1, [x28, #9044]
    msr  nzcv, x1
    br   x0
    .size jit_cp15_control_hook, .-jit_cp15_control_hook
    .size jit_vramcnt_write_flush_recompile, .-jit_vramcnt_write_flush_recompile

    .text
    .align 2
    .globl jit_cp15_dtcm_hook
    .type  jit_cp15_dtcm_hook, %function
jit_cp15_dtcm_hook:
    stp  w12, w13, [x9, #-224]
    stp  w14, w15, [x9, #-216]
    stp  w16, w17, [x9, #-208]
    str  w18, [x28, #8968]
    str  x30, [x28, #9064]
    mrs  x0, nzcv
    str  w0, [x28, #9044]
    ldr  x0, [x28, #8784]
    and  w1, w1, #0xfffffffe
    and  w1, w1, #0xfffff03f
    str  w1, [x0, #24]
    bl   cp15_dtcm_apply
    cbnz w0, L_jit_cp15_dtcm_hook_exit_1
    add  x9, x28, #0x3d0
    add  x9, x9, #0x2, lsl #12
    ldp  x10, x11, [x9, #-120]
    ldp  w12, w13, [x9, #-224]
    ldp  w14, w15, [x9, #-216]
    ldp  w16, w17, [x9, #-208]
    ldr  w18, [x28, #8968]
    ldr  x30, [x28, #9064]
    ldr  w0, [x28, #9044]
    msr  nzcv, x0
    ret
L_jit_cp15_dtcm_hook_exit_1:
    adrp x16, :got:jit_vramcnt_write_flush_recompile
    ldr  x16, [x16, :got_lo12:jit_vramcnt_write_flush_recompile]
    br   x16
    .size jit_cp15_dtcm_hook, .-jit_cp15_dtcm_hook

    .text
    .align 2
    .globl jit_cp15_itcm_hook
    .type  jit_cp15_itcm_hook, %function
jit_cp15_itcm_hook:
    stp  w12, w13, [x9, #-224]
    stp  w14, w15, [x9, #-216]
    stp  w16, w17, [x9, #-208]
    str  w18, [x28, #8968]
    str  x30, [x28, #9064]
    mrs  x0, nzcv
    str  w0, [x28, #9044]
    ldr  x0, [x28, #8784]
    and  w1, w1, #0x3e
    str  w1, [x0, #28]
    bl   cp15_itcm_apply
    add  x9, x28, #0x3d0
    add  x9, x9, #0x2, lsl #12
    ldp  x10, x11, [x9, #-120]
    ldp  w12, w13, [x9, #-224]
    ldp  w14, w15, [x9, #-216]
    ldp  w16, w17, [x9, #-208]
    ldr  w18, [x28, #8968]
    ldr  x30, [x28, #9064]
    ldr  w0, [x28, #9044]
    msr  nzcv, x0
    ret
    .size jit_cp15_itcm_hook, .-jit_cp15_itcm_hook

    .text
    .align 2
    .globl jit_bank_select_hook
    .type  jit_bank_select_hook, %function
jit_bank_select_hook:
    mrs  x0, nzcv
    stp  w13, w14, [x9, #-96]
    stp  w15, w16, [x9, #-88]
    stp  w17, w18, [x9, #-80]
    stp  w19, w20, [x9, #-72]
    stp  w21, w22, [x9, #-64]
    stp  w23, w24, [x9, #-56]
    stp  w25, w26, [x9, #-48]
    stur w27, [x9, #-40]
    str  w0, [x28, #9144]
    str  w12, [x28, #8848]
    str  x30, [x28, #9064]
    mov  x0, x28
    bl   arm_bank_select
    add  x9, x28, #0x3d0
    add  x9, x9, #0x2, lsl #12
    ldp  x10, x11, [x9, #-120]
    ldr  w0, [x28, #9144]
    ldp  w13, w14, [x9, #-96]
    ldp  w15, w16, [x9, #-88]
    ldp  w17, w18, [x9, #-80]
    ldp  w19, w20, [x9, #-72]
    ldp  w21, w22, [x9, #-64]
    ldp  w23, w24, [x9, #-56]
    ldp  w25, w26, [x9, #-48]
    ldur w27, [x9, #-40]
    msr  nzcv, x0
    ldr  w12, [x28, #8848]
    ldr  x30, [x28, #9064]
    ret
    .size jit_bank_select_hook, .-jit_bank_select_hook

    .text
    .align 2
    .globl jit_shift_lsl_reg
    .type  jit_shift_lsl_reg, %function
jit_shift_lsl_reg:
    and  w4, w3, #0xe0
    cbz  w3, .Lh85024_end
    cbnz w4, .Lh85024_large
    mov  w4, #0x20
    sub  w4, w4, w3
    lsr  w4, w1, w4
    lsl  w1, w1, w3
    bfi  w2, w4, #29, #1
.Lh85024_end:
    ret
.Lh85024_large:
    sub  w4, w3, #0x20
    cbnz w4, .Lh85024_greater
    bfi  w2, w1, #29, #1
    mov  w1, wzr
    ret
.Lh85024_greater:
    bfi  w2, wzr, #29, #1
    mov  w1, wzr
    ret
    .size jit_shift_lsl_reg, .-jit_shift_lsl_reg

    .text
    .align 2
    .globl jit_shift_lsr_reg
    .type  jit_shift_lsr_reg, %function
jit_shift_lsr_reg:
    and  w4, w3, #0xe0
    cbz  w3, .Lh85068_end
    cbnz w4, .Lh85068_large
    sub  w4, w3, #0x1
    lsr  w4, w1, w4
    lsr  w1, w1, w3
    bfi  w2, w4, #29, #1
.Lh85068_end:
    ret
.Lh85068_large:
    sub  w4, w3, #0x20
    cbnz w4, .Lh85068_greater
    lsr  w4, w1, #31
    bfi  w2, w4, #29, #1
    mov  w1, wzr
    ret
.Lh85068_greater:
    bfi  w2, wzr, #29, #1
    mov  w1, wzr
    ret
    .size jit_shift_lsr_reg, .-jit_shift_lsr_reg

    .text
    .align 2
    .globl jit_shift_asr_reg
    .type  jit_shift_asr_reg, %function
jit_shift_asr_reg:
    and  w4, w3, #0xe0
    cbz  w3, .Lh850ac_end
    cbnz w4, .Lh850ac_large
    sub  w4, w3, #0x1
    lsr  w4, w1, w3
    asr  w1, w1, w3
    bfi  w2, w4, #29, #1
.Lh850ac_end:
    ret
.Lh850ac_large:
    lsr  w4, w1, #31
    bfi  w2, w4, #29, #1
    asr  w1, w1, #31
    ret
    .size jit_shift_asr_reg, .-jit_shift_asr_reg

    .text
    .align 2
    .globl jit_shift_ror_reg
    .type  jit_shift_ror_reg, %function
jit_shift_ror_reg:
    sub  w4, w3, #0x1
    cbz  w3, .Lh850dc_end
    lsr  w4, w1, w4
    ror  w1, w1, w3
    bfi  w2, w4, #29, #1
.Lh850dc_end:
    ret
    .size jit_shift_ror_reg, .-jit_shift_ror_reg



.text
.globl jit_hook_mem_read32
.type jit_hook_mem_read32, %function
jit_hook_mem_read32:
    lsr  w1, w0, #11
    and  w2, w0, #3
    cbnz w2, 1f
    ldr  x1, [x9, w1, uxtw #3]
    lsl  x1, x1, #2
    cbz  x1, 2f
    ldr  w0, [x1, w0, uxtw]
    ret
2:
    mov  w1, w0
    mov  x0, x9
    stp  w12, w13, [x9, #-224]
    stp  w14, w15, [x9, #-216]
    stp  w16, w17, [x9, #-208]
    str  w18, [x28, #8968]
    str  x30, [x28, #9064]
    mrs  x2, nzcv
    str  w2, [x28, #9044]
    str  w12, [x28, #8848]
    bl   bus_read32_slow
    add  x9, x28, #0x3d0
    add  x9, x9, #0x2, lsl #12
    ldp  x10, x11, [x9, #-120]
    ldp  w12, w13, [x9, #-224]
    ldp  w14, w15, [x9, #-216]
    ldp  w16, w17, [x9, #-208]
    ldr  w18, [x28, #8968]
    ldr  x30, [x28, #9064]
    ldr  w2, [x28, #9044]
    msr  nzcv, x2
    ret
1:
    str  x30, [x28, #9008]
    str  w19, [x28, #9016]
    lsl  w19, w2, #3
    and  w0, w0, #0xfffffffc
    bl   jit_hook_mem_read32
    ror  w0, w0, w19
    ldr  w19, [x28, #9016]
    ldr  x30, [x28, #9008]
    ret
.size jit_hook_mem_read32, .-jit_hook_mem_read32

    .text
    .align 2
    .globl script_lua_register_dual_slot_tail
    .type  script_lua_register_dual_slot_tail, %function
script_lua_register_dual_slot_tail:
    mov  x19, x0
    b    jit_scheduler_return_dispatch
    .size script_lua_register_dual_slot_tail, .-script_lua_register_dual_slot_tail

	.section .note.GNU-stack,"",%progbits
