#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define REC_INV_SQRT_CACHE (16)
typedef uint8_t uf8;

static const uint32_t rsqrt_table[32] = {
    65536, 46341, 32768, 23170, 16384,  /* 2^0 to 2^4 */
    11585,  8192,  5793,  4096,  2896,  /* 2^5 to 2^9 */
     2048,  1448,  1024,   724,   512,  /* 2^10 to 2^14 */
      362,   256,   181,   128,    90,  /* 2^15 to 2^19 */
       64,    45,    32,    23,    16,  /* 2^20 to 2^24 */
       11,     8,     6,     4,     3,  /* 2^25 to 2^29 */
        2,     1                         /* 2^30, 2^31 */
};

static const uint32_t inv_sqrt_cache[REC_INV_SQRT_CACHE] = {
    ~0U,        ~0U, 3037000500, 2479700525,
    2147483647, 1920767767, 1753413056, 1623345051,
    1518500250, 1431655765, 1358187914, 1294981364,
    1239850263, 1191209601, 1147878294, 1108955788
};

static inline unsigned clz(uint32_t x)
{
    int n = 32, c = 16;
    do {
        uint32_t y = x >> c;
        if (y) {
            n -= c;
            x = y;
        }
        c >>= 1;
    } while (c);
    return n - x;
}

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
            : "a0", "a1", "a2", "a7");          \
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

/* Software multiplication for RV32I (no M extension) */
static uint32_t umul(uint32_t a, uint32_t b)
{
    uint32_t result = 0;
    while (b) {
        if (b & 1)
            result += a;
        a <<= 1;
        b >>= 1;
    }
    return result;
}

/* Provide __mulsi3 for GCC */
uint32_t __mulsi3(uint32_t a, uint32_t b)
{
    return umul(a, b);
}

uint64_t __muldi3(uint64_t a, uint64_t b)
{
    uint64_t result = 0;
    
    while (b > 0) {
        if (b & 1) {
            result += a;
        }
        a <<= 1;
        b >>= 1;
    }
    return result;
}
/* Simple integer to hex string conversion */
static void print_hex(unsigned long val)
{
    char buf[20];
    char *p = buf + sizeof(buf) - 1;

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

    *p = 'x';
    p--;
    *p = '0';
    p--;

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

uint32_t fast_rsqrt(uint32_t x)
{
    if (x == 0) return ~0U;

    int exp = 31 - clz(x);
    if (exp == 32) exp = 31;

    uint32_t y_base = rsqrt_table[exp];
    uint32_t y;
    
    if (exp == 31) {
        y = y_base;
    } 
    else {
        uint32_t y_next = rsqrt_table[exp + 1];
        uint32_t power_of_2 = 1U << exp;
        uint64_t fraction_q16 = (((uint64_t)x - power_of_2) << 16) >> exp;
        uint64_t delta_y = (uint64_t)y_base - y_next;
        y = y_base - (uint32_t)((delta_y * fraction_q16) >> 16);
    }
    
    uint64_t y64 = y; 
    uint64_t y2 = y64 * y64;
    uint64_t xy2 = ((uint64_t)x * y2) >> 16;
    uint64_t term = (3U << 16) - (uint32_t)xy2;
    y = (uint32_t)((y64 * term) >> 17);
    
    return y;
}

static void newton_step(uint32_t *rec_inv_sqrt, uint32_t x)
{
    uint32_t invsqrt, invsqrt2;
    uint64_t val;

    invsqrt = *rec_inv_sqrt;  /* Dereference pointer */
    invsqrt2 = ((uint64_t)invsqrt * invsqrt) >> 32;
    val = (3LL << 32) - ((uint64_t)x * invsqrt2);

    val >>= 2; /* Avoid overflow in following multiply */
    val = (val * invsqrt) >> 31;  /* Right shift by 31 = (32 - 2 + 1) */

    *rec_inv_sqrt = (uint32_t)val;
}

uint32_t fast_distance_3d(int32_t dx, int32_t dy, int32_t dz)
{
    uint64_t dist_sq = (uint64_t)dx * dx +
                       (uint64_t)dy * dy +
                       (uint64_t)dz * dz;

    if (dist_sq > 0xFFFFFFFF)
        dist_sq >>= 16;

    uint32_t inv_dist = fast_rsqrt((uint32_t)dist_sq);

    /* sqrt(x) = x / sqrt(x) = x * (1/sqrt(x)) */
    uint64_t dist = ((uint64_t)dist_sq * inv_dist) >> 16;

    return (uint32_t)dist;
}

int main() {
    int num = 16;
    uint64_t start_cycles, end_cycles, cycles_elapsed;
    uint64_t start_instret, end_instret, instret_elapsed;

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