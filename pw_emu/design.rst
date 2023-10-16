.. _module-pw_emu-design:

======
Design
======
.. pigweed-module-subpage::
   :name: pw_emu
   :tagline: Emulators frontend

Multiple instances are supported in order to enable developers who work on
multiple downstream Pigweed projects to work unhindered and also to run multiple
test instances in parallel on the same machine.

Each instance is identified by a system absolute path that is also used to store
state about the running instance such as pid files for running processes,
current emulator and target, etc. This directory also contains information about
how to access the emulator channels (e.g. socket ports, pty paths)

.. mermaid::

   graph TD;
       TemporaryEmulator & pw_emu_cli[pw emu cli] <--> Emulator
       Emulator <--> Launcher & Connector
       Launcher  <--> Handles
       Connector <--> Handles
       Launcher <--> Config
       Handles --Save--> WD --Load--> Handles
       WD[Working Directory]


The implementation uses the following classes:

* :py:class:`pw_emu.frontend.Emulator`: the user visible API

* :py:class:`pw_emu.core.Launcher`: an abstract class that starts an
  emulator instance for a given configuration and target

* :py:class:`pw_emu.core.Connector`: an abstract class that is the
  interface between a running emulator and the user visible APIs

* :py:class:`pw_emu.core.Handles`: class that stores specific
  information about a running emulator instance such as ports to reach emulator
  channels; it is populated by :py:class:`pw_emu.core.Launcher` and
  saved in the working directory and used by
  :py:class:`pw_emu.core.Connector` to access the emulator channels,
  process pids, etc.

* :py:class:`pw_emu.core.Config`: loads the pw_emu configuration and provides
  helper methods to get and validate configuration options

-------------------
Emulator properties
-------------------
The implementation exposes the ability to list, read and write emulator
properties. The frontend does not abstract properties in a way that is emulator
or even emulator target independent, other than mandating that each property is
identified by a path. Note that the format of the path is also emulator specific
and not standardized.

----
QEMU
----
The QEMU frontend is using `QMP <https://wiki.qemu.org/Documentation/QMP>`_ to
communicate with the running QEMU process and implement emulator specific
functionality like reset, list or reading properties, etc.

QMP is exposed to the host through two channels: a temporary one to establish
the initial connection that is used to read the dynamic configuration (e.g. TCP
ports, pty paths) and a permanent one that can be used thought the life of the
QEMU processes. The frontend is configuring QEMU to expose QMP to a
``localhost`` TCP port reserved by the frontend and then waiting for QEMU to
establish the connection on that port. Once the connection is established the
frontend will read the configuration of the permanent QMP channel (which can be
either a TCP port or a PTY path) and save it as a channel named ``qmp`` in the
:py:class:`pw_emu.core.Handles` object.
