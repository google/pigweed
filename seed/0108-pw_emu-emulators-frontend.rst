.. role:: python(code)
   :language: python
   :class: highlight

.. _seed-0108:

========================
0108: Emulators Frontend
========================
.. seed::
   :number: 108
   :name: Emulators Frontend
   :status: Accepted
   :proposal_date: 2023-06-24
   :cl: 158190
   :authors: Octavian Purdila
   :facilitator: Armando Montanez

-------
Summary
-------
This SEED proposes a new Pigweed module that allows users to define emulator
targets, start, control and interact with multiple running emulator instances,
either through a command line interface or programmatically through Python APIs.

-----------
Definitions
-----------
An **emulator** is a program that allows users to run unmodified images compiled
for :ref:`target <docs-targets>` on the host machine. The **host** is the machine that
runs the Pigweed environment.

An emulated **machine** or **board** is an emulator abstraction that typically
has a correspondence in the real world - a product, an evaluation board, a
hardware platform.

An emulated machine can be extended / tweaked through runtime configuration
options: add sensors on an i2c bus, connect a disk drive to a disk controller,
etc.

An emulator may use an **object model**, a hierarchical arrangement of emulator
**objects** which are emulated devices (e.g. SPI controller) or internal
emulator structures.

An object can be accessed through an **object path** and can have
**properties**. Device properties controls how the device is emulated
(e.g. enables or disables certain features, defines memory sizes, etc.).

A **channel** is a communication abstraction between the emulator and host
processes. Examples of channels that an emulator can expose to the host:

* An emulated UART could be exposed on the host as a `PTY
  <https://en.wikipedia.org/wiki/Pseudoterminal>`_ or a socket.

* A flash device could be exposed on the host as file.

* A network device could be exposed on the host as a tun/tap interface.

* A remote gdb interface could be exposed to the host as socket.

A **monitor** is a control channel that allows the user to interactively or
programmatically control the emulator: pause execution, inspect the emulator
internal state, connect new devices, etc.

----------
Motivation
----------
While it is already possible to use emulators directly, there is a significant
learning curve for using a specific emulator. Even for the same emulator each
emulated machine (board) has its own peculiarities and it often requires tweaks
to customize it to a specific project's needs through command line options or
scripts (either native emulator scripts, if supported, or through helper shell
scripts).

Once started, the user is responsible for managing the emulator life-cycle,
potentially for multiple instances. They also have to interact with it through
various channels (monitor, debugger, serial ports) that requires some level of
host resource management. Especially in the case of using multiple emulator
instances manually managing host resources are burdensome.

A frequent example is the default debugger ``localhost:1234`` port that can
conflict with multiple emulator instances or with other debuggers running on the
host. Another example: serials exposed over PTY have the pts number in
``/dev/pts/`` allocated dynamically and it requires the user to retrieve it
somehow.

This gets even more complex when using different operating systems where some
type of host resources are not available (e.g. no PTYs on Windows) or with
limited functionality (e.g. UNIX sockets are supported on Windows > Win10 but
only for stream sockets and there is no Python support available yet).

Using emulators in CI is also difficult, in part because host resource
management is getting more complicated due scaling (e.g. more chances of TCP
port conflicts) and restrictions in the execution environment. But also because
of a lack of high level APIs to control emulators and access their channels.

--------
Proposal
--------
Add a new Pigweed module that:

* Allows users to define emulation :ref:`targets <docs-targets>` that
  encapsulate the emulated machine configuration, the tools configuration and
  the host channels configuration.

* Provides a command line interface that manages multiple emulator instances and
  provides interactive access to the emulator's host channels.

* Provides a Python API to control emulator instances and access the emulator's
  host channels.

* Supports multiple emulators, QEMU and renode as a starting point.

* Expose channels for gdb, monitor and user selected devices through
  configurable host resources, sockets and PTYs as a starting point.

The following sections will add more details about the configuration, the
command line interface, the API for controlling and accessing emulators and the
API for adding support for more emulators.


Configuration
=============
The emulators configuration is part of the Pigweed root configuration file
(``pigweed.json``) and reside in the ``pw:pw_emu`` namespace.

Projects can define emulation targets in the Pigweed root configuration file and
can also import predefined targets from other files. The pw_emu module provides
a set of targets as examples and to promote reusability.

For example, the following top level ``pigweed.json`` configuration includes a
target fragment from the ``pw_emu/qemu-lm3s6965evb.json`` file:

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


``pw_emu/qemu-lm3s6965evb.json`` defines the ``qemu-lm3s6965evb`` target
that uses qemu as the emulator and lm3s6965evb as the machine, with the
``serial0`` chardev exposed as ``serial0``:

.. code-block::

   {
     "targets": {
       "qemu-lm3s6965evb": {
         "gdb": "arm-none-eabi-gdb",
         "qemu": {
           "executable": "qemu-system-arm",
           "machine": "lm3s6965evb",
           "channels": {
             "chardevs": {
               "serial0": {
                 "id": "serial0"
               }
             }
           }
         }
       }
     }
   }

This target emulates a stm32f405 SoC and is compatible with the
:ref:`target-lm3s6965evb-qemu` Pigweed build target.

The configuration defines a ``serial0`` channel to be the QEMU **chardev** with
the ``serial0`` id. The default type of the channel is used, which is TCP and
which is supported by all platforms. The user can change the type by adding a
``type`` key set to the desired type (e.g. ``pty``).

The following configuration fragment defines a target that uses renode:

.. code-block::

   {
     "targets": {
       "renode-stm32f4_discovery": {
         "gdb": "arm-none-eabi-gdb",
         "renode": {
           "executable": "renode",
           "machine": "platforms/boards/stm32f4_discovery-kit.repl",
           "channels": {
             "terminals": {
               "serial0": {
                 "device-path": "sysbus.uart0",
                 "type": "pty"
               }
             }
           }
         }
       }
     }
   }

Note that ``machine`` is used to identify which renode script to use to load the
plaform description from and ``terminals`` to define which UART devices to
expose to the host. Also note the ``serial0`` channel is set to be exposed as a
PTY on the host.

The following channel types are currently supported:

* ``pty``: supported on Mac and Linux; renode only supports PTYs for
  ``terminals`` channels.

* ``tcp``: supported on all platforms and for all channels; it is also the
  default type if no channel type is configured.

The channel configuration can be set at multiple levels: emulator, target, or
specific channel. The channel configuration takes precedence, then the target
channel configuration then the emulator channel configuration.

The following expressions are replaced in the configuration strings:

* ``$pw_emu_wdir{relative-path}``: replaces statement with an absolute path by
  concatenating the emulator's working directory with the given relative path.

* ``$pw_emu_channel_port{channel-name}``: replaces the statement with the port
  number for the given channel name; the channel type should be ``tcp``.

* ``$pw_emu_channel_host{channel-name}``: replaces the statement with the host
  for the given channel name; the channel type should be ``tcp``.

* ``$pw_emu_channel_path{channel-name}``: replaces the statement with the path
  for the given channel name; the channel type should be ``pty``.

Besides running QEMU and renode as the main emulator, the target configuration
allows users to start other programs before or after starting the main emulator
process. This allows extending the emulated target with simulation or emulation
outside of the main emulator. For example, for BLE emulation the main emulator
could emulate just the serial port while the HCI emulation done is in an
external program (e.g. `bumble <https://google.github.io/bumble>`_, `netsim
<https://android.googlesource.com/platform/tools/netsim>`_).


Command line interface
======================
The command line interfaces enables users to control emulator instances and
access their channels interactively.

.. code-block:: text

   usage: pw emu [-h] [-i STRING] [-w WDIR] {command} ...

   Pigweed Emulators Frontend

    start               Launch the emulator and start executing, unless pause
                        is set.
    restart             Restart the emulator and start executing, unless pause
                        is set.
    run                 Start the emulator and connect the terminal to a
                        channel. Stop the emulator when exiting the terminal
    stop                Stop the emulator
    load                Load an executable image via gdb. If pause is not set
                        start executing it.
    reset               Perform a software reset.
    gdb                 Start a gdb interactive session
    prop-ls             List emulator object properties.
    prop-get            Show the emulator's object properties.
    prop-set            Show emulator's object properties.
    gdb-cmds            Run gdb commands in batch mode.
    term                Connect with an interactive terminal to an emulator
                        channel

   optional arguments:
    -h, --help            show this help message and exit
    -i STRING, --instance STRING
                          instance to use (default: default)
    -w WDIR, --wdir WDIR  path to working directory (default: None)

   commands usage:
       usage: pw emu start [-h] [--file FILE] [--runner {None,qemu,renode}]
                     [--args ARGS] [--pause] [--debug] [--foreground]
                           {qemu-lm3s6965evb,qemu-stm32vldiscovery,qemu-netduinoplus2}
        usage: pw emu restart [-h] [--file FILE] [--runner {None,qemu,renode}]
                      [--args ARGS] [--pause] [--debug] [--foreground]
                      {qemu-lm3s6965evb,qemu-stm32vldiscovery,qemu-netduinoplus2}
        usage: pw emu stop [-h]
        usage: pw emu run [-h] [--args ARGS] [--channel CHANNEL]
                      {qemu-lm3s6965evb,qemu-stm32vldiscovery,qemu-netduinoplus2} FILE
        usage: pw emu load [-h] [--pause] FILE
        usage: pw emu reset [-h]
        usage: pw emu gdb [-h] [--executable FILE]
        usage: pw emu prop-ls [-h] path
        usage: pw emu prop-get [-h] path property
        usage: pw emu prop-set [-h] path property value
        usage: pw emu gdb-cmds [-h] [--pause] [--executable FILE] gdb-command [gdb-command ...]
        usage: pw emu term [-h] channel

For example, the ``run`` command is useful for quickly running ELF binaries on an
emulated target and seeing / interacting with a serial channel. It starts an
emulator, loads an images, connects to a channel and starts executing.

.. code-block::

   $ pw emu run qemu-netduinoplus2 out/stm32f429i_disc1_debug/obj/pw_snapshot/test/cpp_compile_test.elf

   --- Miniterm on serial0 ---
   --- Quit: Ctrl+] | Menu: Ctrl+T | Help: Ctrl+T followed by Ctrl+H ---
   INF  [==========] Running all tests.
   INF  [ RUN      ] Status.CompileTest
   INF  [       OK ] Status.CompileTest
   INF  [==========] Done running all tests.
   INF  [  PASSED  ] 1 test(s).
   --- exit ---

Multiple emulator instances can be run and each emulator instance is identified
by its working directory. The default working directory for ``pw emu`` is
``$PW_PROJECT_ROOT/.pw_emu/<instance-id>`` where ``<instance-id>`` is a command
line option that defaults to ``default``.

For more complex usage patterns, the ``start`` command can be used which will
launch an emulator instance in the background. Then, the user can debug the
image with the ``gdb`` command, connect to a channel (e.g. serial port) with the
``term`` command, reset the emulator with the ``reset`` command, inspect or
change emulator properties with the ``prop-ls``, ``prop-get``, ``prop-set`` and
finally stop the emulator instance with ``stop``.


Python APIs
===========
The pw_emu module offers Python APIs to launch, control and interact with an
emulator instance.

The following is an example of using these APIs which implements a simplified
version of the ``run`` pw_emu CLI command:

.. code-block:: python

   # start an emulator instance and load the image to execute
   # pause the emulator after loading the image
   emu = Emulator(args.wdir)
   emu.start(args.target, args.file, pause=True)

   # determine the channel type and create a pyserial compatible URL
   chan_type = emu.get_channel_type(args.chan)
   if chan_type == 'tcp':
       host, port = emu.get_channel_addr(chan)
       url = f'socket://{host}:{port}'
    elif chan_type == 'pty':
        url =  emu.get_channel_path(chan)
    else:
        raise Error(f'unknown channel type `{chan_type}`')

   # open the serial port and create a miniterm instance
   serial = serial_for_url(url)
   serial.timeout = 1
   miniterm = Miniterm(serial)
   miniterm.raw = True
   miniterm.set_tx_encoding('UTF-8')
   miniterm.set_rx_encoding('UTF-8')

   # now that we are connected to the channel we can unpause
   # this approach will prevent and data loses
   emu.cont()

   miniterm.start()
   try:
       miniterm.join(True)
   except KeyBoardInterrupt:
       pass
   miniterm.stop()
   miniterm.join()
   miniterm.close()

For convenience, a ``TemporaryEmulator`` class is also provided.

It manages emulator instances that run in temporary working directories. The
emulator instance is stopped and the working directory is cleared when the with
block completes.

It also supports interoperability with the pw emu cli, i.e.  starting the
emulator with the CLI and controlling / interacting with it from the API.

Usage example:

.. code-block:: python

   # programmatically start and load an executable then access it
   with TemporaryEmulator() as emu:
       emu.start(target, file)
       with emu.get_channel_stream(chan) as stream:
           ...


    # or start it form the command line then access it programmatically
    with TemporaryEmulator() as emu:
        build.bazel(
            ctx,
            "run",
            exec_path,
            "--run_under=pw emu start <target> --file "
        )

        with emu.get_channel_stream(chan) as stream:
            ...


Intended API shape
------------------
This is not an API reference, just an example of the probable shape of the final
API.

:python:`class Emulator` is used to launch, control and interact with an
emulator instance:

.. code-block:: python

   def start(
       self,
       target: str,
       file: Optional[os.PathLike] = None,
       pause: bool = False,
       debug: bool = False,
       foreground: bool = False,
       args: Optional[str] = None,
   ) -> None:

|nbsp|
   Start the emulator for the given target.

   If file is set that the emulator will load the file before starting.

   If pause is True the emulator is paused until the debugger is connected.

   If debug is True the emulator is run in foreground with debug output
   enabled. This is useful for seeing errors, traces, etc.

   If foreground is True the emulator is run in foreground otherwise it is
   started in daemon mode. This is useful when there is another process
   controlling the emulator's life cycle (e.g. cuttlefish)

   args are passed directly to the emulator

:python:`def running(self) -> bool:`
   Check if the main emulator process is already running.

:python:`def stop(self) -> None`
   Stop the emulator

:python:`def get_gdb_remote(self) -> str:`
   Return a string that can be passed to the target remote gdb command.

:python:`def get_gdb(self) -> Optional[str]:`
   Returns the gdb command for current target.

.. code-block:: python

   def run_gdb_cmds(
       commands : list[str],
       executable: Optional[Path] = None,
       pause: bool = False
   ) -> subprocess.CompletedProcess:

|nbsp|
   Connect to the target and run the given commands silently
   in batch mode.

   The executable is optional but it may be required by some gdb
   commands.

   If pause is set do not continue execution after running the
   given commands.

:python:`def reset() -> None`
   Performs a software reset

:python:`def list_properties(self, path: str) -> List[Dict]`
   Returns the property list for an emulator object.

   The object is identified by a full path. The path is target specific and
   the format of the path is emulator specific.

   QEMU path example: /machine/unattached/device[10]

   renode path example: sysbus.uart

:python:`def set_property(path: str, prop: str, value: str) -> None`
   Sets the value of an emulator's object property.

:python:`def get_property(self, path: str, prop: str) -> None`
   Returns the value of an emulator's object property.

:python:`def get_channel_type(self, name: str) -> str`
   Returns the channel type.

   Currently ``pty`` or ``tcp`` are the only supported types.

:python:`def get_channel_path(self, name: str) -> str:`
   Returns the channel path. Raises InvalidChannelType if this is not a PTY
   channel.

:python:`def get_channel_addr(self, name: str) -> tuple:`
   Returns a pair of (host, port) for the channel. Raises InvalidChannelType
   if this is not a tcp channel.

.. code-block:: python

   def get_channel_stream(
       name: str,
       timeout: Optional[float] = None
   ) -> io.RawIOBase:

|nbsp|
   Returns a file object for a given host exposed device.

   If timeout is None than reads and writes are blocking. If timeout is zero the
   stream is operating in non-blocking mode. Otherwise read and write will
   timeout after the given value.

:python:`def get_channels(self) -> List[str]:`
   Returns the list of available channels.

:python:`def cont(self) -> None:`
   Resume the emulator's execution

---------------------
Problem investigation
---------------------
Pigweed is missing a tool for basic emulators control and as shown in the
motivation section directly using emulators directly is difficult.

While emulation is not a goal for every project, it is appealing for some due
to the low cost and scalability. Offering a customizable emulators frontend in
Pigweed will make this even better for downstream projects as the investment to
get started with emulation will be lower - significantly lower for projects
looking for basic usage.

There are two main use-cases that this proposal is addressing:

* Easier and robust interactive debugging and testing on emulators.

* Basic APIs for controlling and accessing emulators to help with emulator
  based testing (and trivial CI deployment - as long as the Pigweed bootstrap
  process can run in CI).

The proposal focuses on a set of fairly small number of commands and APIs in
order to minimize complexity and gather feedback from users before adding more
features.

Since the state of emulated boards may different between emulators, to enable
users access to more emulated targets, the goal of the module is to support
multiple emulators from the start.

Two emulators were selected for the initial implementation: QEMU and
renode. Both run on all Pigweed currently supported hosts (Linux, Mac, Windows)
and there is good overlap in terms of APIs to configure, start, control and
access host exposed channels to start with the two for the initial
implementation. These emulators also have good support for embedded targets
(with QEMU more focused on MMU class machines and renode fully focused on
microcontrollers) and are widely used in this space for emulation purposes.


Prior art
=========
While there are several emulators frontends available, their main focus is on
graphical interfaces (`aqemu <https://sourceforge.net/projects/aqemu/>`_,
`GNOME Boxes <https://wiki.gnome.org/Apps/Boxes>`_,
`QtEmu <https://gitlab.com/qtemu/gui>`_,
`qt-virt-manager <https://f1ash.github.io/qt-virt-manager/>`_,
`virt-manager <https://virt-manager.org/>`_) and virtualization (
`virsh <https://www.libvirt.org/>`_,
`guestfish <https://libguestfs.org/>`_).
`qemu-init <https://github.com/mm1ke/qemu-init>`_ is a qemu CLI frontend but since
it is written in bash it does not work on Windows nor is easy to retrofit it to
add Python APIs for automation.

.. inclusive-language: disable

The QEMU project has a few `Python modules
<https://github.com/qemu/qemu/tree/master/python/qemu>`_ that are used
internally for testing and debugging QEMU. :python:`qemu.machine.QEMUMachine`
implements a QEMU frontend that can start a QEMU process and can interact with
it. However, it is clearly marked for internal use only, it is not published on
pypi or with the QEMU binaries. It is also not as configurable for pw_emu's
use-cases (e.g. does not support running the QEMU process in the background,
does not multiple serial ports, does not support configuring how to expose the
serial port, etc.). The :python:`qemu.qmp` module is `published on pypi
<https://pypi.org/project/qemu.qmp/>`_ and can be potentially used by `pw_emu`
to interact with the emulator over the QMP channel.

.. inclusive-language: enable

---------------
Detailed design
---------------
The implementation supports QEMU and renode as emulators and works on
Linux, Mac and Windows.

Multiple instances are supported in order to enable developers who work on
multiple downstream Pigweed projects to work unhindered and also to run
multiple test instances in parallel on the same machine.

Each instance is identified by a system absolute path that is also used to store
state about the running instance such as pid files for running processes,
current emulator and target, etc. This directory also contains information about
how to access the emulator channels (e.g. socket ports, PTY paths)

.. mermaid::

   graph TD;
       TemporaryEmulator & pw_emu_cli[pw emu cli] <--> Emulator
       Emulator <--> Launcher & Connector
       Launcher  <--> Handles
       Connector <--- Handles
       Launcher <--> Config
       Handles --Save--> WD --Load--> Handles
       WD[Working Directory]

The implementation uses the following classes:

* :py:class:`pw_emu.Emulator`: the user visible APIs

* :py:class:`pw_emu.core.Launcher`: an abstract class that starts an emulator
  instance for a given configuration and target

* :py:class:`pw_emu.core.Connector`: an abstract class that is the interface
  between a running emulator and the user visible APIs

* :py:class:`pw_emu.core.Handles`: class that stores specific information about
  a running emulator instance such as ports to reach emulator channels; it is
  populated by :py:class:`pw_emu.core.Launcher` and saved in the working
  directory and used by :py:class:`pw_emu.core.Connector` to access the emulator
  channels, process pids, etc.

* :py:class:`pw_emu.core.Config`: loads the pw_emu configuration and provides
  helper methods to get and validate configuration options


Documentation update
====================
The following documentation should probably be updated to use ``pw emu`` instead
of direct QEMU invocation: :ref:`module-pw_rust`,
:ref:`target-lm3s6965evb-qemu`. The referenced QEMU targets are defined in
fragment configuration files in the pw_emu module and included in the top level
pigweed.json file.

------------
Alternatives
------------
UNIX sockets were investigated as an alternative to TCP for the host exposed
channels. UNIX sockets main advantages over TCP is that it does not require
dynamic port allocation which simplifies the bootstrap of the emulator (no need
to query the emulator to determine which ports were allocated). Unfortunately,
while Windows supports UNIX sockets since Win10, Python still does not support
them on win32 platforms. renode also does not support UNIX sockets.

--------------
Open questions
--------------

Renode dynamic ports
====================
While renode allows passing 0 for ports to allocate a dynamic port, it does not
have APIs to retrieve the allocated port. Until support for such a feature is
added upstream, the following technique can be used to allocate a port
dynamically:

.. code-block:: python

   sock = socket.socket(socket.SOCK_INET, socket.SOCK_STREAM)
   sock.bind(('', 0))
   _, port = socket.getsockname()
   sock.close()

There is a race condition that allows another program to fetch the same port,
but it should work in most light use cases until the issue is properly resolved
upstream.

qemu_gcc target
===============
It should still be possible to call QEMU directly as described in
:ref:`target-lm3s6965evb-qemu` however, once ``pw_emu`` is implemented it is
probably better to define a lm3s6965evb emulation target and update the
documentation to use ``pw emu`` instead of the direct QEMU invocation.


.. |nbsp| unicode:: 0xA0
   :trim:
