.data
.align 2
rsqrt_table:
    .word 65536, 46341, 32768, 23170, 16384
    .word 11585,  8192,  5793,  4096,  2896
    .word  2048,  1448,  1024,   724,   512
    .word   362,   256,   181,   128,    90
    .word    64,    45,    32,    23,    16
    .word    11,     8,     6,     4,     3
    .word     2,     1

.text
.globl clz
clz:
    li   t0, 32              # n = 32

    # 16-bit
    srli  t2, a0, 16         # y = x >> 16
    snez  t3, t2             # y != 0
    slli  t1, t3, 4          # t1 = 16 or 0
    sub   t0, t0, t1         # n -= t1
    srl   a0, a0, t1         # x >>= t1

    # 8-bit
    srli  t2, a0, 8
    snez  t3, t2
    slli  t1, t3, 3          # t1 = 8 or 0
    sub   t0, t0, t1
    srl   a0, a0, t1

    # 4-bit
    srli  t2, a0, 4
    snez  t3, t2
    slli  t1, t3, 2          # t1 = 4 or 0
    sub   t0, t0, t1
    srl   a0, a0, t1

    # 2-bit
    srli  t2, a0, 2
    snez  t3, t2
    slli  t1, t3, 1          # t1 = 2 or 0
    sub   t0, t0, t1
    srl   a0, a0, t1

    # 1-bit
    srli  t2, a0, 1
    snez  t3, t2
    sub   t0, t0, t3         # n -= 1
    srl   a0, a0, t3

    sub   a0, t0, a0         # return n - x
    ret

.globl __muldi3
__muldi3:
    mv  t0, a0              # a_lo
    mv  t1, a1              # a_hi
    mv  t2, a2              # b_lo
    mv  t3, a3              # b_hi
    
    li  a0, 0               # res_lo
    li  a1, 0               # res_hi

    or t4, t2, t3
    beqz t4, .L_muldi3_end  # if (b == 0) skip loop

.L_muldi3_loop:
    andi t5, t2, 1
    beqz t5, .L_muldi3_skip_add
    
    # res = res + a
    add a0, a0, t0
    sltu t5, a0, t0
    add a1, a1, t1
    add a1, a1, t5

.L_muldi3_skip_add:
    # b >>= 1 (64-bit shift)
    srli t2, t2, 1
    slli t5, t3, 31
    or  t2, t2, t5
    srli t3, t3, 1
    
    # a <<= 1 (64-bit shift)
    mv  t5, t0
    add t0, t0, t0
    sltu t5, t0, t5
    add t1, t1, t1
    add t1, t1, t5
    
    or t4, t2, t3
    bnez t4, .L_muldi3_loop # continue if (b > 0)

.L_muldi3_end:
    ret

.globl __lshrdi3
__lshrdi3:
    andi a2, a2, 63
    beqz a2, .L_lshr_ret
    
    li  t0, 32
    blt a2, t0, .L_lshr_lt32

    # b >= 32
    sub a2, a2, t0
    srl a0, a1, a2
    li  a1, 0
    j  .L_lshr_ret

.L_lshr_lt32:
    # b < 32
    sub t1, t0, a2         # t1 = 32 - b
    sll t2, a1, t1         # t2 = in.hi << (32 - b)
    srl t1, a0, a2         # t1 = in.lo >> b
    srl a1, a1, a2         # out.hi = in.hi >> b
    or  a0, t2, t1         # out.lo = t2 | t1

.L_lshr_ret:
    ret

.globl fast_rsqrt
fast_rsqrt:
    addi sp, sp, -44
    sw  ra, 40(sp)
    sw  s0, 36(sp)         # s0 = x
    sw  s1, 32(sp)         # s1 = exp
    sw  s2, 28(sp)         # s2 = y_base
    sw  s3, 24(sp)         # s3 = y
    sw  s4, 20(sp)         # s4 = y_next
    sw  s6, 16(sp)         # s6 = y64_lo
    sw  s7, 12(sp)         # s7 = y64_hi
    sw  s8, 8(sp)          # s8 = term_lo
    sw  s9, 4(sp)          # s9 = term_hi
    sw  s10, 0(sp)         # s10 = tmp

    mv  s0, a0
    bnez s0, .L_frsqrt_not_zero
    li  a0, -1
    j  .L_frsqrt_epilogue

.L_frsqrt_not_zero:

    mv  a0, s0
    call clz
    li  t0, 31
    sub s1, t0, a0

    li  t0, 32
    bne s1, t0, .L_frsqrt_exp_ok
    li  s1, 31
.L_frsqrt_exp_ok:

    la  t0, rsqrt_table
    slli t1, s1, 2
    add t0, t0, t1
    lw  s2, 0(t0)

    li  t0, 31
    bne s1, t0, .L_frsqrt_interp
    mv  s3, s2
    j  .L_frsqrt_newton

.L_frsqrt_interp:
    la  t0, rsqrt_table
    addi t1, s1, 1
    slli t1, t1, 2
    add t0, t0, t1
    lw  s4, 0(t0)

    # power_of_2 = 1U << exp
    li  t0, 1
    sll t3, t0, s1         # t3 = power_of_2

    # (uint64_t)x - power_of_2
    sub a0, s0, t3
    slt a1, s0, t3
    neg a1, a1
    
    # << 16
    slli t0, a0, 16         # new_lo
    srli t1, a0, 16         # new_hi part 1
    slli t2, a1, 16         # new_hi part 2
    or  t1, t1, t2         # new_hi
    mv  a0, t0
    mv  a1, t1
    
    mv  a2, s1
    call __lshrdi3
    mv  s10, a0          # s10 = fraction_q16 (low)

    # delta_y = (uint64_t)y_base - y_next;
    sub t0, s2, s4
    li  t1, 0
    
    # (delta_y * fraction_q16)
    mv  a0, t0
    mv  a1, t1
    mv  a2, s10
    li  a3, 0
    call __muldi3
    
    # >> 16
    srli t0, a0, 16         # new_lo part 1
    slli t1, a1, 16         # new_lo part 2
    or  a0, t0, t1         # new_lo
    srli a1, a1, 16         # new_hi
    
    # y = y_base - (uint32_t)result
    sub s3, s2, a0
    
.L_frsqrt_newton:
    mv  s6, s3
    li  s7, 0
    
    # y2 = y64 * y64
    mv  a0, s6
    mv  a1, s7
    mv  a2, s6
    mv  a3, s7
    call __muldi3
    sw  a0, 16(sp)         # s6 (tmp) = y2_lo
    sw  a1, 12(sp)         # s7 (tmp) = y2_hi

    # xy2 = ((uint64_t)x * y2)
    mv  a0, s0
    li  a1, 0
    lw  a2, 16(sp)
    lw  a3, 12(sp)
    call __muldi3
    
    # >> 16
    srli t0, a0, 16
    slli t1, a1, 16
    or  a0, t0, t1
    srli a1, a1, 16
    
    # term = (3U << 16) - (uint32_t)xy2;
    li  s8, 196608
    li  s9, 0
    
    sub a0, s8, a0
    sltu t0, s8, a0
    sub a1, s9, t0
    mv  s8, a0
    mv  s9, a1

    # y = (uint32_t)((y64 * term) >> 17);
    mv  a0, s3
    li  a1, 0
    mv  a2, s8
    mv  a3, s9
    call __muldi3
    
    # >> 17
    srli t0, a0, 17         # new_lo part 1
    slli t1, a1, 15         # new_lo part 2 (32-17)
    or  a0, t0, t1         # new_lo
    srli a1, a1, 17         # new_hi
    
.L_frsqrt_epilogue:
    lw  ra, 40(sp)
    lw  s0, 36(sp)
    lw  s1, 32(sp)
    lw  s2, 28(sp)
    lw  s3, 24(sp)
    lw  s4, 20(sp)
    lw  s6, 16(sp)
    lw  s7, 12(sp)
    lw  s8, 8(sp)
    lw  s9, 4(sp)
    lw  s10, 0(sp)
    addi sp, sp, 44
    ret

.globl newton_step
newton_step:
    addi sp, sp, -20
    sw  ra, 16(sp)
    sw  s0, 12(sp)         # s0 = *rec_inv_sqrt
    sw  s1, 8(sp)         # s1 = x
    sw  s2, 4(sp)         # s2 = invsqrt
    sw  s3, 0(sp)         # s3 = invsqrt2
    
    mv  s0, a0
    mv  s1, a1
    lw  s2, 0(s0)

    # invsqrt2 = ((uint64_t)invsqrt * invsqrt) >> 32;
    mv  a0, s2
    li  a1, 0
    mv  a2, s2
    li  a3, 0
    call __muldi3
    mv  s3, a1

    # val = (3LL << 32) - ((uint64_t)x * invsqrt2);
    mv  a0, s1
    li  a1, 0
    mv  a2, s3
    srai a3, s3, 31
    call __muldi3
    
    li  t0, 0
    li  t1, 3
    sub a0, t0, a0
    sltu t2, t0, a0
    sub a1, t1, a1
    sub a1, a1, t2

    # val >>= 2
    srli t0, a0, 2
    slli t1, a1, 30         # 32-2
    or  a0, t0, t1
    srli a1, a1, 2

    # val = (val * invsqrt) >> 31
    mv  t0, a0
    mv  t1, a1
    mv  a0, s2
    li  a1, 0
    mv  a2, t0
    mv  a3, t1
    call __muldi3
    
    # >> 31
    srli t0, a0, 31
    slli t1, a1, 1         # 32-31
    or  a0, t0, t1
    srli a1, a1, 31
    
    sw  a0, 0(s0)

    lw  ra, 16(sp)
    lw  s0, 12(sp)
    lw  s1, 8(sp)
    lw  s2, 4(sp)
    lw  s3, 0(sp)
    addi sp, sp, 20
    ret

.globl fast_distance_3d
fast_distance_3d:
    addi sp, sp, -28
    sw  ra, 24(sp)
    sw  s0, 20(sp)
    sw  s1, 16(sp)
    sw  s2, 12(sp)
    sw  s3, 8(sp)
    sw  s4, 4(sp)
    sw  s5, 0(sp)

    mv  s0, a0
    mv  s1, a1
    mv  s2, a2
    
    # dx*dx
    mv  a0, s0
    srai a1, s0, 31
    mv  a2, s0
    srai a3, s0, 31
    call __muldi3
    mv  s3, a0
    mv  s4, a1

    # dy*dy
    mv  a0, s1
    srai a1, s1, 31
    mv  a2, s1
    srai a3, s1, 31
    call __muldi3
    
    add a0, s3, a0
    sltu t0, a0, s3
    add a1, s4, a1
    add a1, a1, t0
    mv  s3, a0
    mv  s4, a1
    
    # dz*dz
    mv  a0, s2
    srai a1, s2, 31
    mv  a2, s2
    srai a3, s2, 31
    call __muldi3

    add a0, s3, a0
    sltu t0, a0, s3
    add a1, s4, a1
    add a1, a1, t0
    mv  s3, a0
    mv  s4, a1

    bnez s4, .L_fdist_shift
    j  .L_fdist_no_shift
.L_fdist_shift:
    srli t0, s3, 16         # a0 is s3
    slli t1, s4, 16         # a1 is s4
    or  a0, t0, t1
    srli a1, s4, 16
    mv  s3, a0
    mv  s4, a1
.L_fdist_no_shift:
    
    # inv_dist = fast_rsqrt((uint32_t)dist_sq);
    mv  a0, s3
    call fast_rsqrt
    mv  s5, a0

    # dist = ((uint64_t)dist_sq * inv_dist) >> 16;
    mv  a0, s3
    mv  a1, s4
    mv  a2, s5
    li  a3, 0
    call __muldi3
    
    # >> 16
    srli t0, a0, 16
    slli t1, a1, 16
    or  a0, t0, t1
    srli a1, a1, 16
    
    lw  ra, 24(sp)
    lw  s0, 20(sp)
    lw  s1, 16(sp)
    lw  s2, 12(sp)
    lw  s3, 8(sp)
    lw  s4, 4(sp)
    lw  s5, 0(sp)
    addi sp, sp, 28
    ret