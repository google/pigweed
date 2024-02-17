.. _module-pw_hdlc-rpc-example:

=====================
RPC over HDLC example
=====================
.. pigweed-module-subpage::
   :name: pw_hdlc
   :tagline: pw_hdlc: Simple, robust, and efficient serial communication

The :ref:`module-pw_hdlc` module includes an example of bringing up a
:ref:`module-pw_rpc` server that can be used to invoke RPCs. The example code
is located at `//pw_hdlc/rpc_example/
<https://cs.opensource.google/pigweed/pigweed/+/main:pw_hdlc/rpc_example/>`_.
This tutorial walks through invoking RPCs interactively and with a script using
the RPC over HDLC example.

The example implementation of the ``system_server`` facade from ``pw_rpc``
sends HDLC-encoded RPC packets via ``pw_sys_io``. It has blocking sends and
reads so it is not suitable for performance-sensitive applications. This
mostly serves as a simplistic example for quickly bringing up RPC over HDLC
on bare-metal targets.

.. note::

   This tutorial assumes that you've got a :ref:`target-stm32f429i-disc1`
   board, but the instructions work with any target that has implemented
   :ref:`module-pw_sys_io`.

-----------
Get started
-----------

1. Set up your board
====================
Connect the board you'll be communicating with. For the
:ref:`target-stm32f429i-disc1` board, connect the mini USB port, and note which
serial device it appears as (e.g. ``/dev/ttyACM0``).

2. Build Pigweed
================
Activate the Pigweed environment and run the default build.

.. code-block:: console

   . ./activate.sh
   pw package install nanopb
   gn gen out --args='dir_pw_third_party_nanopb="//environment/packages/nanopb"'
   ninja -C out

3. Flash the firmware image
===========================
After a successful build, the binary for the example will be located at
``//out/<toolchain>/obj/pw_hdlc/rpc_example/bin/rpc_example.elf``.

Flash this image to your board. If you are using the
:ref:`target-stm32f429i-disc1` board you can flash the image with
`OpenOCD <http://openocd.org>`_.

.. code-block:: console

   openocd -f \
     targets/stm32f429i_disc1/py/stm32f429i_disc1_utils/openocd_stm32f4xx.cfg \
     -c "program \
     out/stm32f429i_disc1_debug/obj/pw_hdlc/rpc_example/bin/rpc_example.elf \
     verify reset exit"

4. Invoke RPCs from an interactive console
==========================================
The RPC console uses :ref:`module-pw_console` to make a rich interactive
console for working with ``pw_rpc``. Run the RPC console with the following
command, replacing ``/dev/ttyACM0`` with the correct serial device for your
board.

.. code-block:: console

   pw-system-console --no-rpc-logging --proto-globs pw_rpc/echo.proto \
     --device /dev/ttyACM0

RPCs may be accessed through the predefined ``rpcs`` variable. RPCs are
organized by their protocol buffer package and RPC service, as defined in a
.proto file. The ``Echo`` method is part of the ``EchoService``, which
is in the ``pw.rpc`` package. To invoke it synchronously, call
``rpcs.pw.rpc.EchoService.Echo()``:

.. code-block:: pycon

   >>> device.rpcs.pw.rpc.EchoService.Echo(msg='Hello, world!')
   (Status.OK, pw.rpc.EchoMessage(msg='Hello, world!'))

5. Invoke RPCs with a script
============================
RPCs may also be invoked from Python scripts. Close the RPC console if it is
running, and execute the example script. Set the ``--device`` argument to the
serial port for your device.

.. code-block:: console

   python pw_hdlc/rpc_example/example_script.py --device /dev/ttyACM0

You should see this output:

.. code-block:: text

   The status was Status.OK
   The payload was msg: "Hello"

   The device says: Goodbye!

-------------------------
Local RPC example project
-------------------------
This example is similar to the above example, except it uses a socket to
connect a server to a client, both running on the host.

1. Build Pigweed
================
Activate the Pigweed environment and build the code.

.. code-block:: console

   . ./activate.sh
   pw package install nanopb
   gn gen out --args='dir_pw_third_party_nanopb="//environment/packages/nanopb"'
   ninja -C out

2. Start the server
===================
Run a ``pw_rpc`` server in one terminal window.

.. code-block:: console

   ./out/pw_strict_host_clang_debug/obj/pw_hdlc/rpc_example/bin/rpc_example

3. Start the client
===================
In a separate Pigweed-activated terminal, run the ``pw-system-console`` RPC
client with ``--proto-globs`` set to ``pw_rpc/echo.proto``. Additional protos
can be added if needed.

.. code-block:: console

   pw-system-console --no-rpc-logging --proto-globs pw_rpc/echo.proto \
     --socket-addr default

.. tip::

   The ``--socket-addr`` value may be replaced with an IP and port separated by
   a colon, e.g. ``127.0.0.1:33000``. If using a Unix socket, the path to the
   file follows ``file:``, e.g. ``file:/path/to/unix/socket``. Unix socket
   Python support is pending, see `python/cpython#77589
   <https://github.com/python/cpython/issues/77589>`_.

.. tip::

   The default RPC channel ID (``1``) can be overriden with ``--channel-id``.

4. Invoke RPCs from the client
==============================
Invoke RPCs from the interactive console on the client side.

.. code-block:: pycon

   >>> device.rpcs.pw.rpc.EchoService.Echo(msg='Hello, world!')
   (Status.OK, pw.rpc.EchoMessage(msg='Hello, world!'))

.. seealso::

   - The :ref:`module-pw_console`
     :bdg-ref-primary-line:`module-pw_console-user_guide` for more info on using
     the the pw_console UI.

   - The target docs for other RPC-enabled application examples:

     - :bdg-ref-primary-line:`target-host-device-simulator`
     - :bdg-ref-primary-line:`target-raspberry-pi-pico`
     - :bdg-ref-primary-line:`target-stm32f429i-disc1-stm32cube`

-----------------
More pw_hdlc docs
-----------------
.. include:: ../docs.rst
   :start-after: .. pw_hdlc-nav-start
   :end-before: .. pw_hdlc-nav-end
