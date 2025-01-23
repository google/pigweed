.. _docs-bazel-integration:

===================================================
Using a Pigweed module in an existing Bazel project
===================================================
This guide explains how to start using a Pigweed module in your existing
Bazel-based C or C++ project. We'll assume you're familiar with the build
system at the level of the `Bazel tutorial <https://bazel.build/start/cpp>`__.

------------------------
Supported Bazel versions
------------------------
Pigweed uses Bazel 8 features like platform-based flags, and so not all of
Pigweed works with Bazel 7. We strongly recommend mirroring Pigweed's current
Bazel version pin:

.. literalinclude:: ../../.bazelversion
   :language: text

-------------------------------------
Add Pigweed as an external dependency
-------------------------------------
Pigweed can be used in both `bzlmod
<https://bazel.build/external/overview#bzlmod>`__ and `WORKSPACE
<https://bazel.build/external/overview#workspace-system>`__ based projects.

We recommend using bzlmod (it's the future!), but note that Pigweed's FuzzTest
and GoogleTest integration cannot yet be used in bzlmod-based projects
(https://pwbug.dev/365103864).

.. tab-set::

   .. tab-item:: bzlmod

      Use a ``git_override``:

      .. code-block:: python

         bazel_dep(name = "pigweed")
         bazel_dep(name = "pw_toolchain")

         git_override(
             module_name = "pigweed",
             commit = "c00e9e430addee0c8add16c32eb6d8ab94189b9e",
             remote = "https://pigweed.googlesource.com/pigweed/pigweed.git",
         )

      You can find the latest tip-of-tree commit in the **History** tab in
      `CodeSearch <https://cs.opensource.google/pigweed/pigweed>`__.

      If you manage your dependencies as submodules, you can add Pigweed as a
      submodule, too, and then add it to your ``MODULE.bazel`` as a
      `local_path_override
      <https://bazel.build/rules/lib/globals/module#local_path_override>`__:

      .. code-block:: python

         local_path_override(
             module_name = "pigweed",
             path = "third_party/pigweed",
         )

      Pigweed is not yet published to the `Bazel Central Registry
      <https://registry.bazel.build/>`__. If this is a pain point for you,
      please reach out to us on `chat <https://discord.gg/M9NSeTA>`__.

   .. tab-item:: WORKSPACE

      Add Pigweed as a `git_repository
      <https://bazel.build/rules/lib/repo/git#git_repository>`__ in your
      ``WORKSPACE``:

      .. code-block:: python

         git_repository(
             name = "pigweed",
             commit = "c00e9e430addee0c8add16c32eb6d8ab94189b9e",
             remote = "https://pigweed.googlesource.com/pigweed/pigweed.git",
         )

      You can find the latest tip-of-tree commit in the **History** tab in
      `CodeSearch <https://cs.opensource.google/pigweed/pigweed>`__.

      If you manage your dependencies as submodules, you can add Pigweed as a
      submodule, too, and then add it to your ``WORKSPACE`` as a
      `local_repository
      <https://bazel.build/reference/be/workspace#local_repository>`__:

      .. code-block:: python

         local_repository(
             name = "pigweed",
             path = "third_party/pigweed",
         )

---------------------------
Use Pigweed in your project
---------------------------
Let's say you want to use ``pw::Vector`` from :ref:`module-pw_containers`, our
embedded-friendly replacement for ``std::vector``.

#. Include the header you want in your code:

   .. code-block:: cpp

      #include "pw_containers/vector.h"

#. Look at the module's `build file
   <https://cs.opensource.google/pigweed/pigweed/+/main:pw_containers/BUILD.bazel>`__
   to figure out which build target you need to provide the header and
   implementation. For ``pw_containers/vector.h``, it's
   ``//pw_containers:vector``.

#. Add this target to the ``deps`` of your
   `cc_library <https://bazel.build/reference/be/c-cpp#cc_library>`__ or
   `cc_binary <https://bazel.build/reference/be/c-cpp#cc_binary>`__:

   .. code-block:: python

      cc_library(
          name = "my_library",
          srcs  = ["my_library.cc"],
          hdrs = ["my_library.h"],
          deps = [
              "@pigweed//pw_containers:vector",  # <-- The new dependency
          ],
      )

#. Add a dependency on ``@pigweed//pw_build:default_link_extra_lib`` to your
   final *binary* target. See :ref:`docs-build_system-bazel_link-extra-lib`
   for a discussion of why this is necessary, and what the alternatives are.

   .. code-block:: python

      cc_binary(
          name = "my_binary",
          srcs  = ["my_binary.cc"],
          deps = [
              ":my_library",
              "@pigweed//pw_build:default_link_extra_lib",  # <-- The new dependency
          ],
      )

----------------------------
Set the required Bazel flags
----------------------------
Pigweed projects need to set certain flags in their ``.bazelrc``. These
generally pre-adopt Bazel features that will become default in the future and
improve cache performance, disambiguate Python imports, etc. These flags are
listed below.  Unfortunately there's no way to automatically import them, see
:bug:`353750350`.

.. literalinclude:: ../../pw_build/pigweed.bazelrc

--------------------------------------------
Configure backends for facades you depend on
--------------------------------------------
Pigweed makes extensive use of :ref:`docs-facades`, and any module you choose
to use will likely have a transitive dependency on some facade (typically
:ref:`module-pw_assert` or :ref:`module-pw_log`). Continuing with our example,
``pw::Vector`` depends on :ref:`module-pw_assert`.

In Bazel, facades already have a default backend (implementation) that works
for host builds (builds targeting your local development machine). But to build
a binary for your embedded target, you'll need to select a suitable backend
yourself.

Fortunately, the default backend for :ref:`module-pw_assert` is
:ref:`module-pw_assert_basic`, which is a suitable place to start for most
embedded targets, too. But it depends on :ref:`module-pw_sys_io`, another
facade for which you *will* have to choose a backend yourself.

The simplest way to do so is to set the corresponding `label flag
<https://bazel.build/extending/config#label-typed-build-settings>`__ when
invoking Bazel. For example, to use the
:ref:`module-pw_sys_io_baremetal_stm32f429` backend for :ref:`module-pw_sys_io`
provided in upstream Pigweed:

.. code-block:: console

   $ bazel build \
       --@pigweed//targets/pw_sys_io_backend=@pigweed//pw_sys_io_baremetal_stm32f429 \
       //path/to/your:target

You can also define backends within your own project. (If Pigweed doesn't
include a :ref:`module-pw_sys_io` backend suitable for your embedded platform,
that's what you should do now.) See
:ref:`docs-build_system-bazel_configuration` for a tutorial that dives deeper
into facade configuration with Bazel.
