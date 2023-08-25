.. _target-apollo4-pw-system:

============================
Ambiq Apollo4 with pw_system
============================

.. warning::

  This target is in a very preliminary state and is under active development.
  This demo gives a preview of the direction we are heading with
  :ref:`pw_system<module-pw_system>`, but it is not yet ready for production
  use.

This target configuration uses :ref:`pw_system<module-pw_system>` on top of
FreeRTOS and the `AmbiqSuite SDK
<https://ambiq.com/apollo4-blue-plus>`_ HAL.

-----
Setup
-----
To use this target, Pigweed must be set up to use FreeRTOS and the AmbiqSuite
SDK HAL for the Apollo4 series. The FreeRTOS repository can be downloaded via
``pw package``, and the `AmbiqSuite SDK`_ can be downloaded from the Ambiq
website.

.. _AmbiqSuite SDK: https://ambiq.com/apollo4-blue-plus

Once the AmbiqSuite SDK package has been downloaded and extracted, the user
needs to set ``dir_pw_third_party_ambiq_SDK`` build arg to the location of
extracted directory:

.. code-block:: sh

   $ gn args out

Then add the following lines to that text file:

.. code-block::

   # Path to the extracted AmbiqSuite SDK package.
   dir_pw_third_party_ambiq_SDK = "/path/to/AmbiqSuite_R4.3.0"

   # Path to the FreeRTOS source directory.
   dir_pw_third_party_freertos = "/path/to/pigweed/third_party/freertos"

-----------------------------
Building and Running the Demo
-----------------------------
This target has an associated demo application that can be built and then
flashed to a device with the following commands:

.. code-block:: sh

   ninja -C out pw_system_demo

.. seealso::

   The :ref:`target-apollo4` for more info on flashing the Apollo4 board.

Once the board has been flashed, you can connect to it and send RPC commands
via the Pigweed console:

.. code-block:: sh

   pw-system-console -d /dev/{ttyX} -b 115200 \
     --proto-globs pw_rpc/echo.proto \
     --token-databases \
       out/apollo4_pw_system.size_optimized/obj/pw_system/bin/system_example.elf

Replace ``{ttyX}`` with the appropriate device on your machine. On Linux this
may look like ``ttyACM0``, and on a Mac it may look like ``cu.usbmodem***``.

When the console opens, try sending an Echo RPC request. You should get back
the same message you sent to the device.

.. code-block:: pycon

   >>> device.rpcs.pw.rpc.EchoService.Echo(msg="Hello, Pigweed!")
   (Status.OK, pw.rpc.EchoMessage(msg='Hello, Pigweed!'))

You can also try out our thread snapshot RPC service, which should return a
stack usage overview of all running threads on the device in Host Logs.

.. code-block:: pycon

   >>> device.rpcs.pw.thread.proto.ThreadSnapshotService.GetPeakStackUsage()

Example output:

.. code-block::

   20220826 09:47:22  INF  PendingRpc(channel=1, method=pw.thread.ThreadSnapshotService.GetPeakStackUsage) completed: Status.OK
   20220826 09:47:22  INF  Thread State
   20220826 09:47:22  INF    5 threads running.
   20220826 09:47:22  INF
   20220826 09:47:22  INF  Thread (UNKNOWN): IDLE
   20220826 09:47:22  INF  Est CPU usage: unknown
   20220826 09:47:22  INF  Stack info
   20220826 09:47:22  INF    Current usage:   0x20002da0 - 0x???????? (size unknown)
   20220826 09:47:22  INF    Est peak usage:  390 bytes, 76.77%
   20220826 09:47:22  INF    Stack limits:    0x20002da0 - 0x20002ba4 (508 bytes)
   20220826 09:47:22  INF
   20220826 09:47:22  INF  ...

You are now up and running!

.. seealso::

   The :ref:`module-pw_console`
   :bdg-ref-primary-line:`module-pw_console-user_guide` for more info on using
   the the pw_console UI.
