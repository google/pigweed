.. _module-pw_ide-guide-vscode-code-intelligence:

=================
Code intelligence
=================
.. pigweed-module-subpage::
   :name: pw_ide

The Pigweed Visual Studio Code extension bridges the build system and ``clangd``
to provide the smoothest possible C++ embedded development system. For
background on the tools and approaches Pigweed uses, check out the
:ref:`design docs<module-pw_ide-design-cpp>`. This doc is a user guide to the
way those concepts are applied in Visual Studio Code.

----------------
Target discovery
----------------
Pigweed IDE will discover build targets for Bazel, GN and CMake builds
automatically, as well as any other targets described by compilation databases
produced by other means.

In general, the process of discovering and processing build targets is triggered
by running the :ref:`Refresh Compile Commands<module-pw_ide-guide-vscode-commands-refresh-compile-commands>`
command. See below for build system-specific details.

.. tab-set::

   .. tab-item:: Bazel

      Pigweed IDE uses Bazel queries to discover your project's build targets
      and generate ``clangd``-compatible compilation databases for each of them.
      By default, a file watcher monitors Bazel build files and automatically
      updates compilation databases in the background. So in most cases, no
      manual action needs to be taken.

      .. note::

         Previous versions of the Pigweed extension required manually declaring
         Bazel targets to use for code intelligence in a
         ``refresh_compile_commands`` invocation in the top level
         ``BUILD.bazel`` file. This is no longer necessary, and the
         ``refresh_compile_commands`` Bazel function is no longer present.

   .. tab-item:: GN

      GN :ref:`can be configured<module-pw_ide-design-cpp-gn>` to generate a
      compilation database whenever ``gn gen`` is run. Pigweed IDE will find
      that file when :ref:`Refresh Compile Commands<module-pw_ide-guide-vscode-commands-refresh-compile-commands>`
      is run and make those targets available for code analysis.

      Right now, this is a manual process; if the compilation databases need to
      be updated, you have to run ``gn gen`` and then
      :ref:`Refresh Compile Commands<module-pw_ide-guide-vscode-commands-refresh-compile-commands>`.

   .. tab-item:: CMake

      CMake :ref:`can be configured<module-pw_ide-design-cpp-cmake>` to generate
      compilation databases during its build. Pigweed IDE will find those files
      when :ref:`Refresh Compile Commands<module-pw_ide-guide-vscode-commands-refresh-compile-commands>`
      is run and make those targets available for code analysis.

      If you have a CMake build watcher running, then the compilation databases
      will update automatically in response to your code changes without the
      need to run :ref:`Refresh Compile Commands<module-pw_ide-guide-vscode-commands-refresh-compile-commands>`.
      The only time you would need to manually run that command is if build
      targets were added or removed from the build.

----------------------------------------------
Selecting a target group for code intelligence
----------------------------------------------
The currently-selected code intelligence target group is displayed in the
Visual Studio Code status bar:

.. figure:: https://storage.googleapis.com/pigweed-media/vsc-status-bar-target.png
   :alt: Visual Studio Code screenshot showing the target status bar item

You can click the status bar item to select a new target group from a dropdown
list at the top of the screen.

.. figure:: https://storage.googleapis.com/pigweed-media/vsc-dropdown-select-target.png
   :alt: Visual Studio Code screenshot showing the target selector

------------------------------------
Keeping code intelligence data fresh
------------------------------------
As you work on your project, the build graph will change, new compilation
databases will need to be built, and ``clangd`` will need to be re-configured
to provide accurate code intelligence.

The Pigweed extension handles this for you automatically for Bazel builds.
Whenever you make a change that could alter the build graph, a background
process is launched to regenerate fresh compile commands. You'll see the status
bar icon change to look like this while the refresh process is running, and
during that time, you can click on the status bar item to open the output window
and monitor progress.

.. figure:: https://storage.googleapis.com/pigweed-media/vsc-status-bar-refreshing.png
   :alt: Visual Studio Code screenshot showing the target status bar item
         refreshing

When the refresh process is complete, the status bar item will look like this:

.. figure:: https://storage.googleapis.com/pigweed-media/vsc-status-bar-finished.png
   :alt: Visual Studio Code screenshot showing the target status bar item in the
         finished state

.. tip::

   In most cases, you don't have to wait around for the refresh process to
   finish to keep working. You can still switch between targets, and code
   intelligence still works (though it may be a little stale for any files
   affected by the change that triggered the refresh).

No automatic process is perfect, and if an error occurs during the refresh
process, that will be indicated with this icon in the status bar:

.. figure:: https://storage.googleapis.com/pigweed-media/vsc-status-bar-fault.png
   :alt: Visual Studio Code screenshot showing the target status bar item in an
         error state

You can click the status bar item to trigger a retry, or you can
:ref:`open the output panel<module-pw_ide-guide-vscode-commands-open-output-panel>`
to get more details about the error.

.. note::

   * You can always trigger a manual compilation database refresh by running
     :ref:`Pigweed: Refresh Compile Commands<module-pw_ide-guide-vscode-commands-refresh-compile-commands>`.

   * If you don't want to use the automatic refresh process, you can
     :ref:`disable it<module-pw_ide-guide-vscode-settings-disable-compile-commands-file-watcher>`.

----------------------------------
Inactive and orphaned source files
----------------------------------
As discussed in the :ref:`design docs<module-pw_ide-design-cpp>`, some source
files will be compiled in several different targets, possibly with different
compiler and linker options. Likewise, some files may not be compiled as part
of a particular selected target, perhaps because the file is not relevant to
the target (for example, hardware support implementations for a host simulator
target). Finally, some source files may not be compiled by *any* defined target
group, either because those files have not yet been brought into the build
graph, or because none of the defined target groups contain a target that builds
that source file.

We need to care about this because ``clangd`` tries to be helpful in a way that
is very counterproductive in Pigweed projects: If it encounters a file but
cannot find a corresponding compile command in the compilation database, it
will *infer* a compile command for that file from other similar files that *are*
in the compilation database.

Since the compilation databases that Pigweed generates are specifically
engineered to only include compile commands pertinent to the selected target
group, the *inferred* code intelligence ``clangd`` provides for other files
is invalid. So the Pigweed extension provides mechanisms to exclude those files
from ``clangd`` and prevent misleading code intelligence information.

.. glossary::

   Active source file
     A source file that is built in the currently-selected target group

   Inactive source file
     A source file that is *not* built in the currently-selected target group

   Orphaned source file
     A source file that is not built by *any* defined target groups

Disabling ``clangd`` for inactive and orphaned files
====================================================
By default, Pigweed will disable ``clangd`` for inactive and orphaned files to
prevent inaccurate and distracting information from appearing in the editor.
You can see that ``clangd`` is disabled for those files when you see this icon
in the status bar:

.. figure:: https://storage.googleapis.com/pigweed-media/vsc-inactive-clangd-disabled.png
   :alt: Visual Studio Code screenshot showing code intelligence disabled for
         inactive files

You can click the icon to *enable* ``clangd`` for all files, regardless of
whether they are in the current target's build graph or not. That state will be
indicated with this icon:

.. figure:: https://storage.googleapis.com/pigweed-media/vsc-inactive-clangd-enabled.png
   :alt: Visual Studio Code screenshot showing code intelligence enabled for
         inactive files

You can click it again to toggle it back to the default state.

File status indicators
======================
The Visual Studio Code explorer (file tree) displays an indicator next to
inactive and orphaned files to help you understand which files will not have
code intelligence. These indicators will change as you change targets and as
you change the build graph.

.. figure:: https://storage.googleapis.com/pigweed-media/vsc-inactive-file-indicators.png
   :alt: Visual Studio Code screenshot file indicators for inactive and
         orphaned files
   :figwidth: 250

Inactive files are indicated like this:

.. figure:: https://storage.googleapis.com/pigweed-media/vsc-inactive-file-indicators-inactive.png
   :alt: Visual Studio Code screenshot file indicators for inactive files
   :figwidth: 250

Orphaned files are indicated like this:

.. figure:: https://storage.googleapis.com/pigweed-media/vsc-inactive-file-indicators-orphaned.png
   :alt: Visual Studio Code screenshot file indicators for orphaned files
   :figwidth: 250

Note that the colors may vary depending on your Visual Studio Code theme.

.. tip::

   By default, file status indicators will be shown even if ``clangd`` is
   enabled for all files. You can change this behavior with
   :ref:`this setting<module-pw_ide-guide-vscode-settings-hide-inactive-file-indicators>`.
