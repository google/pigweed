.. _module-pw_emu-config:

=============
Configuration
=============
.. pigweed-module-subpage::
   :name: pw_emu

``pw_emu`` configuration is done in the ``pw.pw_emu`` namespace of
:ref:`pigweed.json <seed-0101>`.

The target configuration allows users to start other programs before
or after starting the main emulator process. This allows extending the
emulated target with simulation or emulation outside of the main
emulator. For example, for BLE emulation the main emulator could
emulate just the serial port while the HCI emulation done is in an
external program (e.g. `bumble <https://google.github.io/bumble>`_,
`netsim <https://android.googlesource.com/platform/tools/netsim>`_).

.. _module-pw_emu-config-fragments:

-----------------------
Configuration fragments
-----------------------
Emulation targets can be stored in JSON files outside of ``pigweed.json``.
These are known as *configuration fragments*. ``pigweed.json`` imports these
fragments via ``target_files``.

Example configuration fragments:

* `//pw_emu/qemu-lm3s6965evb.json <https://cs.opensource.google/pigweed/pigweed/+/main:pw_emu/qemu-lm3s6965evb.json>`_
* `//pw_emu/renode-stm32f4_discovery.json <https://cs.opensource.google/pigweed/pigweed/+/main:pw_emu/renode-stm32f4_discovery.json>`_

.. _module-pw_emu-config-options:

-----------------------
Configuration reference
-----------------------
The following list explains the valid schema for the ``pw_emu`` dict in
:ref:`pigweed.json <seed-0101>`.

.. caution::

   All the values below should be nested under ``pw.pw_emu`` in ``pigweed.json``.
   For example, the top-level ``gdb`` item in the list below actually lives
   here:

   .. code-block:: json

      {
        "pw": {
          "pw_emu": {
            "gdb": ["..."]
          }
        }
      }

.. Note to maintainers: Multi-level unordered lists in reST are very finicky.
.. The syntax below is the only one that works (e.g. spaces are significant).

* ``gdb`` (list of strings) - The default GDB command to run.
  Can be overridden at the target level.

* ``target_files`` (list of strings) - Relative or absolute paths
  to :ref:`module-pw_emu-config-fragments`. Relative paths are
  interpreted relative to the project root directory.

* ``qemu`` (dict) - QEMU options.

  * ``executable`` (string) - The default command for starting
    QEMU, e.g. ``system-arm-qemu``. Can be overridden at the target level.

  * ``args`` (list of strings) - Command line options to pass
    directly to QEMU when ``executable`` is invoked. Can be **extended** (not
    overridden) at the target level.

  * ``channels`` (dict) - Channel options.

    * ``type`` (string) - The :ref:`channel type
      <module-pw_emu-config-channel-types>`. Optional.

    * ``gdb`` (dict) - Channel-specific GDB options. Optional.

      * ``type`` (string) - The :ref:`channel type
        <module-pw_emu-config-channel-types>`. Optional.

    * ``qmp`` (dict) - Channel-specific QMP options. Optional.

      * ``type`` (string) - The :ref:`channel type
        <module-pw_emu-config-channel-types>`. Optional.

    * ``monitor`` (dict) - Channel-specific Monitor options. Optional.

      * ``type`` (string) - The :ref:`channel type
        <module-pw_emu-config-channel-types>`. Optional.

* ``renode`` (dict) - Renode options.

  * ``executable`` (string) - The default command for starting Renode. Can be
    overridden at the target level.

  * ``channels`` (dict) - Channel options.

    * ``terminals`` (dict) - Generic options for exposed terminal devices, e.g.
      serial ports.

      * ``type`` (string) - The :ref:`channel type
        <module-pw_emu-config-channel-types>`. Optional.

* ``targets`` (dict) - Target configuration. Each key of this dict represents
  a target name.

  * ``<target-name>`` (dict) - Configuration for a single target. Replace
    ``<target-name>`` with a real target name, e.g. ``qemu-lm3s6965evb``.

    * ``gdb`` (list of strings) - The GDB command to run for this target.
      Overrides the top-level ``gdb`` command.

    * ``pre-start-cmds`` (dict) - Commands to run before the emulator
      is started. See also :ref:`module-pw_emu-config-pre-post-substitutions`.

      * ``<command-id>`` (list of strings) - A pre-start command.
        Replace ``<command-id>`` with a descriptive name for the command.

    * ``post-start-cmds`` (dict) - Commands to run after the emulator
      is started. See also :ref:`module-pw_emu-config-pre-post-substitutions`.

      * ``<command-id>`` (list of strings) - A post-start command.
        Replace ``<command-id>`` with a descriptive name.

    * ``qemu`` (dict) - QEMU options for this target. Overrides the top-level
      ``qemu`` command.

      * ``executable`` (string) - The command for starting QEMU on this target.
        Required.

      * ``machine`` (string) - The QEMU ``-machine`` value for this target,
        e.g. ``stm32vldiscovery``.  See ``qemu-system-<arch> -machine help``
        for a list of supported machines. Required.

      * ``args`` (list of strings) - Command line options to pass
        directly to QEMU when ``executable`` is invoked. This value **extends**
        the top-level ``args`` value; it does *not* override it. Optional.

      * ``channels`` (dict) - Channel options for this target.

	* ``chardevs`` (dict) - QEMU chardev device configuration. Usually
          serial ports.

	  * ``<channel-name>`` (dict) - The configuration for a single channel.
            Replace ``<channel-name>`` with a descriptive name.

	    * ``id`` (string) - The ID of the QEMU chardev.

            * ``type`` (string) - The :ref:`channel type
              <module-pw_emu-config-channel-types>`. Optional.

            * ``gdb`` (dict) - Channel-specific GDB options. Optional.

              * ``type`` (string) - The :ref:`channel type
                <module-pw_emu-config-channel-types>`. Optional.

            * ``qmp`` (dict) - Channel-specific QMP options. Optional.

              * ``type`` (string) - The :ref:`channel type
                <module-pw_emu-config-channel-types>`. Optional.

            * ``monitor`` (dict) - Channel-specific Monitor options. Optional.

              * ``type`` (string) - The :ref:`channel type
                <module-pw_emu-config-channel-types>`. Optional.

    * ``renode`` (dict) - Renode options for this target.

      * ``executable`` (string) - The command for starting Renode on this target,
        e.g. ``renode``.

      * ``machine`` (string) - The Renode script to use for machine definitions,
        e.g. ``platforms/boards/stm32f4_discovery-kit.repl``.

      * ``channels`` (dict) - Channel options.

        * ``terminals`` (dict) - Exposed terminal devices, usually serial ports.

          * ``<device-name>`` (dict) - Device configuration. Replace ``<device-name>``
            with a descriptive name, e.g. ``serial0``.

            * ``device-path`` (string) - The path to the device, e.g.
              ``sysbus.usart1``.

            * ``type`` (string) - The :ref:`channel type
              <module-pw_emu-config-channel-types>`. Optional.

.. _module-pw_emu-config-channel-types:

Channel types
=============
The following channel types are currently supported:

* ``pty`` - Full support on macOS and Linux. Renode only supports PTYs for
  ``terminals``.

* ``tcp`` - Full support on all platforms and channels. This is the default
  value if no channel type is configured.

The channel configuration can be set at the emulator, target, or channel level.
The channel level takes precedence, then the target level, then the emulator
level.

.. _module-pw_emu-config-pre-post-substitutions:

Pre-start and post-start expression substitutions
=================================================
The following expressions are substituted in the ``pre-start-cmd`` and
``post-start-cmd`` strings:

* ``$pw_emu_wdir{relative-path}`` - Replaces the statement with an absolute path
  by concatenating the emulator's working directory with the given relative path.

* ``$pw_emu_channel_port{channel-name}`` - Replaces the statement with the port
  number for the given channel name. The channel type should be ``tcp``.

* ``$pw_emu_channel_host{channel-name}`` - Replaces the statement with the host
  for the given channel name. The channel type should be ``tcp``.

* ``$pw_emu_channel_path{channel-name}`` - Replaces the statement with the path
  for the given channel name. The channel type should be ``pty``.

.. _module-pw_emu-config-string-substitutions:

Configuration string substitutions
==================================
The following expressions are substituted in configuration strings, including
:ref:`module-pw_emu-config-fragments`:

* ``$pw_env{envvar}`` - Replaces the statement with the value of ``envvar``.
  If ``envvar`` doesn't exist a configuration error is raised.

----------------
More pw_emu docs
----------------
.. include:: docs.rst
   :start-after: .. pw_emu-nav-start
   :end-before: .. pw_emu-nav-end
