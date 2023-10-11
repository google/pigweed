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
       Launcher  <--> Handles
       Connector <--> Handles
       Launcher <--> Config
       Handles --Save--> WD --Load--> Handles
       WD[Working Directory]


The implementation uses the following classes:

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
