#include <stdbool.h>
#include <stdint.h>
#include <string.h>

typedef uint8_t uf8;

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

extern uint32_t clz(uint32_t val);
extern uint32_t uf8_decode(uint32_t val);
extern uint32_t uf8_encode(uint32_t val);

/* Test encode/decode round-trip */
static bool test_uf8_codec(void)
{
    int32_t previous_value = -1;
    bool passed = true;

    for (int i = 0; i < 256; i++) {
        uint8_t fl = i;
        int32_t value = uf8_decode(fl);
        uint8_t fl2 = uf8_encode(value);

        if (fl != fl2) {
            print_hex(i);
            TEST_LOGGER(": produces value ");
            print_dec(value);
            TEST_LOGGER(" but encodes back to ");
            print_hex(fl2);
            TEST_LOGGER("\n");
            passed = false;
        }

        if (value <= previous_value) {
            passed = false;
            print_hex(fl);
            TEST_LOGGER(": value ");
            print_dec(value);
            TEST_LOGGER(" <= previous_value ");
            print_dec(previous_value);
            TEST_LOGGER("\n");
        }
        previous_value = value;
    }
    return passed;
}

int main(void)
{
    uint64_t start_cycles, end_cycles, cycles_elapsed;
    uint64_t start_instret, end_instret, instret_elapsed;

    TEST_LOGGER("\n=== UF8 Codec===\n\n");

    start_cycles = get_cycles();
    start_instret = get_instret();

    bool all_passed = test_uf8_codec();

    end_cycles = get_cycles();
    end_instret = get_instret();
    
    cycles_elapsed = end_cycles - start_cycles;
    instret_elapsed = end_instret - start_instret;
    if(all_passed) {
        TEST_LOGGER("   UF8 Codec: ALL PASSED\n");
    }
    TEST_LOGGER("   Cycles: ");
    print_dec((unsigned long) cycles_elapsed);
    TEST_LOGGER("\n   Instructions: ");
    print_dec((unsigned long) instret_elapsed);
    TEST_LOGGER("\n");

    TEST_LOGGER("\n=== All Tests Completed ===\n");

    return 0;
}