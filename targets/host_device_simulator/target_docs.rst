.. _target-host-device-simulator:

=====================
Host Device Simulator
=====================
This Pigweed target simulates the behavior of an embedded device, spawning
threads for facilities like RPC and logging. Executables built by this target
will perpetually run until they crash or are explicitly terminated. All
communications with the process are over the RPC server hosted on a local
socket rather than by directly interacting with the terminal via standard I/O.

-----
Setup
-----
To use this target, Pigweed must be set up to use nanopb. The required source
repository can be downloaded via ``pw package``, and then the build must be
manually configured to point to the location the repository was downloaded to.

.. code:: sh

  pw package install nanopb

  gn args out
    # Add this line, replacing ${PW_ROOT} with the path to the location that
    # Pigweed is checked out at.
    dir_pw_third_party_nanopb = "${PW_ROOT}/.environment/packages/nanopb"

-----------------------------
Building and running the demo
-----------------------------
This target has an associated demo application that can be built and then
run with the following commands:

.. code:: sh

  ninja -C out pw_system_demo

  ./out/host_device_simulator.speed_optimized/obj/pw_system/bin/system_example

To communicate with the launched process, use
``pw-system-console -s localhost:33000 --proto-globs pw_rpc/echo.proto``.

In the bottom-most pane labeled ``Python Repl``, you'll now be able to send RPC
commands to the simulated device process. For example, you can send an RPC
message that will be echoed back:

.. code:: sh

  >>> device.rpcs.pw.rpc.EchoService.Echo(msg='Hello, world!')
  (Status.OK, pw.rpc.EchoMessage(msg='Hello, world!'))

Or run unit tests included on the simulated device:

.. code:: sh

  >>> device.run_tests()
  True
