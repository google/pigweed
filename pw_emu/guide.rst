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
