.. _chapter-build-system:

============
Build system
============

Pigweed's primary build system is `GN`_, which is used for all upstream
development. Some other common build systems are supported for integration into
existing project, which are described in :ref:`chapter-pw-build`. We recommend
using GN where possible.

.. _GN: https://gn.googlesource.com/gn/

This document describes Pigweed's upstream build structure.

.. note::
  A quick note on terminology: the word "target" is overloaded within GN (and
  Pigweed)---it can refer to either a GN build target, such as a ``source_set``
  or ``executable``, or to an output platform (e.g. a specific board, device, or
  system).

  To avoid confusing the two, we refer to the former as "GN targets" and the
  latter as "Pigweed targets".

.gn
===
The entrypoint to the GN build is the ``.gn`` file, which defines the project's
root directory (henceforth ``//``). In Pigweed, its only purpose is to point GN
to the location of the BUILDCONFIG file.

BUILDCONFIG.gn
==============
``//BUILDCONFIG.gn`` configures the GN build by defining global configuration
options. Most of Pigweed's configuration is left to individual build targets,
so the BUILDCONFIG file is relatively empty. It sets Pigweed's default
toolchain, which GN requires before evaluating any BUILD files.

//BUILD.gn
==========
The root ``BUILD.gn`` file defines all of the libraries, images, tests, and
binaries built within Pigweed. These are split across a few logical groups,
which are described below. In order to build a GN target, it *must* be listed in
one of the groups in this file.

``//BUILD.gn`` is also responsible for enumerating each of the Pigweed targets
that Pigweed supports. These targets are defined as toolchains providing their
custom configuration options. ``/BUILD.gn`` instantiates a version of its GN
target groups for each of these toolchains.

.. warning::
  Pigweed's default toolchain is never used, so it is set to a dummy toolchain
  which doesn't define any tools. ``//BUILD.gn`` contains conditions which check
  that the current toolchain is not the default before declaring any GN target
  dependencies to prevent the default toolchain from evaluating any other BUILD
  files. All GN targets added to the build must be placed in one of these
  conditional scopes.

Groups
------

apps
^^^^
This group defines the application images built in Pigweed. It lists all of the
common images built across all Pigweed targets, such as modules' example
executables. Each Pigweed target can additionally provide its own specific
images through the ``pw_TARGET_APPLICATIONS`` build arg, which is included by
this group.

host_tools
^^^^^^^^^^
This group defines host-side tooling binaries built for Pigweed.

pw_modules
^^^^^^^^^^
This group lists the main libraries for all of Pigweed's modules.

pw_module_tests
^^^^^^^^^^^^^^^
All modules' unit tests are collected here, so that they can all be run at once.

pigweed_default
^^^^^^^^^^^^^^^
This group defines everything built in a Pigweed build invocation by collecting
the above groups and conditionally adding them, based on the Pigweed target
configuration. Generally, new dependencies should not be added here; instead use
one of the groups listed above.

The ``pigweed_default`` group is instantiated for each of the Pigweed target
toolchains.

Pigweed target instantiations
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
These groups wrap ``pigweed_default`` with a specific target toolchain. They are
named after the Pigweed target, e.g. ``host_clang``, ``stm32f429i``, etc.

Pigweed targets
===============
Each Pigweed target is defined as a GN toolchain which provides its own build
tool and output binary configs, and custom overrides for Pigweed's build
configuration arguments. For more information on Pigweed's target system, as
well as each of the supported targets, refer to :ref:`chapter-targets`.

Usage examples
==============

Building a custom executable
----------------------------

1. Define your executable GN target using the ``pw_executable`` template.

   .. code::

     # //foo/BUILD.gn
     pw_executable("foo") {
       sources = [ "main.cc" ]
       deps = [ ":libfoo" ]
     }

2. In the root ``BUILD.gn`` file, add the executable's GN target to the ``apps``
   group.

   .. code::

     # //BUILD.gn
     group("apps") {
       deps = [
         # ...
         "//foo",  # Shorthand for //foo:foo
       ]
     }

3. Run the ninja build to compile your executable. The apps group is built by
   default, so there's no need to provide a target. The executable will be
   compiled for every supported Pigweed target.

   .. code::

     ninja -C out

   Alternatively, build your executable by itself by specifying its path to
   Ninja. When building a GN target manually, the Pigweed target for which it
   is built must be specified on the Ninja command line.

   For example, to build for the Pigweed target ``host_gcc_debug``:

   .. code::

     ninja -C out host_gcc_debug/obj/foo/bin/foo

   .. note::

     The path passed to Ninja is a filesystem path within the ``out`` directory,
     rather than a GN path. This path can be found by running ``gn outputs``.

4. Retrieve your compiled binary from the out directory. It is located at the
   path

   .. code::

     out/<pw_target>/obj/<gn_path>/{bin,test}/<executable>

   where ``pw_target`` is the Pigweed target for which the binary was built,
   ``gn_path`` is the GN path to the BUILD.gn file defining the executable,
   and ``executable`` is the executable's GN target name (potentially with an
   extension). Note that the executable is located within a ``bin`` subdirectory
   in the module (or ``test`` for unit tests defined with ``pw_test``).

   For example, the ``foo`` executable defined above and compiled for the
   Pigweed target stm32f429i_disc1_debug is found at:

   .. code::

     out/stm32f429i_disc1_debug/obj/foo/bin/foo
