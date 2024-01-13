.. _target-host-device-simulator:

=====================
Host Device Simulator
=====================
This Pigweed target simulates the behavior of an embedded device, spawning
threads for facilities like RPC and logging. Executables built by this target
will perpetually run until they crash or are explicitly terminated. All
communications with the process are over the RPC server hosted on a local
socket rather than by directly interacting with the terminal via standard I/O.
Host Device Simulator is built on top of :ref:`module-pw_system`.

-----
Setup
-----
.. _Kudzu: https://pigweed.googlesource.com/pigweed/kudzu/+/refs/heads/main/targets/host/BUILD.gn

.. note::

   The instructions below show you how to try out Host Device Simulator within
   an :ref:`upstream Pigweed environment <docs-get-started-upstream>`. To set
   up a target *similar* to Host Device Simulator in your own project, see
   `Kudzu`_.

To use this target, Pigweed must be set up to use Nanopb and FreeRTOS. The
required source repositories can be downloaded via ``pw package``, and then the
build must be manually configured to point to the location the repository was
downloaded to using gn args.

.. code-block:: console

   pw package install nanopb
   pw package install freertos

   gn gen out --export-compile-commands --args="
     dir_pw_third_party_nanopb=\"$PW_PROJECT_ROOT/environment/packages/nanopb\"
     dir_pw_third_party_freertos=\"$PW_PROJECT_ROOT/environment/packages/freertos\"
   "

.. tip::

   Instead of the ``gn gen out`` with args set on the command line above you can
   run:

   .. code-block:: console

      gn args out

   Then add the following lines to that text file:

   .. code-block::

      dir_pw_third_party_nanopb = getenv("PW_PACKAGE_ROOT") + "/nanopb"
      dir_pw_third_party_freertos = getenv("PW_PACKAGE_ROOT") + "/freertos"

-----------------------------
Building and running the demo
-----------------------------
.. _//sample_project_tools/device_sim.py: https://pigweed.googlesource.com/pigweed/sample_project/+/refs/heads/main/tools/sample_project_tools/device_sim.py

.. tip::

   See `//sample_project_tools/device_sim.py`_ for a more polished example
   of running a simulated device. ``device_sim.py`` runs the simulated device
   as a subprocess and then connects to it via the default socket so you just
   have to pass the binary.

To build the demo application:

.. code-block:: console

   ninja -C out pw_system_demo

To run the demo application:

.. code-block:: console

   ./out/host_device_simulator.speed_optimized/obj/pw_system/bin/system_example

To communicate with the launched process run this in a separate shell:

.. code-block:: console

   pw-system-console -s default --proto-globs pw_rpc/echo.proto

Exit the console via the GUI menu or by pressing :kbd:`Ctrl-D` twice.

To stop the ``system_example`` app on Linux / macOS:

.. code-block:: console

   killall system_example

-----------
Communicate
-----------
In the bottom-most pane labeled ``Python Repl`` you should be able to send RPC
commands to the simulated device process.

To send an RPC message that will be echoed back:

.. code-block:: pycon

   >>> device.rpcs.pw.rpc.EchoService.Echo(msg='Hello, world!')
   (Status.OK, pw.rpc.EchoMessage(msg='Hello, world!'))

To run unit tests included on the simulated device:

.. code-block:: pycon

   >>> device.run_tests()
   True

You are now up and running!

.. seealso::

   The :ref:`module-pw_console`
   :bdg-ref-primary-line:`module-pw_console-user_guide` for more info on using
   the the pw_console UI.
