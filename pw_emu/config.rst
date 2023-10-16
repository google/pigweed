.. _module-pw_emu-config:

=============
Configuration
=============
.. pigweed-module-subpage::
   :name: pw_emu
   :tagline: Emulators frontend

The emulators configuration is part of the Pigweed root configuration file
(``pigweed.json``) and reside in the ``pw:pw_emu`` namespace.

Projects can define emulation targets in the Pigweed root configuration file and
can also import predefined targets from other files. The ``pw_emu`` module
provides a set of targets as examples and to promote reusability.

The target configuration allows users to start other programs before
or after starting the main emulator process. This allows extending the
emulated target with simulation or emulation outside of the main
emulator. For example, for BLE emulation the main emulator could
emulate just the serial port while the HCI emulation done is in an
external program (e.g. `bumble <https://google.github.io/bumble>`_,
`netsim <https://android.googlesource.com/platform/tools/netsim>`_).

.. _module-pw_emu-config-options:

-----------------------
Configuration reference
-----------------------
* ``gdb``: top level gdb command as a list of strings
   (e.g. ``[gdb-multiarch]``); this can be overridden at the target level

* ``target_files``: list of paths to json files to include targets from; the
  target configuration should be placed under a ``target`` key; if paths are
  relative they are interpreted relative to the project root directory

* ``qemu``: options for the QEMU emulator:

  * ``executable`: command used to start QEMU (e.g. `system-arm-qemu``); this
    can be overridden at the target level

  * ``args``: list of command line options to pass directly to QEMU
    when starting an emulator instance; can be *extended* at the
    target level

  * ``channels``: options for channel configuration:

    * ``type``: optional type for channels, see channel types below

    * ``gdb``, ``qmp``, ``monitor``: optional channel configuration entries

      * ``type``: channel type, see channel types below

* ``renode``: options for the renode emulator:

  * ``executable``: command used to start renode (e.g. "system-arm-qemu"); this
    can be overridden at the target level

  * ``channels``: options for channel configuration:

    * ``terminals``: exposed terminal devices (serial ports) generic options:

      * ``type``: optional type for channels, see channel types below

* ``targets``: target configurations with target names as keys:

  * ``<target-name>``: target options:

    * ``gdb``: gdb command as a list of strings (e.g. ``[arm-none-eabi-gdb]``)

    * ``pre-start-cmds``: commands to run before the emulator is started

      * ``<name>``: command for starting the process as a list of strings

    * ``post-start-cmds``: processes to run after the emulator is started

      * ``<name>``: command for starting the process as a list of strings

    * ``qemu``: options for the QEMU emulator:

      * ``executable``: command used to start QEMU (e.g. ``system-arm-qemu``)

      * ``args``: list of command line options passed directly to QEMU when
        starting this target

      * ``machine``: QEMU machine name; passed to the QEMU ``-machine`` command
        line argument

      * ``channels``: exposed channels:

	* ``chardevs``: exposed QEMU chardev devices (typically serial
          ports):

	  * ``<channel-name>``: channel options:

	    * ``id``: the id of the QEMU chardev

	    * ``type``: optional type of the channel see channel types below

	* ``gdb``, ``qmp`, ``monitor``: optional channel configuration
          entries

	  * ``type``: channel type, see channel types below

The following channel types are currently supported:

* ``pty``: supported on Mac and Linux; renode only supports ptys for
  ``terminals`` channels

* ``tcp``: supported on all platforms and for all channels; it is also the
  default type if no channel type is configured

The channel configuration can be set at multiple levels: emulator, target, or
specific channel. The channel configuration takes precedence, then the target
channel configuration then the emulator channel configuration.

The following expressions are substituted in the ``pre-start-cmd`` and
``post-start-cmd`` strings:

* ``$pw_emu_wdir{relative-path}``: replaces statement with an absolute path
  by concatenating the emulator's working directory with the given relative path

* ``$pw_emu_channel_port{channel-name}``: replaces the statement with the port
  number for the given channel name; the channel type should be ``tcp``

* ``$pw_emu_channel_host{channel-name}``: replaces the statement with the host
  for the given channel name; the channel type should be ``tcp``

* ``$pw_emu_channel_path{channel-name}``: replaces the statement with the path
  for the given channel name; the channel type should be ``pty``
