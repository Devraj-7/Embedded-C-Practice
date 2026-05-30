/*
 * bit_manipulation.c
 * 
 * Common bit manipulation operations used in embedded firmware.
 * These patterns appear in GPIO configuration, register access,
 * flag handling, and protocol implementations.
 *
 * Author : Devraj Pravin Patil
 * Board  : STM32 / ESP32 / any ARM Cortex-M MCU
 * Tool   : Any C compiler (GCC ARM, Keil, IAR)
 */

#include <stdint.h>
#include <stdio.h>

/* ----------------------------------------------------------------
 * CORE BIT OPERATIONS
 * ---------------------------------------------------------------- */

/* Set bit n in a register */
#define BIT_SET(reg, n)         ((reg) |=  (1U << (n)))

/* Clear bit n in a register */
#define BIT_CLEAR(reg, n)       ((reg) &= ~(1U << (n)))

/* Toggle bit n in a register */
#define BIT_TOGGLE(reg, n)      ((reg) ^=  (1U << (n)))

/* Read bit n — returns 1 if set, 0 if clear */
#define BIT_READ(reg, n)        (((reg) >> (n)) & 1U)

/* Check if bit n is set — returns non-zero if set */
#define BIT_IS_SET(reg, n)      ((reg) &   (1U << (n)))


/* ----------------------------------------------------------------
 * MULTI-BIT FIELD OPERATIONS
 * Writing a value to a specific bit field without disturbing
 * other bits — common for GPIO mode, UART baud, timer prescaler.
 * ---------------------------------------------------------------- */

/*
 * Write 'val' into a field of 'width' bits starting at position 'pos'
 * Example: write 0b10 into bits 5:4 of a register
 *   FIELD_WRITE(reg, 4, 2, 0x2)
 */
#define FIELD_WRITE(reg, pos, width, val) \
    ((reg) = ((reg) & ~(((1U << (width)) - 1U) << (pos))) \
           | (((val) & ((1U << (width)) - 1U)) << (pos)))

/*
 * Read a field of 'width' bits starting at position 'pos'
 */
#define FIELD_READ(reg, pos, width) \
    (((reg) >> (pos)) & ((1U << (width)) - 1U))


/* ----------------------------------------------------------------
 * BIT TRICK FUNCTIONS
 * These come up in embedded interviews and real firmware work.
 * ---------------------------------------------------------------- */

/*
 * Check if n is a power of 2
 * Powers of 2 have exactly one bit set.
 * n & (n-1) clears the lowest set bit — result is 0 for powers of 2.
 * Used to validate buffer sizes, DMA transfer lengths, etc.
 */
uint8_t is_power_of_two(uint32_t n)
{
    return (n != 0U) && ((n & (n - 1U)) == 0U);
}

/*
 * Count the number of set bits in a value (Brian Kernighan's algorithm)
 * Each iteration clears the lowest set bit.
 * Used in CRC checks, parity calculation, error flag counting.
 */
uint8_t count_set_bits(uint32_t n)
{
    uint8_t count = 0;
    while (n) {
        n &= (n - 1U);   /* clear lowest set bit */
        count++;
    }
    return count;
}

/*
 * Get the position of the lowest set bit
 * Returns 0–31, or 32 if n == 0
 * Useful for finding the first pending interrupt flag.
 */
uint8_t lowest_set_bit_pos(uint32_t n)
{
    if (n == 0U) return 32U;
    uint8_t pos = 0;
    while ((n & 1U) == 0U) {
        n >>= 1;
        pos++;
    }
    return pos;
}

/*
 * Reverse the bits of an 8-bit value
 * Used in SPI LSB-first protocol handling.
 */
uint8_t reverse_bits_u8(uint8_t n)
{
    uint8_t result = 0;
    for (uint8_t i = 0; i < 8U; i++) {
        result = (result << 1) | (n & 1U);
        n >>= 1;
    }
    return result;
}

/*
 * Swap two variables using XOR — no temporary variable needed
 * Classic interview question. Works only for integer types.
 */
void xor_swap(uint32_t *a, uint32_t *b)
{
    if (a != b) {          /* guard: swapping a variable with itself */
        *a ^= *b;
        *b ^= *a;
        *a ^= *b;
    }
}


/* ----------------------------------------------------------------
 * PRACTICAL EMBEDDED EXAMPLES
 * Patterns taken directly from real register-level firmware.
 * ---------------------------------------------------------------- */

/*
 * Simulate a GPIO output register (8-bit for demonstration)
 * Bit 0 = LED1, Bit 1 = LED2, Bit 5 = RELAY, Bit 7 = STATUS_LED
 */
#define LED1_PIN        0
#define LED2_PIN        1
#define RELAY_PIN       5
#define STATUS_LED_PIN  7

void gpio_examples(void)
{
    uint8_t GPIO_ODR = 0x00;   /* output data register — all pins low */

    /* Turn on LED1 */
    BIT_SET(GPIO_ODR, LED1_PIN);
    printf("After SET   LED1  : 0x%02X (binary: ", GPIO_ODR);
    for (int i = 7; i >= 0; i--) printf("%d", (GPIO_ODR >> i) & 1);
    printf(")\n");

    /* Turn on RELAY */
    BIT_SET(GPIO_ODR, RELAY_PIN);
    printf("After SET   RELAY : 0x%02X\n", GPIO_ODR);

    /* Toggle STATUS_LED twice */
    BIT_TOGGLE(GPIO_ODR, STATUS_LED_PIN);
    printf("After TOGGLE LED  : 0x%02X\n", GPIO_ODR);
    BIT_TOGGLE(GPIO_ODR, STATUS_LED_PIN);
    printf("After TOGGLE LED  : 0x%02X\n", GPIO_ODR);

    /* Turn off LED1 */
    BIT_CLEAR(GPIO_ODR, LED1_PIN);
    printf("After CLEAR LED1  : 0x%02X\n", GPIO_ODR);

    /* Read RELAY state */
    printf("RELAY state       : %d\n", BIT_READ(GPIO_ODR, RELAY_PIN));
}

/*
 * Simulate writing a 2-bit GPIO mode field
 * STM32 GPIOx_MODER has 2 bits per pin:
 *   00 = Input, 01 = Output, 10 = Alternate function, 11 = Analog
 */
#define GPIO_MODE_INPUT     0x00U
#define GPIO_MODE_OUTPUT    0x01U
#define GPIO_MODE_ALTFUNC   0x02U
#define GPIO_MODE_ANALOG    0x03U

void gpio_mode_example(void)
{
    uint32_t GPIOA_MODER = 0x00000000UL;
    uint8_t  pin = 5;   /* PA5 */

    /* Set PA5 as output (bits 11:10 = 01) */
    FIELD_WRITE(GPIOA_MODER, pin * 2U, 2U, GPIO_MODE_OUTPUT);
    printf("MODER after PA5 output : 0x%08lX\n", (unsigned long)GPIOA_MODER);

    /* Read it back */
    uint32_t mode = FIELD_READ(GPIOA_MODER, pin * 2U, 2U);
    printf("PA5 mode reads back as : %lu (expect 1 = output)\n", (unsigned long)mode);

    /* Change PA5 to alternate function */
    FIELD_WRITE(GPIOA_MODER, pin * 2U, 2U, GPIO_MODE_ALTFUNC);
    printf("MODER after PA5 alt-fn : 0x%08lX\n", (unsigned long)GPIOA_MODER);
}


/* ----------------------------------------------------------------
 * MAIN — runs all examples
 * ---------------------------------------------------------------- */

int main(void)
{
    printf("=== Bit Manipulation — Embedded C Examples ===\n\n");

    printf("--- GPIO register operations ---\n");
    gpio_examples();

    printf("\n--- GPIO mode field write ---\n");
    gpio_mode_example();

    printf("\n--- Utility functions ---\n");
    printf("is_power_of_two(64)  : %d (expect 1)\n", is_power_of_two(64));
    printf("is_power_of_two(60)  : %d (expect 0)\n", is_power_of_two(60));
    printf("count_set_bits(0xAB) : %d (expect 5)\n", count_set_bits(0xAB));
    printf("lowest_set_bit(0x18) : %d (expect 3)\n", lowest_set_bit_pos(0x18));
    printf("reverse_bits(0b10110001) : 0x%02X (expect 0x8D)\n", reverse_bits_u8(0xB1));

    uint32_t x = 42, y = 99;
    printf("\nBefore XOR swap: x=%lu y=%lu\n", (unsigned long)x, (unsigned long)y);
    xor_swap(&x, &y);
    printf("After  XOR swap: x=%lu y=%lu\n", (unsigned long)x, (unsigned long)y);

    return 0;
}
