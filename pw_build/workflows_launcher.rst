.. _module-pw_build-workflows-launcher:

==================
Workflows Launcher
==================
.. pigweed-module-subpage::
   :name: pw_build

The workflows launcher is a command-line tool for running tools, builds, and
other workflows defined in a ``workflows.json`` file. It provides a centralized
and discoverable way to execute common development tasks.

.. admonition:: Note
   :class: warning

   `b/425973227 <https://pwbug.dev/425973227>`__: This tool is currently under
   active development, and will change significantly as the experience is
   refined.

.. _module-pw_build-workflows-launcher-getting-started:

---------------
Getting Started
---------------
.. tab-set::

   .. tab-item:: Bazel
      :sync: bazel

      .. code-block:: console

         bazelisk run //pw_build/py:workflows_launcher

   .. tab-item:: GN (bootstrapped environment)
      :sync: gn

      .. code-block:: console

         python -m pw_build.workflows.launcher

The launcher will automatically find and load a ``workflows.json`` file in the
current directory or any parent directory.

--------
Commands
--------
The Workflows launcher provides a set of built-in commands, and will later
support commands generated from the ``workflows.json`` file.

Built-in Commands
=================

``describe``
------------
The ``describe`` command prints a human-readable description of a ``Tool``,
``Build``, ``BuildConfig``, or ``TaskGroup`` from the ``workflows.json`` file.
This is useful for inspecting the configuration of a specific workflow, and is
particularly helpful for debugging programmatically-generated configurations.

Example:

.. tab-set::

   .. tab-item:: Bazel
      :sync: bazel

      .. code-block:: console

         bazelisk run //pw_build/py:workflows_launcher -- describe format

   .. tab-item:: GN (bootstrapped environment)
      :sync: gn

      .. code-block:: console

         python -m pw_build.workflows.launcher describe format

This will print the configuration for the ``format`` tool:

.. code-block::

   ▒█████▄   █▓  ▄███▒  ▒█    ▒█ ░▓████▒ ░▓████▒ ▒▓████▄
    ▒█░  █░ ░█▒ ██▒ ▀█▒ ▒█░ █ ▒█  ▒█   ▀  ▒█   ▀  ▒█  ▀█▌
    ▒█▄▄▄█░ ░█▒ █▓░ ▄▄░ ▒█░ █ ▒█  ▒███    ▒███    ░█   █▌
    ▒█▀     ░█░ ▓█   █▓ ░█░ █ ▒█  ▒█   ▄  ▒█   ▄  ░█  ▄█▌
    ▒█      ░█░ ░▓███▀   ▒█▓▀▓█░ ░▓████▒ ░▓████▒ ▒▓████▀

   name: "format"
   description: "Find and fix code formatting issues"
   use_config: "bazel_default"
   target: "@pigweed//:format"
   analyzer_friendly_args: "--check"

``build``
---------
The ``build`` command launches the build for the requested build name.

Example:

.. tab-set::

   .. tab-item:: Bazel
      :sync: bazel

      .. code-block:: console

         bazelisk run //pw_build/py:workflows_launcher -- build all_host

   .. tab-item:: GN (bootstrapped environment)
      :sync: gn

      .. code-block:: console

         python -m pw_build.workflows.launcher build all_host

.. note::

   `b/425973227 <https://pwbug.dev/425973227>`__: This doesn't offer any
   configurability or additional argument handling today.

Generated Commands
==================
The workflows launcher generates commands from the ``tools`` and ``groups``
defined in the ``workflows.json`` configuration file.

Tools
-----
For each tool in the ``tools`` list, a command is created with the same name.
Running this command will execute the tool's specified command.

From the :ref:`example workflows.json <module-pw_build-workflows-launcher-example-configuration>`,
the following command is created:

.. tab-set::

   .. tab-item:: Bazel
      :sync: bazel

      .. code-block:: console

         bazelisk run //pw_build/py:workflows_launcher -- format

   .. tab-item:: GN (bootstrapped environment)
      :sync: gn

      .. code-block:: console

         python -m pw_build.workflows.launcher format

This will launch a ``bazel run`` invocation of Pigweed's code formatter tool.

Groups
------
For each group in the ``groups`` list, a command is created with the same name.
Running this command will execute all the workflows in the group in sequence.

From the :ref:`example workflows.json <module-pw_build-workflows-launcher-example-configuration>`,
the following command is created:

.. tab-set::

   .. tab-item:: Bazel
      :sync: bazel

      .. code-block:: console

         bazelisk run //pw_build/py:workflows_launcher -- presubmit

   .. tab-item:: GN (bootstrapped environment)
      :sync: gn

      .. code-block:: console

         python -m pw_build.workflows.launcher presubmit

This will launch a series of builds followed by code health check tooling as
enumerated by the group named ``presubmit``.

-------------
Configuration
-------------
The Workflows launcher is configured via
`ProtoJSON <https://protobuf.dev/programming-guides/json/>`__. The schema
lives in ``workflows.proto``:

.. dropdown:: Configuration schema

   .. literalinclude:: proto/workflows.proto
      :language: protobuf
      :start-after: syntax = "proto3";

The workflows launcher searches for a ``workflows.json`` file from the current
working directory, traversing through parent directories as needed. When a
``workflows.json`` file is found, it is loaded as a ``WorkflowSuite`` Protobuf
message. This configuration is then used to drive the rest of the launcher's
behavior.

.. _module-pw_build-workflows-launcher-example-configuration:

.. dropdown:: Example ``workflows.json``

   .. literalinclude:: py/workflows_launcher_test.py
      :language: json
      :start-after: _EXAMPLE_CONFIG = """
      :end-before: """

.. _module-pw_build-workflows-launcher-commands:

Build programming
=================
The definition of how build configurations become a series of actions is defined
by :ref:`docs-workflows-build-drivers`.

.. toctree::
   :hidden:
   :maxdepth: 1

   workflows_build_drivers
