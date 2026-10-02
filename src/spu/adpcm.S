    .text
    .align 2
    .globl spu_adpcm_decode_block
    .type  spu_adpcm_decode_block, %function
spu_adpcm_decode_block:
    adrp x16, :got:spu_adpcm_tables
    ldr  x10, [x16, :got_lo12:spu_adpcm_tables]
    b    spu_adpcm_decode_block_body
    .size spu_adpcm_decode_block, .-spu_adpcm_decode_block

    .text
    .align 2
    .type  spu_adpcm_decode_block_body, %function
spu_adpcm_decode_block_body:
    ldr w8, [x0, #144]
    ldr x9, [x0, #160]
    ldrb w12, [x0, #192]
    lsr x14, x8, #1
    add w15, w8, #0x8
    and x13, x8, #0x3f
    ldr w9, [x9, x14]
    str w15, [x0, #144]
    add x8, x0, x13, lsl #1
    ldrsh w13, [x10, x12, lsl #1]
    lsl w16, w9, #29
    ldrsh w11, [x0, #186]
    sbfx w14, w9, #0, #1
    sbfx w15, w9, #1, #1
    and w16, w13, w16, asr #31
    and w14, w14, w13, lsr #2
    and w15, w15, w13, lsr #1
    add w13, w16, w13, lsr #3
    add w13, w13, w14
    add w13, w13, w15
    tbnz w9, #3, .Lnibble0_add
    sub w11, w11, w13
    mov w13, #0xffff8001
    cmp w11, w13
    csel w13, w11, w13, gt
    b .Lnibble0_next
.Lnibble0_add:
    add w11, w13, w11
    mov w13, #0x7fff
    cmp w11, w13
    csel w13, w11, w13, lt
.Lnibble0_next:
    mov x11, x10
    and x14, x9, #0x7
    add x11, x11, #0xb2
    ldrsb w14, [x11, x14]
    mov w15, #0x58
    strh w13, [x8]
    lsl w17, w9, #25
    add w12, w12, w14
    cmp w12, #0x0
    csel w14, wzr, w15, lt
    cmp w12, #0x58
    csel w12, w14, w12, hi
    ldrsh w14, [x10, w12, uxtw #1]
    sbfx w15, w9, #4, #1
    sbfx w16, w9, #5, #1
    and w17, w14, w17, asr #31
    and w15, w15, w14, lsr #2
    and w16, w16, w14, lsr #1
    add w14, w17, w14, lsr #3
    add w14, w14, w15
    add w14, w14, w16
    tbnz w9, #7, .Lnibble1_add
    sub w13, w13, w14
    mov w14, #0xffff8001
    cmp w13, w14
    csel w13, w13, w14, gt
    b .Lnibble1_next
.Lnibble1_add:
    add w13, w14, w13
    mov w14, #0x7fff
    cmp w13, w14
    csel w13, w13, w14, lt
.Lnibble1_next:
    ubfx x14, x9, #4, #3
    ldrsb w14, [x11, x14]
    mov w15, #0x58
    strh w13, [x8, #2]
    lsl w17, w9, #21
    add w12, w12, w14
    cmp w12, #0x0
    csel w14, wzr, w15, lt
    cmp w12, #0x58
    csel w12, w14, w12, hi
    ldrsh w14, [x10, w12, uxtw #1]
    sbfx w15, w9, #8, #1
    sbfx w16, w9, #9, #1
    and w17, w14, w17, asr #31
    and w15, w15, w14, lsr #2
    and w16, w16, w14, lsr #1
    add w14, w17, w14, lsr #3
    add w14, w14, w15
    add w14, w14, w16
    tbnz w9, #11, .Lnibble2_add
    sub w13, w13, w14
    mov w14, #0xffff8001
    cmp w13, w14
    csel w13, w13, w14, gt
    b .Lnibble2_next
.Lnibble2_add:
    add w13, w14, w13
    mov w14, #0x7fff
    cmp w13, w14
    csel w13, w13, w14, lt
.Lnibble2_next:
    ubfx x14, x9, #8, #3
    ldrsb w14, [x11, x14]
    mov w15, #0x58
    strh w13, [x8, #4]
    lsl w17, w9, #17
    add w12, w12, w14
    cmp w12, #0x0
    csel w14, wzr, w15, lt
    cmp w12, #0x58
    csel w12, w14, w12, hi
    ldrsh w14, [x10, w12, uxtw #1]
    sbfx w15, w9, #12, #1
    sbfx w16, w9, #13, #1
    and w17, w14, w17, asr #31
    and w15, w15, w14, lsr #2
    and w16, w16, w14, lsr #1
    add w14, w17, w14, lsr #3
    add w14, w14, w15
    add w14, w14, w16
    tbnz w9, #15, .Lnibble3_add
    sub w13, w13, w14
    mov w14, #0xffff8001
    cmp w13, w14
    csel w13, w13, w14, gt
    b .Lnibble3_next
.Lnibble3_add:
    add w13, w14, w13
    mov w14, #0x7fff
    cmp w13, w14
    csel w13, w13, w14, lt
.Lnibble3_next:
    ubfx x14, x9, #12, #3
    ldrsb w14, [x11, x14]
    mov w15, #0x58
    strh w13, [x8, #6]
    lsl w17, w9, #13
    add w12, w12, w14
    cmp w12, #0x0
    csel w14, wzr, w15, lt
    cmp w12, #0x58
    csel w12, w14, w12, hi
    ldrsh w14, [x10, w12, uxtw #1]
    sbfx w15, w9, #16, #1
    sbfx w16, w9, #17, #1
    and w17, w14, w17, asr #31
    and w15, w15, w14, lsr #2
    and w16, w16, w14, lsr #1
    add w14, w17, w14, lsr #3
    add w14, w14, w15
    add w14, w14, w16
    tbnz w9, #19, .Lnibble4_add
    sub w13, w13, w14
    mov w14, #0xffff8001
    cmp w13, w14
    csel w13, w13, w14, gt
    b .Lnibble4_next
.Lnibble4_add:
    add w13, w14, w13
    mov w14, #0x7fff
    cmp w13, w14
    csel w13, w13, w14, lt
.Lnibble4_next:
    ubfx x14, x9, #16, #3
    ldrsb w14, [x11, x14]
    mov w15, #0x58
    strh w13, [x8, #8]
    lsl w17, w9, #9
    add w12, w12, w14
    cmp w12, #0x0
    csel w14, wzr, w15, lt
    cmp w12, #0x58
    csel w12, w14, w12, hi
    ldrsh w14, [x10, w12, uxtw #1]
    sbfx w15, w9, #20, #1
    sbfx w16, w9, #21, #1
    and w17, w14, w17, asr #31
    and w15, w15, w14, lsr #2
    and w16, w16, w14, lsr #1
    add w14, w17, w14, lsr #3
    add w14, w14, w15
    add w14, w14, w16
    tbnz w9, #23, .Lnibble5_add
    sub w13, w13, w14
    mov w14, #0xffff8001
    cmp w13, w14
    csel w13, w13, w14, gt
    b .Lnibble5_next
.Lnibble5_add:
    add w13, w14, w13
    mov w14, #0x7fff
    cmp w13, w14
    csel w13, w13, w14, lt
.Lnibble5_next:
    ubfx x14, x9, #20, #3
    ldrsb w14, [x11, x14]
    mov w15, #0x58
    strh w13, [x8, #10]
    lsl w17, w9, #5
    add w12, w12, w14
    cmp w12, #0x0
    csel w14, wzr, w15, lt
    cmp w12, #0x58
    csel w12, w14, w12, hi
    ldrsh w14, [x10, w12, uxtw #1]
    sbfx w15, w9, #24, #1
    sbfx w16, w9, #25, #1
    and w17, w14, w17, asr #31
    and w15, w15, w14, lsr #2
    and w16, w16, w14, lsr #1
    add w14, w17, w14, lsr #3
    add w14, w14, w15
    add w14, w14, w16
    tbnz w9, #27, .Lnibble6_add
    sub w13, w13, w14
    mov w14, #0xffff8001
    cmp w13, w14
    csel w13, w13, w14, gt
    b .Lnibble6_next
.Lnibble6_add:
    add w13, w14, w13
    mov w14, #0x7fff
    cmp w13, w14
    csel w13, w13, w14, lt
.Lnibble6_next:
    ubfx x14, x9, #24, #3
    ldrsb w14, [x11, x14]
    mov w15, #0x58
    strh w13, [x8, #12]
    lsl w16, w9, #1
    add w12, w12, w14
    cmp w12, #0x0
    csel w14, wzr, w15, lt
    cmp w12, #0x58
    csel w12, w14, w12, hi
    ldrsh w10, [x10, w12, uxtw #1]
    sbfx w14, w9, #28, #1
    sbfx w15, w9, #29, #1
    and w16, w10, w16, asr #31
    and w14, w14, w10, lsr #2
    and w15, w15, w10, lsr #1
    add w10, w16, w10, lsr #3
    add w10, w10, w14
    add w10, w10, w15
    tbnz w9, #31, .Lnibble7_add
    sub w10, w13, w10
    mov w13, #0xffff8001
    cmp w10, w13
    csel w10, w10, w13, gt
    b .Lnibble7_next
.Lnibble7_add:
    add w10, w10, w13
    mov w13, #0x7fff
    cmp w10, w13
    csel w10, w10, w13, lt
.Lnibble7_next:
    ubfx x9, x9, #28, #3
    ldrsb w9, [x11, x9]
    strh w10, [x8, #14]
    mov w11, #0x58
    strh w10, [x0, #186]
    add w8, w12, w9
    cmp w8, #0x0
    csel w9, wzr, w11, lt
    cmp w8, #0x58
    csel w8, w9, w8, hi
    strb w8, [x0, #192]
    ret
    .size spu_adpcm_decode_block_body, .-spu_adpcm_decode_block_body
