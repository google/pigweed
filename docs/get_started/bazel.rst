.. _docs-get-started-bazel:

==================================
Get Started With Pigweed And Bazel
==================================
This guide provides a starting point for using Pigweed in a Bazel-based project.
Bazel is :ref:`the recommended build system <seed-0111>` for new projects using
Pigweed.

-----------
Limitations
-----------
.. TODO: b/306393519 - Update the MacOS description once that path is verified.

* **Linux**. Your development host must be running Linux. Bazel-based Pigweed
  projects are not yet supported on Windows. MacOS probably works but is
  unverified.

-----
Setup
-----
#. `Install Bazel <https://bazel.build/install>`_.

   .. tip::

      If you want to minimize system-wide installations, first install
      `Node Version Manager <https://github.com/nvm-sh/nvm>`_ and then
      install Bazelisk through NPM using ``npm install -g @bazel/bazelisk``.
      If you use this workflow, remember that Bazel will only be available
      in the version of Node that's currently activated through NVM.

#. Clone `the project <https://pigweed.googlesource.com/pigweed/quickstart/bazel/+/refs/heads/main>`_:

   .. code-block:: console

      $ git clone --recursive https://pigweed.googlesource.com/pigweed/quickstart/bazel

   .. tip::

      If you forgot the ``--recursive`` flag when cloning the code, run
      ``git submodule update --init``.

All subsequent commands that you see in this guide should be run from the
root directory of your new ``quickstart`` repo.

-----------------
Build the project
-----------------
#. Build the project for :ref:`target-host` and
   :ref:`target-stm32f429i-disc1`:

   .. code-block:: console

      $ bazel build //...

   You should see output like this:

   .. code-block:: none

      Starting local Bazel server and connecting to it...
      INFO: Analyzed 7 targets (105 packages loaded, 14022 targets configured).
      INFO: Found 7 targets...
      INFO: Elapsed time: 30.705s, Critical Path: 5.55s
      INFO: 86 processes: 24 internal, 62 linux-sandbox.
      INFO: Build completed successfully, 86 total actions

Troubleshooting: ``Network is unreachable (connect failed)``
============================================================
.. _bazelbuild/bazel#2486: https://github.com/bazelbuild/bazel/issues/2486#issuecomment-1870698756

If your build fails and you see a ``Network is unreachable (connect failed)``
error, check if you're on an IPv6 network. If you are, try switching to an IPv4
network. See `bazelbuild/bazel#2486`_.

.. code-block:: console

   bazel build //...
   Starting local Bazel server and connecting to it...
   INFO: Repository platforms instantiated at:
     /home/kayce/sandbox/echo/WORKSPACE:21:13: in <toplevel>
   Repository rule http_archive defined at:
     /home/kayce/.cache/bazel/_bazel_kayce/5b77aa1b33d7b7c439479c603973101b/external/bazel_tools/tools/build_defs/repo/http.bzl:372:31: in <toplevel>
   WARNING: Download from https://mirror.bazel.build/github.com/bazelbuild/platforms/releases/download/0.0.8/platforms-0.0.8.tar.gz failed: class java.net.ConnectException Network is unreachable (connect failed)
   WARNING: Download from https://github.com/bazelbuild/platforms/releases/download/0.0.8/platforms-0.0.8.tar.gz failed: class java.net.ConnectException Network is unreachable (connect failed)
   ...

-----------------------
Run the project locally
-----------------------
#. Run the project locally on your Linux development host:

   .. code-block:: console

      bazel run //src:echo

   You should see output like this:

   .. code-block:: none

      INFO: Analyzed target //src:echo (36 packages loaded, 202 targets configured).
      INFO: Found 1 target...
      Target //src:echo up-to-date:
        bazel-bin/src/echo
      INFO: Elapsed time: 0.899s, Critical Path: 0.03s
      INFO: 1 process: 1 internal.
      INFO: Build completed successfully, 1 total action
      INFO: Running command line: bazel-bin/src/echo

#. Press ``Ctrl`` + ``C`` to stop running the project.

----------------------------------------
Flash the project onto a Discovery board
----------------------------------------
If you have an `STM32F429 Discovery <https://www.st.com/stm32f4-discover>`_
board, you can run the project on that hardware.

.. note::

   You don't need this hardware to run the project. Because this project
   supports the :ref:`target-host` target, you can run everything
   on your Linux development host.

#. Ensure your udev rules are set up to allow the user running the commands
   below to access the Discovery Board.  For example, you may want to add the
   following rule as ``/etc/udev/rules.d/99-stm32f329i-disc1.rules``:

   .. code-block:: console

      ATTRS{idVendor}=="0483", ATTRS{idProduct}=="374b", MODE="664", GROUP="plugdev"

   The user running the commands needs to be in the group ``plugdev``.

#. Connect the Discovery board to your development host with a USB
   cable. **Use the Mini-B USB port on the Discovery board, not the
   Micro-B port**.

#. Flash the project to the Discovery board:

   .. code-block:: console

      $ bazel run //tools:flash

   You should see output like this:

   .. code-block:: none

      INFO: Analyzed target //tools:flash (52 packages loaded, 2760 targets configured).
      INFO: Found 1 target...
      Target //tools:flash up-to-date:
        bazel-bin/tools/flash
      INFO: Elapsed time: 0.559s, Critical Path: 0.04s
      INFO: 1 process: 1 internal.
      INFO: Build completed successfully, 1 total action
      INFO: Running command line: bazel-bin/tools/flash
      binary Rlocation is: /home/xyz/.cache/bazel/_bazel_xyz/8c700b5cf88b83b789ceaf0e4e271fac/execroot/__main__/bazel-out/k8-fastbuild/bin/src/echo.elf
      openocd Rlocation is: /home/xyz/.cache/bazel/_bazel_xyz/8c700b5cf88b83b789ceaf0e4e271fac/external/openocd/bin/openocd
      openocd config Rlocation is: /home/xyz/.cache/bazel/_bazel_xyz/8c700b5cf88b83b789ceaf0e4e271fac/external/pigweed/targets/stm32f429i_disc1/py/stm32f429i_disc1_utils/openocd_stm32f4xx.cfg
      xPack OpenOCD x86_64 Open On-Chip Debugger 0.11.0+dev (2021-12-07-17:30)
      Licensed under GNU GPL v2
      For bug reports, read
      	http://openocd.org/doc/doxygen/bugs.html
      DEPRECATED! use 'adapter driver' not 'interface'
      DEPRECATED! use 'adapter serial' not 'hla_serial'
      Info : The selected transport took over low-level target control. The results might differ compared to plain JTAG/SWD
      srst_only separate srst_nogate srst_open_drain connect_deassert_srst

      Info : clock speed 2000 kHz
      Info : STLINK V2J25M14 (API v2) VID:PID 0483:374B
      Info : Target voltage: 2.837377
      Info : stm32f4x.cpu: Cortex-M4 r0p1 processor detected
      Info : stm32f4x.cpu: target has 6 breakpoints, 4 watchpoints
      Info : gdb port disabled
      Info : Unable to match requested speed 2000 kHz, using 1800 kHz
      Info : Unable to match requested speed 2000 kHz, using 1800 kHz
      target halted due to debug-request, current mode: Thread
      xPSR: 0x01000000 pc: 0x08000708 msp: 0x20030000
      Info : Unable to match requested speed 8000 kHz, using 4000 kHz
      Info : Unable to match requested speed 8000 kHz, using 4000 kHz
      ** Programming Started **
      Info : device id = 0x20016419
      Info : flash size = 2048 kbytes
      Info : Dual Bank 2048 kiB STM32F42x/43x/469/479 found
      Info : Padding image section 0 at 0x08000010 with 496 bytes
      ** Programming Finished **
      ** Resetting Target **
      Info : Unable to match requested speed 2000 kHz, using 1800 kHz
      Info : Unable to match requested speed 2000 kHz, using 1800 kHz
      shutdown command invoked


Communicate with the project over serial
========================================
After you've flashed the project onto your Discovery board, your Linux development
host can communicate with the project over a serial terminal like ``miniterm``.

#. Transmit and receive characters:

   .. code-block:: console

      $ bazel run //tools:miniterm -- /dev/ttyACM0 --filter=debug

   After typing ``hello`` and pressing ``Ctrl`` + ``]`` to exit you should see output
   like this:

   .. code-block:: none

      INFO: Analyzed target //tools:miniterm (41 packages loaded, 2612 targets configured).
      INFO: Found 1 target...
      Target //tools:miniterm up-to-date:
        bazel-bin/tools/miniterm
      INFO: Elapsed time: 0.373s, Critical Path: 0.02s
      INFO: 1 process: 1 internal.
      INFO: Build completed successfully, 1 total action
      INFO: Running command line: bazel-bin/tools/miniterm /dev/ttyACM0 '--filter=debug'
      --- Miniterm on /dev/ttyACM0  115200,8,N,1 ---
      --- Quit: Ctrl+] | Menu: Ctrl+T | Help: Ctrl+T followed by Ctrl+H ---
       [TX:'h']  [RX:'h'] h [TX:'e']  [RX:'e'] e [TX:'l']  [RX:'l'] l [TX:'l']  [RX:'l'] l [TX:'o']  [RX:'o'] o
      --- exit ---

------------------------------
Questions? Comments? Feedback?
------------------------------
Please join `our Discord <https://discord.com/invite/M9NSeTA>`_ and talk to us
in the ``#bazel-build`` channel or `file a bug <https://issues.pigweed.dev>`_.
