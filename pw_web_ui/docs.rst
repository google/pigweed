.. _module-pw_web_ui:

---------
pw_web_ui
---------

This module is a set of npm libraries for building web UIs
for pigweed devices.

Also included is a basic React app that demonstrates using the npm libraries.

Running the Demo
=================
The simplest way to see the web demo in action is to first flash the `RPC Example <https://pigweed.googlesource.com/pigweed/pigweed/+/main/pw_hdlc/rpc_example/>`_ to a device (like the STMicroelectronics STM32F429I-DISC1). This example basically echoes back the RPC message it receives.

To do this:

#. Follow the Prerequisites & Bootstrap steps from `Getting Started <https://pigweed.dev/docs/getting_started.html#prerequisites>`_ first.
#. After that, create the out build directory by running:

   .. code:: bash

     $ gn gen out

#. Run the compile with:

   .. code:: bash

     $ ninja -C out

#. Flash ``rpc_example.elf``:

   .. code:: bash

     $ openocd -f targets/stm32f429i_disc1/py/stm32f429i_disc1_utils/openocd_stm32f4xx.cfg -c "program out/stm32f429i_disc1_debug/obj/pw_hdlc/rpc_example/bin/rpc_example.elf verify reset exit"

#. Finally run the web server:

   .. code:: bash

     $ bazel run //pw_web_ui:devserver


You can now open `http://127.0.0.1:8081 <http://127.0.0.1:8081>`_ in your browser to see the web console. Now click "Send Echo RPC" and notice the echo received under "Serial Debug".

Available Typescript Modules
=============================
Following Pigweed modules are available as Typescript modules:

- `pw_hdlc <https://pigweed.dev/pw_hdlc/#typescript>`_
- `pw_rpc <https://pigweed.dev/pw_rpc/ts/>`_
- `pw_transfer <https://pigweed.dev/pw_transfer/#typescript>`_
