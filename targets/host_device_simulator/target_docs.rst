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
.. _examples repo: https://pigweed.googlesource.com/pigweed/examples/+/refs/heads/main/

.. note::

   The instructions below show you how to try out Host Device Simulator within
   an :ref:`upstream Pigweed environment <docs-contributing>`. To set
   up a target *similar* to Host Device Simulator in your own project, see
   `Kudzu`_ or the `examples repo`_.

.. tab-set::

   .. tab-item:: Bazel
      :sync: bazel

      No extra setup steps are required for Bazel.

   .. tab-item:: GN
      :sync: gn

      To use this target, Pigweed must be set up to use Nanopb and FreeRTOS. The
      required source repositories can be downloaded via ``pw package``, and
      then the build must be manually configured to point to the location the
      repository was downloaded to using gn args.

      .. code-block:: console

         pw package install nanopb
         pw package install freertos

         gn gen out --export-compile-commands --args="
           dir_pw_third_party_nanopb=\"//environment/packages/nanopb\"
           dir_pw_third_party_freertos=\"//environment/packages/freertos\"
         "

      .. tip::

         Instead of the ``gn gen out`` with args set on the command line above
         you can run:

         .. code-block:: console

            gn args out

         Then add the following lines to that text file:

         .. code-block::

            dir_pw_third_party_nanopb = "//environment/packages/nanopb"
            dir_pw_third_party_freertos = "//environment/packages/freertos"


.. _target-host-device-simulator-demo:

-----------------------------
Building and running the demo
-----------------------------
.. _examples repo device_sim.py: https://pigweed.googlesource.com/pigweed/examples/+/refs/heads/main/tools/sample_project_tools/device_sim.py

.. seealso::

   See the `examples repo device_sim.py`_ for a downstream project example of
   launching a device simulator with project-specific RPC protos. That script
   uses upstream Pigweed's :cs:`device_sim.py
   <main:pw_system/py/pw_system/device_sim.py>` which runs the simulated device
   as a subprocess and then connects to it via the default socket so you just
   have to pass the binary.

.. tab-set::

   .. tab-item:: Bazel
      :sync: bazel

      Build and run the demo application console with:

      .. code-block:: console

         bazel run //pw_system:simulator_system_example_console

   .. tab-item:: GN
      :sync: gn

      Build the demo application:

      .. code-block:: console

         ninja -C out pw_system_demo

      Launch the demo application and connect to it with the pw_system console:

      .. code-block:: console

         pw-system-device-sim \
           --sim-binary \
           ./out/host_device_simulator.speed_optimized/obj/pw_system/bin/system_example

Exit the console via the GUI menu, running ``exit`` or ``quit`` in the Python
repl or by pressing :kbd:`Ctrl-D` twice.

-----------
Communicate
-----------
In the bottom-most pane labeled ``Python Repl`` you should be able to send RPC
commands to the simulated device process.

To send an RPC message that will be echoed back:

.. code-block:: pycon

   >>> device.rpcs.pw.rpc.EchoService.Echo(msg='Hello, world!')
   (Status.OK, pw.rpc.EchoMessage(msg='Hello, world!'))

You are now up and running!

.. seealso::

   The :ref:`module-pw_console`
   :bdg-ref-primary-line:`module-pw_console-user_guide` for more info on using
   the pw_console UI.
