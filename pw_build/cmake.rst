.. _module-pw_build-cmake:

CMake
=====
Pigweed's `CMake`_ support is provided primarily for projects that have an
existing CMake build and wish to integrate Pigweed without switching to a new
build system.

.. tip::
   To run upstream Pigweed's CMake build use the ``pw build`` command:

   .. code-block:: console

      pw build -r default_cmake

   This will install any required packages, generate cmake build files and invkoke ninja.

   .. code-block:: text

      19:36:58 INF [1/1] Starting ==> Recipe: default_cmake Targets: pw_run_tests.modules pw_apps pw_run_tests.pw_bluetooth Logfile: /out/build_default_cmake.txt
      19:36:58 INF [1/1] Run ==> pw --no-banner package install emboss
      19:36:59 INF [1/1] Run ==> pw --no-banner package install nanopb
      19:37:00 INF [1/1] Run ==> pw --no-banner package install boringssl
      19:37:10 INF [1/1] Run ==> cmake --fresh --debug-output -DCMAKE_MESSAGE_LOG_LEVEL=WARNING -S . -B ./out/cmake -G Ninja -DCMAKE_TOOLCHAIN_FILE=./pw_toolchain/host_clang/toolchain.cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=1 -Ddir_pw_third_party_nanopb=./environment/packages/nanopb -Dpw_third_party_nanopb_ADD_SUBDIRECTORY=ON -Ddir_pw_third_party_emboss=./environment/packages/emboss -Ddir_pw_third_party_boringssl=./environment/packages/boringssl -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
      19:37:10 INF [1/1] Run ==> ninja -C out/cmake pw_apps pw_run_tests.modules pw_run_tests.pw_bluetooth

   :ref:`module-pw_watch` works with ``pw build`` as well. You can run the
   following to automatically rebuild when files change.

   .. code-block:: console

      pw build -r default_cmake --watch

CMake functions
---------------
CMake convenience functions are defined in ``pw_build/pigweed.cmake``.

* ``pw_add_library_generic`` -- The base helper used to instantiate CMake
  libraries. This is meant for use in downstream projects as upstream Pigweed
  modules are expected to use ``pw_add_library``.
* ``pw_add_library`` -- Add an upstream Pigweed library.
* ``pw_add_facade_generic`` -- The base helper used to instantiate facade
  libraries. This is meant for use in downstream projects as upstream Pigweed
  modules are expected to use ``pw_add_facade``.
* ``pw_add_facade`` -- Declare an upstream Pigweed facade.
* ``pw_set_backend`` -- Set the backend library to use for a facade.
* ``pw_add_test_generic`` -- The base helper used to instantiate test targets.
  This is meant for use in downstrema projects as upstream Pigweed modules are
  expected to use ``pw_add_test``.
* ``pw_add_test`` -- Declare an upstream Pigweed test target.
* ``pw_add_test_group`` -- Declare a target to group and bundle test targets.
* ``pw_target_link_targets`` -- Helper wrapper around ``target_link_libraries``
  which only supports CMake targets and detects when the target does not exist.
  Note that generator expressions are not supported.
* ``pw_add_global_compile_options`` -- Applies compilation options to all
  targets in the build. This should only be used to add essential compilation
  options, such as those that affect the ABI. Use ``pw_add_library`` or
  ``target_compile_options`` to apply other compile options.
* ``pw_add_error_target`` -- Declares target which reports a message and causes
  a build failure only when compiled. This is useful when ``FATAL_ERROR``
  messages cannot be used to catch problems during the CMake configuration
  phase.
* ``pw_parse_arguments`` -- Helper to parse CMake function arguments.
* ``pw_rebase_paths`` -- Helper to update a set of file paths to be rooted on a
  new directory.

See ``pw_build/pigweed.cmake`` for the complete documentation of these
functions.

Special libraries that do not fit well with these functions are created with the
standard CMake functions, such as ``add_library`` and ``target_link_libraries``.

Facades and backends
--------------------
The CMake build uses CMake cache variables for configuring
:ref:`facades<docs-module-structure-facades>` and backends. Cache variables are
similar to GN's build args set with ``gn args``. Unlike GN, CMake does not
support multi-toolchain builds, so these variables have a single global value
per build directory.

The ``pw_add_module_facade`` function declares a cache variable named
``<module_name>_BACKEND`` for each facade. Cache variables can be awkward to
work with, since their values only change when they're assigned, but then
persist accross CMake invocations. These variables should be set in one of the
following ways:

* Prior to setting a backend, your application should include
  ``$ENV{PW_ROOT}/backends.cmake``. This file will setup all the backend targets
  such that any misspelling of a facade or backend will yield a warning.

  .. note::
    Zephyr developers do not need to do this, backends can be set automatically
    by enabling the appropriate Kconfig options.

* Call ``pw_set_backend`` to set backends appropriate for the target in the
  target's toolchain file. The toolchain file is provided to ``cmake`` with
  ``-DCMAKE_TOOLCHAIN_FILE=<toolchain file>``.
* Call ``pw_set_backend`` in the top-level ``CMakeLists.txt`` before other
  CMake code executes.
* Set the backend variable at the command line with the ``-D`` option.

  .. code-block:: sh

     cmake -B out/cmake_host -S "$PW_ROOT" -G Ninja \
         -DCMAKE_TOOLCHAIN_FILE=$PW_ROOT/pw_toolchain/host_clang/toolchain.cmake \
         -Dpw_log_BACKEND=pw_log_basic

* Temporarily override a backend by setting it interactively with ``ccmake`` or
  ``cmake-gui``.

If the backend is set to a build target that does not exist, there will be an
error message like the following:

.. code-block::

   CMake Error at pw_build/pigweed.cmake:257 (message):
     my_module.my_facade's INTERFACE dep "my_nonexistent_backend" is not
     a target.
   Call Stack (most recent call first):
     pw_build/pigweed.cmake:238:EVAL:1 (_pw_target_link_targets_deferred_check)
     CMakeLists.txt:DEFERRED


Toolchain setup
---------------
In CMake, the toolchain is configured by setting CMake variables, as described
in the `CMake documentation <https://cmake.org/cmake/help/latest/manual/cmake-toolchains.7.html>`_.
These variables are typically set in a toolchain CMake file passed to ``cmake``
with the ``-D`` option (``-DCMAKE_TOOLCHAIN_FILE=path/to/file.cmake``).
For Pigweed embedded builds, set ``CMAKE_SYSTEM_NAME`` to the empty string
(``""``).

Toolchains may set the ``pw_build_WARNINGS`` variable to a list of ``INTERFACE``
libraries with compilation options for Pigweed's upstream libraries. This
defaults to a strict set of warnings. Projects may need to use less strict
compilation warnings to compile backends exposed to Pigweed code (such as
``pw_log``) that cannot compile with Pigweed's flags. If desired, Projects can
access these warnings by depending on ``pw_build.warnings``.

Third party libraries
---------------------
The CMake build includes third-party libraries similarly to the GN build. A
``dir_pw_third_party_<library>`` cache variable is defined for each third-party
dependency. The variable must be set to the absolute path of the library in
order to use it. If the variable is empty
(``if("${dir_pw_third_party_<library>}" STREQUAL "")``), the dependency is not
available.

Third-party dependencies are not automatically added to the build. They can be
manually added with ``add_subdirectory`` or by setting the
``pw_third_party_<library>_ADD_SUBDIRECTORY`` option to ``ON``.

Third party variables are set like any other cache global variable in CMake. It
is recommended to set these in one of the following ways:

* Set with the CMake ``set`` function in the toolchain file or a
  ``CMakeLists.txt`` before other CMake code executes.

  .. code-block:: cmake

     set(dir_pw_third_party_nanopb ${CMAKE_CURRENT_SOURCE_DIR}/external/nanopb CACHE PATH "" FORCE)

* Set the variable at the command line with the ``-D`` option.

  .. code-block:: sh

     cmake -B out/cmake_host -S "$PW_ROOT" -G Ninja \
         -DCMAKE_TOOLCHAIN_FILE=$PW_ROOT/pw_toolchain/host_clang/toolchain.cmake \
         -Ddir_pw_third_party_nanopb=/path/to/nanopb

* Set the variable interactively with ``ccmake`` or ``cmake-gui``.

.. _module-pw_build-existing-cmake-project:

Use Pigweed from an existing CMake project
------------------------------------------
To use Pigweed libraries form a CMake-based project, simply include the Pigweed
repository from a ``CMakeLists.txt``.

.. code-block:: cmake

   add_subdirectory(path/to/pigweed pigweed)

All module libraries will be available as ``module_name`` or
``module_name.sublibrary``.

If desired, modules can be included individually.

.. code-block:: cmake

   add_subdirectory(path/to/pigweed/pw_some_module pw_some_module)
   add_subdirectory(path/to/pigweed/pw_another_module pw_another_module)

.. seealso::
   Additional Pigweed CMake function references:
   - :bdg-ref-primary-line:`module-pw_fuzzer-guides-using_fuzztest-toolchain`
   - :bdg-ref-primary-line:`module-pw_protobuf_compiler-cmake`
   - :bdg-ref-primary-line:`module-pw_unit_test-cmake`
