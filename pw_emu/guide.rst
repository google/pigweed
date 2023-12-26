.. _module-pw_emu-guide:

====================
Get started & guides
====================
.. pigweed-module-subpage::
   :name: pw_emu
   :tagline: pw_emu: Flexible emulators frontend

.. _module-pw_emu-get-started:

-----------
Get started
-----------
.. tab-set::

   .. tab-item:: Bazel

      ``pw_emu`` is currently only supported for GN-based projects.

   .. tab-item:: GN

      Include the desired emulator target files :ref:`pigweed.json
      <seed-0101>`. For example:

      .. code-block:: json

         "pw_emu": {
            "target_files": [
              "pw_emu/qemu-lm3s6965evb.json",
              "pw_emu/qemu-stm32vldiscovery.json",
              "pw_emu/qemu-netduinoplus2.json",
              "renode-stm32f4_discovery.json"
            ]
          }

      See :ref:`module-pw_emu-config-fragments` for examples of target files.

      Build a target and then use ``pw emu run`` to run the target binaries on
      the host. Continuing with the example:

      .. code-block:: console

         ninja -C out qemu_gcc
         pw emu run --args=-no-reboot qemu-lm3s6965evb \
             out/lm3s6965evb_qemu_gcc_size_optimized/obj/pw_snapshot/test/cpp_compile_test

   .. tab-item:: CMake

      ``pw_emu`` is currently only supported for GN-based projects.

   .. tab-item:: Zephyr

      ``pw_emu`` is currently only supported for GN-based projects.

------------------------
Set up emulation targets
------------------------
An emulator target can be defined directly in the ``pw.pw_emu`` namespace of
:ref:`pigweed.json <seed-0101>`, like this:

.. code-block::

   {
     "pw": {
       "pw_emu": {
         "targets": {
           "qemu-lm3s6965evb": {
             "...": "..."
           }
         }
       }
     }
   }

Or it can be defined elsewhere and then imported into ``pigweed.json``, like
this:

.. code-block::

   {
     "pw": {
       "pw_emu": {
         "target_files": [
           "qemu-lm3s6965evb.json"
         ]
       }
     }
   }

Relative paths are interpreted relative to the root of your project directory.

You can configure default options at the emulator level and then override
those options at the target or channel level. See :ref:`module-pw_emu-config`
for details.

QEMU targets
============
When defining a QEMU emulation target the following keys must be defined
under ``<target>.qemu`` (where ``<target>`` is a placeholder for a real target
name):

* ``executable`` - The name of the QEMU executable, e.g. ``qemu-system-arm``,
  ``qemusystem-riscv64``, etc.
* ``machine`` - The QEMU machine name. See ``qemu-system-<arch> -machine help``
  for a list of supported machines names.

The following example is a :ref:`config fragment <module-pw_emu-config-fragments>`
for a target that runs on QEMU:

.. literalinclude:: qemu-lm3s6965evb.json

.. note::

   Since this is an Arm machine the QEMU executable is defined as
   ``qemu-system-arm``.

QEMU chardevs can be exposed as host channels under
``<target>.qemu.channels.chardevs.<chan-name>`` where ``<chan-name>`` is
the name that the channel can be accessed with (e.g. ``pw emu term
<chan-name>``). The ``id`` option is mandatory; it should match a valid
chardev ``id`` for this particular QEMU machine.

The example target above emulates a `Stellaris EEVB
<https://www.ti.com/product/LM3S6965>`_ and is compatible with the
:ref:`target-lm3s6965evb-qemu` Pigweed target. The configuration defines a
``serial0`` channel to be the QEMU ``chardev`` with an ``id`` of ``serial0``.
The default channel type (``tcp``) is used, which is supported by all platforms.
You can change the type by adding a ``type`` key set to the desired type, e.g.
``pty``.

Renode targets
==============
The following example is a :ref:`config fragment <module-pw_emu-config-fragments>`
for a target that runs on Renode:

.. literalinclude:: renode-stm32f4_discovery.json

This target emulates the `ST 32F429I Discovery Kit
<https://www.st.com/en/evaluation-tools/32f429idiscovery.html>`_ and is
compatible with the :ref:`target-stm32f429i-disc1` Pigweed target.  ``machine``
identifies which Renode script to use for the machine definitions. ``terminals``
defines which UART devices to expose to the host. ``serial0`` exposes the serial
port identified as ``sysbus.usart1`` in the Renode machine script.

-------------------
Run target binaries
-------------------
Use ``pw emu run`` to quickly run target binaries on the host. ``pw emu run``
starts an emulator instance, connects to a given serial port, and then loads
and runs the given binary.

.. code-block:: console

   $ pw emu run --args=-no-reboot qemu-lm3s6965evb \
         out/lm3s6965evb_qemu_gcc_size_optimized/obj/pw_snapshot/test/cpp_compile_test

   --- Miniterm on serial0 ---
   --- Quit: Ctrl+] | Menu: Ctrl+T | Help: Ctrl+T followed by Ctrl+H ---
   INF  [==========] Running all tests.
   INF  [ RUN      ] Status.CompileTest
   INF  [       OK ] Status.CompileTest
   INF  [==========] Done running all tests.
   INF  [  PASSED  ] 1 test(s).
   --- exit ---

.. note::

   The ``-no-reboot`` option is passed directly to QEMU and instructs the
   emulator to quit instead of rebooting.

---------
Debugging
---------
Use ``pw emu gdb`` to debug target binaries.

.. note::

   You always need to run an emulator instance (``pw emu start``) before
   starting a debug session.

In the following example, ``status_test`` from ``pw_status`` is debugged. The
binary was compiled for :ref:`target-lm3s6965evb-qemu`. First an emulator
instance is started using the ``qemu-lm3s6965evb`` emulation target definition
and then the test file is loaded:

.. code-block:: console

   $ pw emu start qemu-lm3s6965evb \
       --file out/lm3s6965evb_qemu_gcc_size_optimized/obj/pw_status/test/status_test

Next, a ``gdb`` session is started and connected to the emulator instance:

.. code-block:: console

   $ pw emu gdb -e out/lm3s6965evb_qemu_gcc_size_optimized/obj/pw_status/test/status_test
   GNU gdb (Arm GNU Toolchain 12.2.MPACBTI-Rel1 ...
   ...
   Reading symbols from out/stm32f429i_disc1_debug/obj/pw_status/test/status_test.elf...
   Remote debugging using ::1:32979
   pw::sys_io::WriteByte (b=(unknown: 0x20)) at pw_sys_io_baremetal_lm3s6965evb/sys_io_baremetal.cc:117
   117	  uart0.data_register = static_cast<uint32_t>(b);
   (gdb) bt
   #0  pw::sys_io::WriteByte (b=(unknown: 0x20)) at pw_sys_io_baremetal_lm3s6965evb/sys_io_baremetal.cc:117
   #1  0x00002f6a in pw::sys_io::WriteBytes (src=...) at pw_span/public/pw_span/internal/span_impl.h:408
   #2  0x00002eca in pw::sys_io::WriteLine (s=...) at pw_span/public/pw_span/internal/span_impl.h:264
   #3  0x00002f92 in operator() (log=..., __closure=0x0 <vector_table>) at pw_log_basic/log_basic.cc:87
   #4  _FUN () at pw_log_basic/log_basic.cc:89
   #5  0x00002fec in pw::log_basic::pw_Log (level=<optimized out>, flags=<optimized out>, module_name=<optimized out>, file_name=<optimized out>, line_number=95,
    function_name=0x6e68 "TestCaseStart", message=0x6e55 "[ RUN      ] %s.%s") at pw_log_basic/log_basic.cc:155
   #6  0x00002b0a in pw::unit_test::LoggingEventHandler::TestCaseStart (this=<optimized out>, test_case=...) at pw_unit_test/logging_event_handler.cc:95
   #7  0x00000f54 in pw::unit_test::internal::Framework::CreateAndRunTest<pw::(anonymous namespace)::Status_Strings_Test> (test_info=...)
    at pw_unit_test/public/pw_unit_test/internal/framework.h:266
   #8  0x0000254a in pw::unit_test::internal::TestInfo::run (this=0x20000280 <_pw_unit_test_Info_Status_Strings>)
    at pw_unit_test/public/pw_unit_test/internal/framework.h:413
   #9  pw::unit_test::internal::Framework::RunAllTests (this=0x20000350 <pw::unit_test::internal::Framework::framework_>) at pw_unit_test/framework.cc:64
   #10 0x000022b0 in main (argc=<optimized out>, argv=<optimized out>) at pw_unit_test/public/pw_unit_test/internal/framework.h:218

At this point you can debug the program with ``gdb`` commands.

To stop the debugging session:

.. code-block:: console

   $ pw emu stop

--------------
Boot debugging
--------------
Use the ``-p`` or ``--pause`` option when starting an emulator to debug
bootstrapping code:

.. code-block:: console

   $ pw emu start -p qemu-lm3s6965evb \
         --file out/lm3s6965evb_qemu_gcc_size_optimized/obj/pw_status/test/status_test

The given program loads but the emulator doesn't start executing. Next, start
a debugging session with ``pw emu gdb``:

.. code-block:: console

   $ pw emu gdb -e out/lm3s6965evb_qemu_gcc_size_optimized/obj/pw_status/test/status_test
   GNU gdb (Arm GNU Toolchain 12.2.MPACBTI-Rel1 ...
   ...
   Reading symbols from out/lm3s6965evb_qemu_gcc_size_optimized//obj/pw_status/test/status_test...
   Remote debugging using ::1:38723
   pw_boot_Entry () at pw_boot_cortex_m/core_init.c:122
   122	  asm volatile("cpsid i");
   (gdb)

The program stops at the :ref:`pw_boot_Entry() <module-pw_boot>` function. From
here you can add breakpoints or step through the program with ``gdb`` commands.

-------------------------------
Run multiple emulator instances
-------------------------------
Use the ``-i`` or ``--instance`` option to run multiple emulator instances.

.. note::

   Internally each emulator instance is identified by a working directory. The
   working directory for ``pw_emu`` is ``$PROJECT_ROOT/.pw_emu/<instance-id>``.

The next example attempts to start two emulators at the same time:

.. code-block:: console

   $ pw emu start qemu-lm3s6965evb --file out/lm3s6965evb_qemu_gcc_size_optimized/obj/pw_status/test/status_test
   $ pw emu start qemu-lm3s6965evb --file out/lm3s6965evb_qemu_gcc_size_optimized/obj/pw_status/test/status_test
   pigweed/.pw_emu/default: emulator already started


This fails because ``pw emu`` attempts to assign the same default instance
ID to each instance. The default ID is ``default``. To fix this, assign the
second instance a custom ID:

.. code-block:: console

   $ pw emu start qemu-lm3s6965evb --file out/lm3s6965evb_qemu_gcc_size_optimized/obj/pw_status/test/status_test
   $ pw emu -i instance2 start qemu-lm3s6965evb --file out/lm3s6965evb_qemu_gcc_size_optimized/obj/pw_status/test/status_test

To stop both emulator instances:

.. code-block:: console

   $ pw emu stop
   $ pw emu stop -i instance2

-------------------------
Adding new emulator types
-------------------------
``pw_emu`` can be extended to support new emulator types by providing
implementations for :py:class:`pw_emu.core.Launcher` and
:py:class:`pw_emu.core.Connector` in a dedicated ``pw_emu`` Python module
(e.g. :py:mod:`pw_emu.myemu`) or in an external Python module.

Internal ``pw_emu`` modules must register the connector and launcher
classes by updating :py:obj:`pw_emu.pigweed_emulators.pigweed_emulators`. For
example, the QEMU implementation sets the following values:

.. code-block:: py

   pigweed_emulators: Dict[str, Dict[str, str]] = {
     ...
     'qemu': {
       'connector': 'pw_emu.qemu.QemuConnector',
       'launcher': 'pw_emu.qemu.QemuLauncher',
      },
     ...
   }

For external emulator frontend modules, ``pw_emu`` uses
:ref:`pigweed.json <seed-0101>` to determine the connector and launcher
classes under ``pw_emu.emulators.<emulator-name>.connector`` and
``pw_emu.emulators:<emulator-name>.launcher``.

Configuration example:

.. code-block::

   {
     "pw": {
       "pw_emu": {
         "emulators": [
           "myemu": {
             "launcher": "mypkg.mymod.mylauncher",
             "connector": "mypkg.mymod.myconnector",
           }
         ]
       }
     }
   }

The :py:class:`pw_emu.core.Launcher` implementation must implement the following
methods:

* :py:meth:`pw_emu.core.Launcher._pre_start`
* :py:class:`pw_emu.core.Launcher._post_start`
* :py:class:`pw_emu.core.Launcher._get_connector`

There are several abstract methods that need to be implemented for the
connector, like :py:meth:`pw_emu.core.Connector.reset` and
:py:meth:`pw_emu.core.Connector.cont`.  These are typically implemented using
internal channels and :py:class:`pw_emu.core.Connector.get_channel_stream`. See
:py:class:`pw_emu.core.Connector` for a complete list of the abstract methods
that need to be implemented.

----------------
More pw_emu docs
----------------
.. include:: docs.rst
   :start-after: .. pw_emu-nav-start
   :end-before: .. pw_emu-nav-end
