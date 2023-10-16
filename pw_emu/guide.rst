.. _module-pw_emu-guide:

============
How-to guide
============
.. pigweed-module-subpage::
   :name: pw_emu
   :tagline: Emulators frontend

This guide shows you how to do common ``pw_emu`` tasks.

------------------------
Set up emulation targets
------------------------
An emulator target can be defined in the top level ``pigweed.json`` under
``pw:pw_emu:target`` or defined in a separate json file under ``targets`` and
then included in the top level ``pigweed.json`` via ``pw:pw_emu:target_files``.

For example, for the following layout:

.. code-block::

   ├── pigweed.json
   └── pw_emu
       └── qemu-lm3s6965evb.json

the following can be added to the ``pigweed.json`` file:

.. code-block::

   {
     "pw": {
       "pw_emu": {
         "target_files": [
           "pw_emu/qemu-lm3s6965evb.json"
         ]
       }
     }
   }

The following subsections presents examples of how to define new emulation
targets for QEMU and renode. For detailed emulation target configuration please
see :ref:`module-pw_emu-config`.

QEMU targets
============
When defining a QEMU emulation target the following keys have to be defined
under ``<target>:qemu``:

 * ``executable``: name of the QEMU executable, e.g. ``qemu-system-arm``,
   ``qemusystem-riscv64``, etc.

 * ``machine``: the QEMU machine name; see ``qemu-system-<arch> -machine help``
   for a list of supported machines names

Here is an example that defines the ``qemu-lm3s6965evb`` target as configuration
fragment:

.. literalinclude:: qemu-lm3s6965evb.json

Since this is an ARM machine note that QEMU executable is defined as
``qemu-system-arm``.

QEMU chardevs can be exposed as host channels under
``<target>:qemu:channels:chardevs:<chan-name>`` where ``chan-name`` is
the name that the channel can be accessed with (e.g. ``pw emu term
<chan-name>``). The ``id`` option is mandatory and it should match a valid
chardev id for the particular QEMU machine.

This target emulates a `Stellaris EEVB <https://www.ti.com/product/LM3S6965>`_
and is compatible with the :ref:`target-lm3s6965evb-qemu` Pigweed target. The
configuration defines a ``serial0`` channel to be the QEMU **chardev** with the
``serial0`` id. The default type of the channel is used, which is TCP and which is
supported by all platforms. The user can change the type by adding a ``type`` key
set to the desired type (e.g. ``pty``)

-------------------
Run target binaries
-------------------
To quickly run target binaries on the host using an emulation target the ``pw
emu run`` command can be used. It will start an emulator instance, connect to a
(given) serial port and load and run the given binary.

.. code-block::

   $ pw emu run --args=-no-reboot qemu-lm3s6965evb out/lm3s6965evb_qemu_gcc_size_optimized/obj/pw_snapshot/test/cpp_compile_test

   --- Miniterm on serial0 ---
   --- Quit: Ctrl+] | Menu: Ctrl+T | Help: Ctrl+T followed by Ctrl+H ---
   INF  [==========] Running all tests.
   INF  [ RUN      ] Status.CompileTest
   INF  [       OK ] Status.CompileTest
   INF  [==========] Done running all tests.
   INF  [  PASSED  ] 1 test(s).
   --- exit ---

Note the ``-no-reboot`` option is passed directly to QEMU and instructs the
emulator to quit instead of rebooting.

---------
Debugging
---------
Debugging target binaries can be done using the ``pw emu gdb`` command. It
requires a running emulator instance which can be started with the ``pw emu
start`` command.

In the following example we are going to debug the ``status_test`` from the
``pw_status`` module compiled for :ref:`target-lm3s6965evb-qemu`. First we are
going to start an emulator instance using the ``qemu-lm3s6965evb`` emulator
target and load the test file:

.. code-block::

   $ pw emu start qemu-lm3s6965evb \
       --file out/lm3s6965evb_qemu_gcc_size_optimized/obj/pw_status/test/status_test


Next, we will start a gdb session connected to the emulator instance:

.. code-block::

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

At this point gdb commands can be used to debug the program.

Once the debugging session is over the emulator instance should be stopped:

.. code-block::

   $ pw emu stop

--------------
Boot debugging
--------------
To debug bootstraping code the ``-p`` option can be used when the emulator is
started:

.. code-block::

   $ pw emu start -p qemu-lm3s6965evb --file out/lm3s6965evb_qemu_gcc_size_optimized/obj/pw_status/test/status_test

That will load the given program but the emulator will not start executing. Next, we can start a debugging session using the ``pw emu gdb`` commands:

.. code-block::

   $ pw emu gdb -e out/lm3s6965evb_qemu_gcc_size_optimized//obj/pw_status/test/status_test
   GNU gdb (Arm GNU Toolchain 12.2.MPACBTI-Rel1 ...
   ...
   Reading symbols from out/lm3s6965evb_qemu_gcc_size_optimized//obj/pw_status/test/status_test...
   Remote debugging using ::1:38723
   pw_boot_Entry () at pw_boot_cortex_m/core_init.c:122
   122	  asm volatile("cpsid i");
   (gdb)

Note that the program is stopped at the ``pw_boot_Entry`` function. From here
you can add breakpoints or step through the program with gdb commands.

------------------------
Using multiple instances
------------------------
To use multiple emulator instances the ``--instance <instance-id>`` option can
be used. The default ``pw emu`` instance id is ``default``.

.. note::
   Internally each emulator instance is identified by a working directory. pw
   emu's working directory is ``$PROJECT_ROOT/.pw_emu/<instance-id>``


As an example, we will try to start the same emulator instance twice:

.. code-block::

   $ pw emu start qemu-lm3s6965evb --file out/lm3s6965evb_qemu_gcc_size_optimized/obj/pw_status/test/status_test
   $ pw emu start qemu-lm3s6965evb --file out/lm3s6965evb_qemu_gcc_size_optimized/obj/pw_status/test/status_test
   pigweed/.pw_emu/default: emulator already started

Note that the second command failed because the default emulator instance is
already running. To start another emulator instance we will use the
``--instance`` or ``-i`` option:


.. code-block::

   $ pw emu -i my-instance start qemu-lm3s6965evb --file out/lm3s6965evb_qemu_gcc_size_optimized/obj/pw_status/test/status_test


Note that no error was triggered this time. Finally, lets stop both emulator
instances:

.. code-block::

   $ pw emu stop -i my-instance
   $ pw emu stop

-------------------------
Adding new emulator types
-------------------------
The pw_emu module can be extended to support new emulator types by providing
implementations for :py:class:`pw_emu.core.Launcher` and
:py:class:`pw_emu.core.Connector` in a dedicated ``pw_emu`` Python module
(e.g. :py:mod:`pw_emu.myemu`) or in an external Python module.

Internal ``pw_emu`` modules must register the connector and launcher
classes by updating
:py:obj:`pw_emu.pigweed_emulators.pigweed_emulators`. For example, the
QEMU implementation sets the following values:

.. code-block:: python

   pigweed_emulators: Dict[str, Dict[str, str]] = {
     ...
     'qemu': {
       'connector': 'pw_emu.qemu.QemuConnector',
       'launcher': 'pw_emu.qemu.QemuLauncher',
      },
     ...

For external emulator frontend modules ``pw_emu`` is using the Pigweed
configuration file to determine the connector and launcher classes, under the
following keys: ``pw_emu:emulators:<emulator_name>:connector`` and
``pw_emu:emulators:<emulator_name>:connector``. Configuration example:

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
methods: :py:meth:`pw_emu.core.Launcher._pre_start`,
:py:class:`pw_emu.core.Launcher._post_start` and
:py:class:`pw_emu.core.Launcher._get_connector`.

There are several abstract methods that need to be implement for the connector,
like :py:meth:`pw_emu.core.Connector.reset` or
:py:meth:`pw_emu.core.Connector.cont`.  These are typically implemented using
internal channels and :py:class:`pw_emu.core.Connector.get_channel_stream`. See
:py:class:`pw_emu.core.Connector` for a complete list of abstract methods that
need to be implemented.
