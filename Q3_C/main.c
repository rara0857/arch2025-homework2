#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define REC_INV_SQRT_CACHE (16)
extern unsigned clz(uint32_t x);
extern uint32_t fast_rsqrt(uint32_t x);
extern void newton_step(uint32_t *rec_inv_sqrt, uint32_t x);
extern uint32_t fast_distance_3d(int32_t dx, int32_t dy, int32_t dz);

static const uint32_t inv_sqrt_cache[REC_INV_SQRT_CACHE] = {
    ~0U,        ~0U, 3037000500, 2479700525,
    2147483647, 1920767767, 1753413056, 1623345051,
    1518500250, 1431655765, 1358187914, 1294981364,
    1239850263, 1191209601, 1147878294, 1108955788
};

#define printstr(ptr, length)                   \
    do {                                        \
        asm volatile(                           \
            "add a7, x0, 0x40;"                 \
            "add a0, x0, 0x1;" /* stdout */     \
            "add a1, x0, %0;"                   \
            "mv a2, %1;" /* length character */ \
            "ecall;"                            \
            :                                   \
            : "r"(ptr), "r"(length)             \
            : "a0", "a1", "a2", "a7","memory");          \
    } while (0)

#define TEST_OUTPUT(msg, length) printstr(msg, length)

#define TEST_LOGGER(msg)                     \
    {                                        \
        char _msg[] = msg;                   \
        TEST_OUTPUT(_msg, sizeof(_msg) - 1); \
    }

extern uint64_t get_cycles(void);
extern uint64_t get_instret(void);

/* Software division for RV32I (no M extension) */
static unsigned long udiv(unsigned long dividend, unsigned long divisor)
{
    if (divisor == 0)
        return 0;

    unsigned long quotient = 0;
    unsigned long remainder = 0;

    for (int i = 31; i >= 0; i--) {
        remainder <<= 1;
        remainder |= (dividend >> i) & 1;

        if (remainder >= divisor) {
            remainder -= divisor;
            quotient |= (1UL << i);
        }
    }

    return quotient;
}

static unsigned long umod(unsigned long dividend, unsigned long divisor)
{
    if (divisor == 0)
        return 0;

    unsigned long remainder = 0;

    for (int i = 31; i >= 0; i--) {
        remainder <<= 1;
        remainder |= (dividend >> i) & 1;

        if (remainder >= divisor) {
            remainder -= divisor;
        }
    }

    return remainder;
}
static void print_hex(unsigned long val)
{
    char buf[20];
    char *p = buf + sizeof(buf) - 1;
    *p = '\n';
    p--;

    if (val == 0) {
        *p = '0';
        p--;
    } else {
        while (val > 0) {
            int digit = val & 0xf;
            *p = (digit < 10) ? ('0' + digit) : ('a' + digit - 10);
            p--;
            val >>= 4;
        }
    }

    p++;
    printstr(p, (buf + sizeof(buf) - p));
}

static void print_dec(unsigned long val)
{
    char buf[20];
    char *p = buf + sizeof(buf) - 1;

    if (val == 0) {
        *p = '0';
        p--;
    } else {
        while (val > 0) {
            *p = '0' + umod(val, 10);
            p--;
            val = udiv(val, 10);
        }
    }

    p++;
    printstr(p, (buf + sizeof(buf) - p));
}

int main() {
    volatile int num = 16;
    volatile uint64_t start_cycles, end_cycles, cycles_elapsed;
    volatile uint64_t start_instret, end_instret, instret_elapsed;

    TEST_LOGGER("\n=== Q3_C Test ===\n\n");
    TEST_LOGGER("Num: ");
    print_dec((unsigned long)num);
    TEST_LOGGER("\n");
    start_cycles = get_cycles();
    start_instret = get_instret();

    uint32_t result = fast_rsqrt(num);

    end_cycles = get_cycles();
    end_instret = get_instret();

    cycles_elapsed = end_cycles - start_cycles;
    instret_elapsed = end_instret - start_instret;

    TEST_LOGGER("Fast_rsqrt result (Q0.16): ");
    print_dec((unsigned long)result);
    TEST_LOGGER("\n\n");

    TEST_LOGGER("Cycles: ");
    print_dec((unsigned long)cycles_elapsed);
    TEST_LOGGER("\n");

    TEST_LOGGER("Instructions: ");
    print_dec((unsigned long)instret_elapsed);
    TEST_LOGGER("\n");

    TEST_LOGGER("\n=== All Tests Completed ===\n");
    return 0;
}