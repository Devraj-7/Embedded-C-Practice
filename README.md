# Embedded-C-Practice
 
Bare-metal Embedded C examples covering the fundamentals of microcontroller firmware development — register-level peripheral configuration, bit manipulation, and communication protocols. Written without HAL or framework abstractions to demonstrate understanding of what happens underneath.
 
Target hardware is STM32F4 (Nucleo-64), but the patterns apply to any ARM Cortex-M MCU.
 
---
 
## Files
 
### `bit_manipulation.c`
Core bit operation macros and functions used throughout embedded firmware — set, clear, toggle, read individual bits and multi-bit register fields. Includes practical examples simulating GPIO ODR register operations and STM32 MODER field writes, plus utility functions (power-of-2 check, Brian Kernighan's set-bit count, XOR swap) that come up in embedded interviews and real firmware work.
 
### `bare_metal_stm32.c`
Direct register-level peripheral configuration on STM32F411 with no HAL, no CubeMX. Covers:
- Enabling peripheral clocks via RCC_AHB1ENR and RCC_APB1ENR
- GPIO mode, output type, speed, and pull configuration via MODER/OTYPER/OSPEEDR/PUPDR
- Atomic GPIO set/reset using BSRR
- USART2 init at 115200 baud with alternate function pin config
- GPIO input with pull-up for button reading
This is what `HAL_GPIO_Init()` and `HAL_UART_Init()` generate under the hood.
 
### `uart_basics.c`
UART communication patterns in bare-metal C:
- Polling TX with `TXE` flag — blocking single-byte and string transmit
- Polling RX with `RXNE` flag — blocking receive and line buffering
- ISR-driven RX with a power-of-2 ring buffer — correct pattern for production firmware where the main loop must not block
- Integer-to-string formatting for UART debug output without `printf` (avoids ~8KB of C stdlib overhead on small MCUs)
- USART2 IRQ handler with overrun error handling
---
 
## Why no HAL?
 
HAL (Hardware Abstraction Layer) is useful for production code and portability. But understanding the registers underneath is what makes debugging possible when HAL behaves unexpectedly — which it does, especially around DMA, timing, and interrupt configuration. These examples exist to build that register-level understanding.
 
---
 
## Tools
 
`STM32F411RE` `ARM Cortex-M4` `Embedded C` `GCC ARM` `STM32CubeIDE` `Keil` `ST-Link` `UART` `GPIO` `Bare-metal`
