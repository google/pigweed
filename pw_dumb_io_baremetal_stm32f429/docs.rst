.. _chapter-dumb-io-baremetal-stm32f429:

.. default-domain:: cpp

.. highlight:: sh

---------------------------
STM32F429 baremetal dumb IO
---------------------------
The STM32F429 baremetal dumb IO backend provides device startup code and a UART
driver to layer that allows applications built against the ``pw_dumb_io``
interface to run on a STM32F429 chip and do simple input/output via UART.
The code is optimized for the STM32F429I-DISC1, using USART1 (which is connected
to the virtual COM port on the embedded ST-LINKv2 chip). However, this should
work with all STM32F429 variations (and even some STM32F4xx chips).

This backend has no configuration options. The point of it is to provide bare-
minimum platform code needed to do UART reads/writes.

Setup
=====
This module requires relatively minimal setup:

  1. Write code against the ``pw_dumb_io`` facade.
  2. Specify the ``dir_pw_dumb_io_backend`` GN gloabal variable to point to this
     backend.
  3. Build an executable with a main() function using a toolchain that
     supports Cortex-M4.

.. note::
  This module provides early firmware init and a linker script, so it will
  conflict with other modules that do any early device init or provide a linker
  script.

Module usage
============
After building a an executable that utilizes this backend, flash the
produced .elf binary to the development board. Then, using a serial
communication terminal (like minicom or screen), connect to the device at a
baud rate of 115200 (8N1). If you're not using a STM32F429I-DISC1 development
board, manually connect a USB TLL adapter to pins `A9` (TX) and
`A10` (RX).

Dependencies
============
  * pw_dumb_io facade
  * pw_preprocessor module
