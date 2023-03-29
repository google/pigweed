.. _module-pw_hdlc-rpc-example:

=============================
RPC over HDLC example project
=============================
The :ref:`module-pw_hdlc` module includes an example of bringing up a
:ref:`module-pw_rpc` server that can be used to invoke RPCs. The example code
is located at ``pw_hdlc/rpc_example``. This section walks through invoking RPCs
interactively and with a script using the RPC over HDLC example.

These instructions assume the STM32F429i Discovery board, but they work with
any target with :ref:`pw::sys_io <module-pw_sys_io>` implemented.

---------------------
Getting started guide
---------------------

1. Set up your board
====================
Connect the board you'll be communicating with. For the Discovery board, connect
the mini USB port, and note which serial device it appears as (e.g.
``/dev/ttyACM0``).

2. Build Pigweed
================
Activate the Pigweed environment and run the default build.

.. code-block:: sh

   . ./activate.sh
   pw package install nanopb
   gn gen out --args='dir_pw_third_party_nanopb="//environment/packages/nanopb"'
   ninja -C out

3. Flash the firmware image
===========================
After a successful build, the binary for the example will be located at
``out/<toolchain>/obj/pw_hdlc/rpc_example/bin/rpc_example.elf``.

Flash this image to your board. If you are using the STM32F429i Discovery Board,
you can flash the image with `OpenOCD <http://openocd.org>`_.

.. code-block:: sh

   openocd -f \
     targets/stm32f429i_disc1/py/stm32f429i_disc1_utils/openocd_stm32f4xx.cfg \
     -c "program \
     out/stm32f429i_disc1_debug/obj/pw_hdlc/rpc_example/bin/rpc_example.elf \
     verify reset exit"

4. Invoke RPCs from in an interactive console
=============================================
The RPC console uses :ref:`module-pw_console` to make a rich interactive
console for working with pw_rpc. Run the RPC console with the following command,
replacing ``/dev/ttyACM0`` with the correct serial device for your board.

.. code-block:: sh

   pw-system-console --no-rpc-logging --proto-globs pw_rpc/echo.proto \
     --device /dev/ttyACM0

RPCs may be accessed through the predefined ``rpcs`` variable. RPCs are
organized by their protocol buffer package and RPC service, as defined in a
.proto file. To call the ``Echo`` method is part of the ``EchoService``, which
is in the ``pw.rpc`` package. To invoke it synchronously, call
``rpcs.pw.rpc.EchoService.Echo``:

.. code:: pycon

   >>> device.rpcs.pw.rpc.EchoService.Echo(msg='Hello, world!')
   (Status.OK, pw.rpc.EchoMessage(msg='Hello, world!'))

5. Invoke RPCs with a script
============================
RPCs may also be invoked from Python scripts. Close the RPC console if it is
running, and execute the example script. Set the --device argument to the
serial port for your device.

.. code-block:: sh

   python pw_hdlc/rpc_example/example_script.py --device /dev/ttyACM0

You should see this output:

.. code-block:: text

   The status was Status.OK
   The payload was msg: "Hello"

   The device says: Goodbye!

-------------------------
Local RPC example project
-------------------------

This example is similar to the above example, except it use socket to
connect server and client running on the host.

1. Build Pigweed
================
Activate the Pigweed environment and build the code.

.. code-block:: sh

   . ./activate.sh
   pw package install nanopb
   gn gen out --args='dir_pw_third_party_nanopb="//environment/packages/nanopb"'
   ninja -C out

2. Start client side and server side
====================================

Run pw_rpc server in one terminal window.

.. code-block:: sh

   ./out/pw_strict_host_clang_debug/obj/pw_hdlc/rpc_example/bin/rpc_example

In a separate activated terminal, run the ``pw-system-console`` RPC client with
``--proto-globs`` set to ``pw_rpc/echo.proto``. Additional protos can be added
if needed.

.. code-block:: sh

   pw-system-console --no-rpc-logging --proto-globs pw_rpc/echo.proto \
     --socket-addr default

.. tip::

   The ``--socket-addr`` may be replaced with IP and port separated by a colon. For
   example: ``127.0.0.1:33000``.

Then you can invoke RPCs from the interactive console on the client side.

.. code:: pycon

   >>> device.rpcs.pw.rpc.EchoService.Echo(msg='Hello, world!')
   (Status.OK, pw.rpc.EchoMessage(msg='Hello, world!'))

.. seealso::

   - The :ref:`module-pw_console`
     :bdg-ref-primary-line:`module-pw_console-user_guide` for more info on using
     the the pw_console UI.

   - The target docs for other RPC enabled application examples:

     - :bdg-ref-primary-line:`target-host-device-simulator`
     - :bdg-ref-primary-line:`target-raspberry-pi-pico-pw-system`
     - :bdg-ref-primary-line:`target-stm32f429i-disc1-stm32cube`
