.. _module-pw_emu-design:

======
Design
======
.. pigweed-module-subpage::
   :name: pw_emu
   :tagline: pw_emu: Flexible emulators frontend

-----------
Parallelism
-----------

``pw_emu`` supports running multiple instances simultaneously. This is helpful
for developers who work on multiple downstream Pigweed projects or who want
to run multiple tests in parallel on the same machine.

Each instance is identified by a system absolute path that is also used to store
state about the running instance such as ``pid`` files for running processes,
current emulator and target, etc. This directory also contains information about
how to access the emulator channels, e.g. socket ports, PTY paths, etc.

.. mermaid::

   graph TD;
       TemporaryEmulator & pw_emu_cli[pw emu cli] <--> Emulator
       Emulator <--> Launcher & Connector
       Launcher  <--> Handles
       Connector <--> Handles
       Launcher <--> Config
       Handles --Save--> WD --Load--> Handles
       WD[Working Directory]

-----------
API surface
-----------

The implementation uses the following classes:

* :py:class:`pw_emu.frontend.Emulator` - The user-visible API.
* :py:class:`pw_emu.core.Launcher` - An abstract class that starts an
  emulator instance for a given configuration and target.
* :py:class:`pw_emu.core.Connector` - An abstract class that is the
  interface between a running emulator and the user-visible APIs.
* :py:class:`pw_emu.core.Handles` - A class that stores specific
  information about a running emulator instance such as ports to reach emulator
  channels; it is populated by :py:class:`pw_emu.core.Launcher` and
  saved in the working directory and used by
  :py:class:`pw_emu.core.Connector` to access the emulator channels,
  process PIDs, etc.
* :py:class:`pw_emu.core.Config` - Loads the ``pw_emu`` configuration and
  provides helper methods to get and validate configuration options.

-------------------
Emulator properties
-------------------
The implementation exposes the ability to list, read, and write emulator
properties. The frontend does not abstract properties in a way that is
emulator-independent or even emulator-target-independent, other than mandating
that each property is identified by a path. Note that the format of the path is
also emulator-specific and not standardized.

----
QEMU
----
The QEMU frontend is using `QMP <https://wiki.qemu.org/Documentation/QMP>`_ to
communicate with the running QEMU process and implement emulator-specific
functionality like reset, list properties, reading properties, etc.

QMP is exposed to the host through two channels: a temporary one to establish
the initial connection that is used to read the dynamic configuration (e.g. TCP
ports, PTY paths) and a permanent one that can be used throughout the life of
the QEMU processes. The frontend is configuring QEMU to expose QMP to a
``localhost`` TCP port reserved by the frontend and then waiting for QEMU to
establish the connection on that port. Once the connection is established the
frontend reads the configuration of the permanent QMP channel (which can be
either a TCP port or a PTY path) and save it as a channel named ``qmp`` in the
:py:class:`pw_emu.core.Handles` object.

------
Renode
------
The Renode frontend uses `robot port
<https://renode.readthedocs.io/en/latest/introduction/testing.html>`_ to
interact with the Renode process. Although the robot interface is designed for
testing and not as a control interface, it is more robust and better suited to
be used as a machine interface than the alternative ``monitor`` interface which
is a user-oriented, ANSI-colored, echoed, log-mixed, telnet interface.

Bugs
====
While Renode allows passing ``0`` for ports to allocate a dynamic port, it does
not have APIs to retrieve the allocated port. Until support for such a feature
is added upstream, the implementation is using the following technique to
allocate a port dynamically:

.. code-block:: py

   sock = socket.socket(socket.SOCK_INET, socket.SOCK_STREAM)
   sock.bind(('', 0))
   _, port = socket.getsockname()
   sock.close()

There is a race condition that allows another program to fetch the same port,
but it should work in most light use cases until the issue is properly resolved
upstream.

----------------
More pw_emu docs
----------------
.. include:: docs.rst
   :start-after: .. pw_emu-nav-start
   :end-before: .. pw_emu-nav-end
