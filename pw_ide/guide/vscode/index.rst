.. _module-pw_ide-guide-vscode:

==================
Visual Studio Code
==================
.. pigweed-module-subpage::
   :name: pw_ide

.. toctree::
   :maxdepth: 1
   :hidden:

   code_intelligence
   extension_enforcement
   troubleshooting
   legacy
   development

Pigweed provides rich and robust support for development in `Visual Studio Code <https://code.visualstudio.com/>`_,
including:

* High-quality C/C++ code intelligence for embedded systems projects using `clangd <https://clangd.llvm.org/>`_
  integrated directly with your project's Bazel build graph

* Bundled core Bazel tools, letting you get started immediately without the need
  to install global system dependencies

* Interactive browsing, building, and running Bazel targets

.. note::

   Currently, this document only applies to :ref:`Bazel projects<module-pw_ide-design-projects-bazel>`.
   We're working on adding support for :ref:`bootstrap projects<module-pw_ide-design-projects-bootstrap>`.
   In the meantime, bootstrap projects can use the :ref:`command-line interface<module-pw_ide-guide-cli>`
   with the :ref:`legacy support for Visual Studio Code<module-pw_ide-guide-vscode-legacy>`.

---------------
Getting started
---------------
All you need to do is install the `Pigweed extension <http://localhost/404>`_ from the extension
marketplace. If you start your project from one of Pigweed's quickstart or
showcase example projects, you will be prompted to install the extension as soon
as you open the project.

Once installed, the Pigweed extension will do a few things for you automatically
when you open :ref:`Pigweed projects<module-pw_ide-design-projects>`:

* The Bazel extension will discover all of the targets in your project

* The Pigweed extension will generate `compilation databases <https://clangd.llvm.org/design/compile-commands>`_
  for the :ref:`target groups<module-pw_ide-design-cpp-target-groups>`
  in your project.

You can now select a target group from the status bar item at the bottom
of your window or by running the ``Pigweed: Select Code Analysis Target``
command.

Once you select a target group, the ``clangd`` extension will be automatically
configured to use the ``clang`` toolchain in the Bazel environment and the
compilation database associated with the selected target group.

What this gives you
===================
Here's a non-exhaustive list of cool features you can now enjoy:

* Code navigation, including routing through facades to the correct backend

* Code completion, including correct class members and function signatures

* Tooltips with docs, inferred types for ``auto``, inferred values for ``constexpr``,
  data type sizes, etc.

* Compiler errors and warnings as you write your code

* Code formatting via the standard ``Format ...`` commands, including
  Starlark files

* Linting and debugging for Starlark files

* A tree view of all Bazel targets, allowing you to build or run them directly

-----------------
Code intelligence
-----------------
Learn more about using and configuring code intelligence :ref:`here<module-pw_ide-guide-vscode-code-intelligence>`.

----------------
Project settings
----------------
Pigweed manipulates some editor and ``clangd`` settings to support features like
the ones described above. For example, when you select a code analysis target,
Pigweed sets the ``clangd`` extensions settings in ``.vscode/settings.json`` to
configure ``clangd`` to use the selected target. Likewise, when using the
:ref:`feature to disable code intelligence for source files not in the target's build graph<module-pw_ide-guide-vscode-settings-disable-inactive-file-code-intelligence>`,
Pigweed will manipulate the ``.clangd`` `configuration file <https://clangd.llvm.org/config#files>`_.

These files shouldn't be committed to the project repository, because they
contain state that is specific to what an individual developer is working on.
Nonetheless, most projects will want to commit certain shared settings to their
repository to help create a more consistent development environment.

Pigweed provides a mechanism to achieve that through additional settings files.

Visual Studio Code settings
===========================
The ``.vscode/settings.json`` file should be excluded from source control.
Instead, add your shared project settings to ``.vscode/settings.shared.json``
and commit *that* file to source control.

The Pigweed extension watches the shared settings file and automatically applies
those settings to you local settings file. So shared project settings can be
committed to ``.vscode/settings.shared.json``, and your current editor state, as
well as your personal configuration preferences, can be stored in
``.vscode/settings.json``.

The automatic sync process will respect any settings you have in your personal
settings file. In other words, if a conflicting setting appears in the shared
settings file, the automatic sync will not override your personal setting.

At some point, you may wish to *fully* synchronize with the shared settings,
overriding any conflicting settings you may already have in your personal
settings file. You can accomplish that by running the
:ref:`settings sync command<module-pw_ide-guide-vscode-commands-sync-settings>`.

``clangd`` settings
===================
Additional configuration for ``clangd`` can be stored in ``.clangd.shared``,
following the `YAML configuration format <https://clangd.llvm.org/config>`_.
The Pigweed extension watches this file and automatically applies its settings
to a ``.clangd`` file that *should not* be committed to source control. That
file will *also* be used to configure ``clangd`` in ways that are specific to
your selected analysis target and the state of your code tree.

--------
Commands
--------
Access commands by opening the command palette :kbd:`Ctrl+Shift+P`
(:kbd:`Cmd+Shift+P` on Mac).

.. describe:: Pigweed: Check Extensions

   The Pigweed extension lets development teams maintain a consistent
   development environment for all members of the team by ensuring that the
   recommendations in ``extensions.json`` are enforced. Learn more at
   :ref:`extension enforcement<module-pw_ide-guide-vscode-extension-enforcement>`.

.. describe:: Pigweed: File Bug

   Found a problem in the Pigweed Visual Studio Code extension, other Pigweed
   tools, or Pigweed itself? Add a bug to our bug tracker to help us fix it.

.. _module-pw_ide-guide-vscode-commands-sync-settings:

.. describe:: Pigweed: Sync Settings

   Pigweed automatically syncronizes shared Visual Studio Code settings from
   ``.vscode/settings.shared.json`` to ``.vscode/settings.json``, but in the
   case of conflicts, the automatic process will preserve the value in
   ``.vscode/settings.json``. If you want to do a full sync of the shared
   settings to your personal settings, including overriding conflicting values,
   run this command.

.. _module-pw_ide-guide-vscode-commands-open-output-panel:

.. describe:: Pigweed: Open Output Panel

   Opens the Pigweed output panel, which contains diagnostic output generated by
   the Pigweed extension. This is a good first place to look if things go wrong.

.. _module-pw_ide-guide-vscode-commands-refresh-compile-commands:

.. describe:: Pigweed: Refresh Compile Commands

   Manually trigger a refresh of the compilation databases used for C/C++ code
   intelligence. Normally, the databases are refreshed automatically when build
   files are changed, but if you have
   :ref:`automatic refreshing disabled<module-pw_ide-guide-vscode-settings-disable-compile-commands-file-watcher>`
   or need to refresh outside of the automatic cycle, this command will refresh
   manually.

.. describe:: Pigweed: Refresh Compile Commands and Set Code Analysis Target

   This is the same as the :ref:`Pigweed: Refresh Compile Commands<module-pw_ide-guide-vscode-commands-refresh-compile-commands>`,
   except that it also triggers :ref:`Pigweed: Select Code Analysis Target<module-pw_ide-guide-vscode-commands-select-target>`
   after the refresh is complete.

.. _module-pw_ide-guide-vscode-commands-select-target:

.. describe:: Pigweed: Select Code Analysis Target

   Select the target group that ``clangd`` should use for code analysis.

.. describe:: Pigweed: Disable Inactive File Code Intelligence

   See :ref:`the associated setting<module-pw_ide-guide-vscode-settings-disable-inactive-file-code-intelligence>`.

.. describe:: Pigweed: Enable Inactive File Code Intelligence

   See :ref:`the associated setting<module-pw_ide-guide-vscode-settings-disable-inactive-file-code-intelligence>`.

.. describe:: Pigweed: Set Bazel Recommended Settings

   Configure Visual Studio Code to use Pigweed's recommended Bazel settings.
   Note that these settings are only applied to the *project* settings, so they
   don't affect any other project's settings, or your user settings.

   * Sets the Buildifier path to the version bundled with the Pigweed extension, enabling Starlark code intelligence

   * Enables Bazel CodeLens support, allowing you to build and run targets directly from Bazel files

.. describe:: Pigweed: Set Bazelisk Path

   Pigweed recommends using `Bazelisk <https://github.com/bazelbuild/bazelisk>`_
   instead of plain Bazel to ensure that the right version of Bazel is used.
   This command allows you to set the path to Bazelisk, selecting from versions
   installed on your system or the version bundled with the Pigweed extension.

.. describe:: Pigweed: Activate Bazelisk in Terminal

   This will change the `$PATH` of your active integrated terminal to include
   the path to Bazelisk configured in your editor settings. This allows you to
   run Bazel actions via Visual Studio Code commands or via `bazelisk ...`
   invocations in the integrated terminal, while working in the same Bazel
   environment.

---------------------
Configuration options
---------------------
.. py:data:: pigweed.codeAnalysisTarget
   :type: string

   The build target to use for editor code intelligence

.. warning::

   You should only set this value by running the :ref:`Pigweed\: Select Code Analysis Target<module-pw_ide-guide-vscode-commands-select-target>`.
   The command has other configuration side-effects that won't be triggered if
   you manually set the value in ``settings.json``.

.. py:data:: pigweed.disableBazelSettingsRecommendation
   :type: boolean
   :value: false

   Disable reminders to use Pigweed's Bazel settings recommendations

.. py:data:: pigweed.disableBazeliskCheck
   :type: boolean
   :value: false

   Disable the recommendation to use Bazelisk

.. _module-pw_ide-guide-vscode-settings-disable-compile-commands-file-watcher:

.. py:data:: pigweed.disableCompileCommandsFileWatcher
   :type: boolean
   :value: false

   Disable automatically refreshing compile commands

.. _module-pw_ide-guide-vscode-settings-disable-inactive-file-code-intelligence:

.. py:data:: pigweed.disableInactiveFileCodeIntelligence
   :type: boolean
   :value: true

   When you select a target for code analysis, some source files in the project
   may not appear in the compilation database for that target because they are
   not part of the build graph for the target. By default, ``clangd`` will
   attempt to provide code intelligence for those files anyway by inferring
   compile commands from similar files in the build graph, but this code
   intelligence is incorrect and meaningless, as the file won't actually be
   compiled for that target.

   Enabling this option will configure ``clangd`` to suppress all diagnostics
   for any source files that are *not* part of the build graph for the currently
   selected target.

.. _module-pw_ide-guide-vscode-settings-enforce-extension-recommendations:

.. py:data:: pigweed.enforceExtensionRecommendations
   :type: boolean
   :value: false

   Require installing and disabling extensions recommended in ``extensions.json``

.. _module-pw_ide-guide-vscode-settings-hide-inactive-file-indicators:

.. py:data:: pigweed.hideInactiveFileIndicators
   :type: boolean
   :value: false

   When code intelligence is enabled for all files, hide indicators for inactive
   and orphaned files. Note that changing this setting requires you to reload
   Visual Studio Code to take effect.

.. py:data:: pigweed.preserveBazelPath
   :type: boolean
   :value: false

   When enabled, this extension won't override the specified Bazel path under
   any circumstances.

.. _module-pw_ide-guide-vscode-settings-project-root:

.. py:data:: pigweed.projectRoot
   :type: string

   The root of the Pigweed project source directory

.. _module-pw_ide-guide-vscode-settings-project-type:

.. py:data:: pigweed.projectType
   :type: bootstrap or bazel

   The type of Pigweed project, either bootstrap or Bazel

.. _module-pw_ide-guide-vscode-settings-refresh-compile-commands-target:

.. py:data:: pigweed.refreshCompileCommandsTarget
   :type: string
   :value: //:refresh_compile_commands

   The Bazel target to run to refresh compile commands
