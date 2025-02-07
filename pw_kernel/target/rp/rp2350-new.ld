/*
 * Copyright 2025 The Pigweed Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not
 * use this file except in compliance with the License. You may obtain a copy of
 * the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations under
 * the License.
 */

/* This relatively simplified linker script will work with many ARMv7-M and
 * ARMv8-M cores that have on-board memory-mapped RAM and FLASH. For more
 * complex projects and devices, it's possible this linker script will not be
 * sufficient as-is.
 *
 * This linker script is likely not suitable for a project with a bootloader.
 */

/* Note: This technically doesn't set the firmware's entry point. Setting the
 *       firmware entry point is done by setting vector_table[1]
 *       (Reset_Handler). However, this DOES tell the compiler how to optimize
 *       when --gc-sections is enabled.
 */
ENTRY(Reset)

MEMORY
{
  /* Vector Table (typically in flash) */
  VECTOR_TABLE(rx) : ORIGIN = 0x10000000, LENGTH = 2048
  /* Internal Flash */
  FLASH(rx) : ORIGIN = 0x10000800, LENGTH = 2046K
  /* Internal SRAM */
  RAM(rwx) : ORIGIN = 0x20000000, LENGTH = 64K
}

# rp235x-pac crate handlers
PROVIDE(TIMER0_IRQ_0 = DefaultHandler);
PROVIDE(TIMER0_IRQ_1 = DefaultHandler);
PROVIDE(TIMER0_IRQ_2 = DefaultHandler);
PROVIDE(TIMER0_IRQ_3 = DefaultHandler);
PROVIDE(TIMER1_IRQ_0 = DefaultHandler);
PROVIDE(TIMER1_IRQ_1 = DefaultHandler);
PROVIDE(TIMER1_IRQ_2 = DefaultHandler);
PROVIDE(TIMER1_IRQ_3 = DefaultHandler);
PROVIDE(PWM_IRQ_WRAP_0 = DefaultHandler);
PROVIDE(PWM_IRQ_WRAP_1 = DefaultHandler);
PROVIDE(DMA_IRQ_0 = DefaultHandler);
PROVIDE(DMA_IRQ_1 = DefaultHandler);
PROVIDE(DMA_IRQ_2 = DefaultHandler);
PROVIDE(DMA_IRQ_3 = DefaultHandler);
PROVIDE(USBCTRL_IRQ = DefaultHandler);
PROVIDE(PIO0_IRQ_0 = DefaultHandler);
PROVIDE(PIO0_IRQ_1 = DefaultHandler);
PROVIDE(PIO1_IRQ_0 = DefaultHandler);
PROVIDE(PIO1_IRQ_1 = DefaultHandler);
PROVIDE(PIO2_IRQ_0 = DefaultHandler);
PROVIDE(PIO2_IRQ_1 = DefaultHandler);
PROVIDE(IO_IRQ_BANK0 = DefaultHandler);
PROVIDE(IO_IRQ_BANK0_NS = DefaultHandler);
PROVIDE(IO_IRQ_QSPI = DefaultHandler);
PROVIDE(IO_IRQ_QSPI_NS = DefaultHandler);
PROVIDE(SIO_IRQ_FIFO = DefaultHandler);
PROVIDE(SIO_IRQ_BELL = DefaultHandler);
PROVIDE(SIO_IRQ_FIFO_NS = DefaultHandler);
PROVIDE(SIO_IRQ_BELL_NS = DefaultHandler);
PROVIDE(SIO_IRQ_MTIMECMP = DefaultHandler);
PROVIDE(CLOCKS_IRQ = DefaultHandler);
PROVIDE(SPI0_IRQ = DefaultHandler);
PROVIDE(SPI1_IRQ = DefaultHandler);
PROVIDE(UART0_IRQ = DefaultHandler);
PROVIDE(UART1_IRQ = DefaultHandler);
PROVIDE(ADC_IRQ_FIFO = DefaultHandler);
PROVIDE(I2C0_IRQ = DefaultHandler);
PROVIDE(I2C1_IRQ = DefaultHandler);
PROVIDE(OTP_IRQ = DefaultHandler);
PROVIDE(TRNG_IRQ = DefaultHandler);
PROVIDE(PLL_SYS_IRQ = DefaultHandler);
PROVIDE(PLL_USB_IRQ = DefaultHandler);
PROVIDE(POWMAN_IRQ_POW = DefaultHandler);
PROVIDE(POWMAN_IRQ_TIMER  = DefaultHandler);
