/*
 * bare_metal_stm32.c
 *
 * Bare-metal peripheral configuration on STM32F4 (no HAL, no CubeMX).
 * Directly accesses memory-mapped registers using pointers.
 * Demonstrates what HAL_GPIO_Init() and HAL_UART_Init() do underneath.
 *
 * Target : STM32F411RE (Nucleo-64)
 * Clock  : Default HSI 16 MHz (no PLL configured here)
 * Author : Devraj Pravin Patil
 */

#include <stdint.h>

/* ----------------------------------------------------------------
 * BASE ADDRESSES — STM32F4 Reference Manual, Table 1
 * ---------------------------------------------------------------- */
#define PERIPH_BASE         0x40000000UL
#define AHB1_BASE           (PERIPH_BASE + 0x00020000UL)
#define APB1_BASE           (PERIPH_BASE + 0x00000000UL)
#define APB2_BASE           (PERIPH_BASE + 0x00010000UL)

#define GPIOA_BASE          (AHB1_BASE + 0x0000UL)
#define GPIOB_BASE          (AHB1_BASE + 0x0400UL)
#define GPIOC_BASE          (AHB1_BASE + 0x0800UL)
#define RCC_BASE            (AHB1_BASE + 0x3800UL)
#define USART2_BASE         (APB1_BASE + 0x4400UL)


/* ----------------------------------------------------------------
 * REGISTER STRUCTS
 * Each peripheral's registers mapped as a struct at its base address.
 * volatile tells the compiler: don't cache — always read from memory.
 * ---------------------------------------------------------------- */

typedef struct {
    volatile uint32_t MODER;    /* mode register            offset 0x00 */
    volatile uint32_t OTYPER;   /* output type register     offset 0x04 */
    volatile uint32_t OSPEEDR;  /* output speed register    offset 0x08 */
    volatile uint32_t PUPDR;    /* pull-up/pull-down        offset 0x0C */
    volatile uint32_t IDR;      /* input data register      offset 0x10 */
    volatile uint32_t ODR;      /* output data register     offset 0x14 */
    volatile uint32_t BSRR;     /* bit set/reset register   offset 0x18 */
    volatile uint32_t LCKR;     /* lock register            offset 0x1C */
    volatile uint32_t AFR[2];   /* alternate function regs  offset 0x20 */
} GPIO_TypeDef;

typedef struct {
    volatile uint32_t CR;           /* clock control register   offset 0x00 */
    volatile uint32_t PLLCFGR;      /* PLL config               offset 0x04 */
    volatile uint32_t CFGR;         /* clock config             offset 0x08 */
    volatile uint32_t CIR;          /* clock interrupt          offset 0x0C */
    volatile uint32_t AHB1RSTR;     /* AHB1 peripheral reset    offset 0x10 */
             uint32_t RESERVED[7];
    volatile uint32_t AHB1ENR;      /* AHB1 clock enable        offset 0x30 */
             uint32_t RESERVED2[3];
    volatile uint32_t APB1ENR;      /* APB1 clock enable        offset 0x40 */
    volatile uint32_t APB2ENR;      /* APB2 clock enable        offset 0x44 */
} RCC_TypeDef;

typedef struct {
    volatile uint32_t SR;           /* status register          offset 0x00 */
    volatile uint32_t DR;           /* data register            offset 0x04 */
    volatile uint32_t BRR;          /* baud rate register       offset 0x08 */
    volatile uint32_t CR1;          /* control register 1       offset 0x0C */
    volatile uint32_t CR2;          /* control register 2       offset 0x10 */
    volatile uint32_t CR3;          /* control register 3       offset 0x14 */
} USART_TypeDef;


/* Peripheral pointers — cast base address to struct pointer */
#define GPIOA       ((GPIO_TypeDef  *) GPIOA_BASE)
#define GPIOB       ((GPIO_TypeDef  *) GPIOB_BASE)
#define GPIOC       ((GPIO_TypeDef  *) GPIOC_BASE)
#define RCC         ((RCC_TypeDef   *) RCC_BASE)
#define USART2      ((USART_TypeDef *) USART2_BASE)


/* ----------------------------------------------------------------
 * BIT FIELD HELPERS
 * ---------------------------------------------------------------- */
#define BIT_SET(reg, n)          ((reg) |=  (1UL << (n)))
#define BIT_CLEAR(reg, n)        ((reg) &= ~(1UL << (n)))
#define BIT_TOGGLE(reg, n)       ((reg) ^=  (1UL << (n)))
#define BIT_READ(reg, n)         (((reg) >> (n)) & 1UL)
#define FIELD_WRITE(reg, pos, width, val) \
    ((reg) = ((reg) & ~(((1UL << (width)) - 1UL) << (pos))) \
           | (((uint32_t)(val) & ((1UL << (width)) - 1UL)) << (pos)))


/* ----------------------------------------------------------------
 * SIMPLE DELAY (not accurate — use SysTick for real timing)
 * ---------------------------------------------------------------- */
static void delay_approx(volatile uint32_t count)
{
    while (count--);
}


/* ----------------------------------------------------------------
 * GPIO INIT — PA5 as push-pull output (onboard LED on Nucleo-64)
 *
 * Steps (same as what CubeMX generates under the hood):
 *   1. Enable GPIOA clock in RCC_AHB1ENR
 *   2. Set MODER bits for PA5 to 01 (output)
 *   3. Set OTYPER bit for PA5 to 0 (push-pull)
 *   4. Set OSPEEDR bits for PA5 to 00 (low speed)
 *   5. Set PUPDR bits for PA5 to 00 (no pull)
 * ---------------------------------------------------------------- */
void gpio_pa5_output_init(void)
{
    /* Step 1: enable GPIOA clock (bit 0 of AHB1ENR) */
    BIT_SET(RCC->AHB1ENR, 0);

    /* Small delay after clock enable — good practice before
     * accessing peripheral registers (clock stabilisation) */
    (void)RCC->AHB1ENR;

    /* Step 2: MODER — PA5 = output (01)
     * MODER has 2 bits per pin: bits 11:10 for PA5 */
    FIELD_WRITE(GPIOA->MODER, 5 * 2, 2, 0x01);

    /* Step 3: OTYPER — PA5 = push-pull (bit 5 = 0) */
    BIT_CLEAR(GPIOA->OTYPER, 5);

    /* Step 4: OSPEEDR — PA5 = low speed (00) */
    FIELD_WRITE(GPIOA->OSPEEDR, 5 * 2, 2, 0x00);

    /* Step 5: PUPDR — PA5 = no pull (00) */
    FIELD_WRITE(GPIOA->PUPDR, 5 * 2, 2, 0x00);
}


/* ----------------------------------------------------------------
 * GPIO WRITE — using BSRR for atomic set/reset
 * BSRR upper 16 bits = reset, lower 16 bits = set
 * Writing to BSRR is atomic — no read-modify-write needed,
 * which matters in ISR + main code sharing a GPIO port.
 * ---------------------------------------------------------------- */
void gpio_pa5_set(void)
{
    GPIOA->BSRR = (1UL << 5);          /* set bit 5 */
}

void gpio_pa5_reset(void)
{
    GPIOA->BSRR = (1UL << (5 + 16));   /* reset bit 5 (upper half) */
}

void gpio_pa5_toggle(void)
{
    BIT_TOGGLE(GPIOA->ODR, 5);
}


/* ----------------------------------------------------------------
 * USART2 INIT — 115200 baud, 8N1, TX only (PA2 = USART2_TX)
 *
 * On Nucleo-64, USART2 TX/RX are connected to the ST-Link USB
 * virtual COM port — so this is what drives the serial monitor.
 * ---------------------------------------------------------------- */
void usart2_init(void)
{
    /* 1. Enable GPIOA and USART2 clocks */
    BIT_SET(RCC->AHB1ENR, 0);           /* GPIOA clock */
    BIT_SET(RCC->APB1ENR, 17);          /* USART2 clock (bit 17) */

    /* 2. Configure PA2 as alternate function (MODER = 10) */
    FIELD_WRITE(GPIOA->MODER, 2 * 2, 2, 0x02);

    /* 3. Set PA2 alternate function to AF7 (USART2)
     * AFR[0] covers pins 0–7, 4 bits per pin */
    FIELD_WRITE(GPIOA->AFR[0], 2 * 4, 4, 0x07);

    /* 4. Set baud rate
     * BRR = f_PCLK / baud = 16000000 / 115200 = 138.88 ≈ 0x008B
     * Mantissa = 138 (0x8A), Fraction = 0 for simplicity */
    USART2->BRR = 0x008BUL;

    /* 5. Enable USART, TX enable (CR1 bits 13 and 3) */
    BIT_SET(USART2->CR1, 13);           /* UE — USART enable */
    BIT_SET(USART2->CR1, 3);            /* TE — transmitter enable */
}

/* Transmit a single byte over USART2 */
void usart2_send_byte(uint8_t byte)
{
    /* Wait until TXE (transmit data register empty) — bit 7 of SR */
    while (!BIT_READ(USART2->SR, 7));
    USART2->DR = byte;
}

/* Transmit a null-terminated string */
void usart2_send_string(const char *str)
{
    while (*str) {
        usart2_send_byte((uint8_t)*str);
        str++;
    }
}


/* ----------------------------------------------------------------
 * GPIO INPUT — PC13 as input with pull-up (onboard button Nucleo-64)
 * ---------------------------------------------------------------- */
void gpio_pc13_input_init(void)
{
    /* Enable GPIOC clock (bit 2 of AHB1ENR) */
    BIT_SET(RCC->AHB1ENR, 2);

    /* MODER — PC13 = input (00) — default, but set explicitly */
    FIELD_WRITE(GPIOC->MODER, 13 * 2, 2, 0x00);

    /* PUPDR — PC13 = pull-up (01) */
    FIELD_WRITE(GPIOC->PUPDR, 13 * 2, 2, 0x01);
}

/* Read PC13 — returns 1 if HIGH, 0 if LOW */
uint8_t gpio_pc13_read(void)
{
    return (uint8_t)BIT_READ(GPIOC->IDR, 13);
}


/* ----------------------------------------------------------------
 * MAIN — blinks PA5 LED and sends status over USART2
 * ---------------------------------------------------------------- */
int main(void)
{
    gpio_pa5_output_init();
    gpio_pc13_input_init();
    usart2_init();

    usart2_send_string("Bare metal STM32 — starting\r\n");

    while (1) {
        if (!gpio_pc13_read()) {
            /* Button pressed (active low) — LED on, send message */
            gpio_pa5_set();
            usart2_send_string("Button pressed — LED ON\r\n");
            delay_approx(200000UL);
        } else {
            /* Button not pressed — toggle LED at ~1Hz */
            gpio_pa5_toggle();
            usart2_send_string("LED toggled\r\n");
            delay_approx(800000UL);
        }
    }

    return 0;
}
