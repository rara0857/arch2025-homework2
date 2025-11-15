.text
.globl clz
clz:
    li   t0, 32         # n
    li   t1, 16         # c

clz_loop:   
    srl  t2, a0, t1     # uint32_t y = x >> c
    beqz t2, clz_end    # if (y)
    sub  t0, t0, t1     # n -= c    
    mv   a0, t2         # x = y
    
clz_end:
    srli t1, t1, 1
    bnez t1, clz_loop
    sub  a0, t0, a0
    ret


.globl uf8_decode
uf8_decode:
    andi t0, a0, 0x0f
    srli t1, a0, 4
    li   t2, 15
    sub  t2, t2, t1
    li   t3, 0x7FFF
    srl  t3, t3, t2
    slli t3, t3, 4
    sll  t0, t0, t1
    add  a0, t0, t3
    ret

.globl uf8_encode
uf8_encode:
    addi sp, sp, -8
    sw   ra, 4(sp)
    sw   s0, 0(sp)
    mv   s0, a0
    li   t0, 16
    blt  s0, t0, encode_ret
    
    mv   a0, s0
    jal  ra, clz
    li   t0, 31
    sub  t0, t0, a0          # t0 = msb
    li   t1, 0               # exponent
    li   t2, 0               # overflow

    # if (msb >= 5)
    li   t3, 5
    blt  t0, t3, find_exact_exponent

    # exponent = msb - 4
    # if (exponent > 15) exponent = 15
    addi t1, t0, -4
    li   t3, 15

    li   t3, 15
    ble  t1, t3, done1
    mv   t1, t3

done1:
    li   t3, 0               # e = 0

# for (uint8_t e = 0; e < exponent; e++)
# overflow = (overflow << 1) + 16
for_loop:    
    bge  t3, t1, adjust
    slli t2, t2, 1
    addi t2, t2, 16
    addi t3, t3, 1
    j    for_loop

# while (exponent > 0 && value < overflow)
adjust:
    blez t1, find_exact_exponent
    bge  s0, t2, find_exact_exponent
    addi t2, t2, -16
    srli t2, t2, 1
    addi t1, t1, -1
    j    adjust

find_exact_exponent:
    li   t3, 15
    bge  t1, t3, done2 # while (exponent < 15)
    slli t4, t2, 1
    addi t4, t4, 16
    blt  s0, t4, done2
    mv   t2, t4
    addi t1, t1, 1
    j    find_exact_exponent

# mantissa = (value - overflow) >> exponent
# return (exponent << 4) | mantissa;
done2:
    sub  t3, s0, t2
    srl  t3, t3, t1
    slli t1, t1, 4
    or   a0, t1, t3
    j    end

encode_ret:
    mv   a0, s0

end:
    lw   s0, 0(sp)
    lw   ra, 4(sp)
    addi sp, sp, 8
    ret