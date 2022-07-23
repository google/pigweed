.. _target-stm32f429i-disc1-stm32cube:

---------------------------
stm32f429i-disc1: STM32Cube
---------------------------
.. warning::
  This target is in a very preliminary state and is under active development.
  This demo gives a preview of the direction we are heading with
  :ref:`pw_system<module-pw_system>`, but it is not yet ready for production
  use.


The STMicroelectronics STM32F429I-DISC1 development board is currently Pigweed's
primary target for on-device testing and development. This target configuration
uses :ref:`pw_system<module-pw_system>` on top of FreeRTOS and the STM32Cube HAL
rather than a from-the-ground-up baremetal approach.

-----
Setup
-----
To use this target, Pigweed must be set up to use FreeRTOS and the STM32Cube HAL
for the STM32F4 series. The supported repositories can be downloaded via
``pw package``, and then the build must be manually configured to point to the
locations the repositories were downloaded to.

.. code:: sh

  pw package install freertos
  pw package install stm32cube_f4
  pw package install nanopb

  gn args out
    # Add these lines, replacing ${PW_ROOT} with the path to the location that
    # Pigweed is checked out at.
    dir_pw_third_party_freertos = "${PW_ROOT}/.environment/packages/freertos"
    dir_pw_third_party_stm32cube_f4 = "${PW_ROOT}/.environment/packages/stm32cube_f4"
    dir_pw_third_party_nanopb = "${PW_ROOT}/.environment/packages/nanopb"

Building and running the demo
=============================
This target has an associated demo application that can be built and then
flashed to a device with the following commands:

.. code:: sh

  ninja -C out pw_system_demo

  openocd -f targets/stm32f429i_disc1/py/stm32f429i_disc1_utils/openocd_stm32f4xx.cfg -c "program out/stm32f429i_disc1_stm32cube.size_optimized/obj/pw_system/bin/system_example.elf reset exit"

Once the board has been flashed, you can connect to it and send RPC commands
via the Pigweed console:

.. code:: sh

  pw-system-console -d /dev/{ttyX} -b 115200 --proto-globs pw_rpc/echo.proto --token-databases out/stm32f429i_disc1_stm32cube.size_optimized/obj/pw_system/bin/system_example.elf

Replace ``{ttyX}`` with the appropriate device on your machine. On Linux this
may look like ``ttyACM0``, and on a Mac it may look like ``cu.usbmodem***``.

When the console opens, try sending an Echo RPC request. You should get back
the same message you sent to the device.

.. code:: sh

  >>> device.rpcs.pw.rpc.EchoService.Echo(msg="Hello, Pigweed!")
  (Status.OK, pw.rpc.EchoMessage(msg='Hello, Pigweed!'))

You are now up and running!
