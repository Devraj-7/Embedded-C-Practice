/*
 * uart_basics.c
 *
 * UART communication fundamentals in bare-metal Embedded C.
 * Covers polling-based TX/RX, basic ring buffer for received data,
 * and common debugging patterns used in real firmware development.
 *
 * Target : STM32F4 / adaptable to any ARM Cortex-M MCU
 * Author : Devraj Pravin Patil
 */

#include <stdint.h>
#include <stddef.h>

/* ----------------------------------------------------------------
 * REGISTER DEFINITIONS (STM32F4 USART2 — see bare_metal_stm32.c
 * for full peripheral map)
 * ---------------------------------------------------------------- */
#define USART2_BASE     0x40004400UL

typedef struct {
    volatile uint32_t SR;    /* status register   */
    volatile uint32_t DR;    /* data register     */
    volatile uint32_t BRR;   /* baud rate         */
    volatile uint32_t CR1;   /* control register 1*/
    volatile uint32_t CR2;   /* control register 2*/
    volatile uint32_t CR3;   /* control register 3*/
} USART_TypeDef;

#define USART2  ((USART_TypeDef *) USART2_BASE)

/* SR register bit positions */
#define USART_SR_TXE    7   /* transmit data register empty */
#define USART_SR_TC     6   /* transmission complete        */
#define USART_SR_RXNE   5   /* read data register not empty */
#define USART_SR_ORE    3   /* overrun error                */
#define USART_SR_FE     1   /* framing error                */

#define BIT_READ(reg, n)  (((reg) >> (n)) & 1UL)
#define BIT_SET(reg, n)   ((reg) |= (1UL << (n)))


/* ----------------------------------------------------------------
 * POLLING TX — simplest approach, blocks until byte is sent.
 * Fine for debug logging where timing is not critical.
 * Not suitable for use inside an ISR.
 * ---------------------------------------------------------------- */
void uart_send_byte(uint8_t byte)
{
    /* Wait until TXE = 1 (data register empty, ready for next byte) */
    while (!BIT_READ(USART2->SR, USART_SR_TXE));
    USART2->DR = (uint32_t)byte;
}

/* Wait for transmission to fully complete (last byte shifted out) */
void uart_flush(void)
{
    while (!BIT_READ(USART2->SR, USART_SR_TC));
}

/* Send a null-terminated string */
void uart_print(const char *str)
{
    while (*str) {
        uart_send_byte((uint8_t)*str);
        str++;
    }
}

/* Send a string followed by CRLF */
void uart_println(const char *str)
{
    uart_print(str);
    uart_send_byte('\r');
    uart_send_byte('\n');
}


/* ----------------------------------------------------------------
 * POLLING RX — blocks until a byte arrives.
 * Simple but ties up the CPU — use ring buffer + ISR in production.
 * ---------------------------------------------------------------- */
uint8_t uart_recv_byte(void)
{
    /* Wait until RXNE = 1 (data register has received byte) */
    while (!BIT_READ(USART2->SR, USART_SR_RXNE));
    return (uint8_t)(USART2->DR & 0xFFUL);
}

/* Receive a line (up to max_len-1 chars) terminated by '\n' */
void uart_recv_line(char *buf, uint8_t max_len)
{
    uint8_t i = 0;
    while (i < (max_len - 1U)) {
        char c = (char)uart_recv_byte();
        if (c == '\n') break;
        if (c != '\r') {        /* ignore carriage return */
            buf[i++] = c;
        }
    }
    buf[i] = '\0';              /* null terminate */
}


/* ----------------------------------------------------------------
 * RING BUFFER — for ISR-driven RX
 *
 * ISR writes incoming bytes into the ring buffer.
 * Main loop reads from it without blocking.
 * This is the correct pattern for production firmware —
 * never do heavy processing inside the ISR itself.
 * ---------------------------------------------------------------- */
#define RING_BUF_SIZE   64U     /* must be power of 2 for fast masking */

typedef struct {
    uint8_t  buf[RING_BUF_SIZE];
    volatile uint16_t head;     /* ISR writes here  */
    volatile uint16_t tail;     /* main reads here  */
} RingBuffer;

static RingBuffer rx_ring = {0};

/* Called from USART2 ISR — write one received byte into the buffer */
void ring_buf_write(uint8_t byte)
{
    uint16_t next_head = (rx_ring.head + 1U) & (RING_BUF_SIZE - 1U);
    if (next_head != rx_ring.tail) {    /* not full */
        rx_ring.buf[rx_ring.head] = byte;
        rx_ring.head = next_head;
    }
    /* If full: byte is dropped — add overflow counter in real firmware */
}

/* Called from main loop — read one byte, returns 0 if buffer empty */
uint8_t ring_buf_read(uint8_t *byte)
{
    if (rx_ring.tail == rx_ring.head) return 0;   /* empty */
    *byte = rx_ring.buf[rx_ring.tail];
    rx_ring.tail = (rx_ring.tail + 1U) & (RING_BUF_SIZE - 1U);
    return 1;
}

uint8_t ring_buf_available(void)
{
    return rx_ring.head != rx_ring.tail;
}


/* ----------------------------------------------------------------
 * NUMBER FORMATTING — print integers over UART without printf
 * printf pulls in ~8KB of C stdlib — too heavy for small MCUs.
 * These functions print numbers with zero dependency.
 * ---------------------------------------------------------------- */

/* Print unsigned 32-bit integer in decimal */
void uart_print_u32(uint32_t val)
{
    char buf[11];               /* max 10 digits + null */
    uint8_t i = 10;
    buf[10] = '\0';

    if (val == 0U) {
        uart_send_byte('0');
        return;
    }
    while (val > 0U && i > 0U) {
        buf[--i] = (char)('0' + (val % 10U));
        val /= 10U;
    }
    uart_print(&buf[i]);
}

/* Print unsigned 32-bit integer in hex (e.g. "0x1A2B3C4D") */
void uart_print_hex(uint32_t val)
{
    const char hex_chars[] = "0123456789ABCDEF";
    uart_print("0x");
    for (int8_t shift = 28; shift >= 0; shift -= 4) {
        uart_send_byte((uint8_t)hex_chars[(val >> shift) & 0xFU]);
    }
}

/* Print a register value with a label — useful during debugging */
void uart_print_reg(const char *label, uint32_t val)
{
    uart_print(label);
    uart_print(" = ");
    uart_print_hex(val);
    uart_print("  (");
    uart_print_u32(val);
    uart_println(")");
}


/* ----------------------------------------------------------------
 * USART ISR — example of interrupt-driven RX
 * In real firmware this would be the actual IRQ handler.
 * Registered via the vector table in startup_stm32f4xx.s
 * ---------------------------------------------------------------- */
void USART2_IRQHandler(void)
{
    if (BIT_READ(USART2->SR, USART_SR_RXNE)) {
        /* Data received — read DR to clear RXNE flag */
        uint8_t byte = (uint8_t)(USART2->DR & 0xFFUL);
        ring_buf_write(byte);
    }

    if (BIT_READ(USART2->SR, USART_SR_ORE)) {
        /* Overrun error — read DR to clear it, log if needed */
        (void)USART2->DR;
    }
}


/* ----------------------------------------------------------------
 * MAIN — demonstrates TX, RX, and debug printing patterns
 * ---------------------------------------------------------------- */
int main(void)
{
    /* (clock and GPIO init omitted — see bare_metal_stm32.c) */

    uart_println("UART basics — bare metal STM32");
    uart_println("Polling TX, ring buffer RX, no printf");

    /* Print a register value — typical debugging pattern */
    uart_print_reg("USART2->CR1", USART2->CR1);
    uart_print_reg("USART2->BRR", USART2->BRR);

    /* Echo received bytes back */
    uart_println("Echo mode — send any character:");

    while (1) {
        uint8_t byte;
        if (ring_buf_read(&byte)) {
            uart_print("Received: ");
            uart_send_byte(byte);
            uart_print("  (0x");
            uart_print_hex(byte);
            uart_println(")");
        }
    }

    return 0;
}
