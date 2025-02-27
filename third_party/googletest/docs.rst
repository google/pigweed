.. _module-pw_third_party_googletest:

==========
GoogleTest
==========
The ``$dir_pw_third_party/googletest/`` module provides various helpers to
optionally use full upstream `GoogleTest/GoogleMock`__ with
:ref:`module-pw_unit_test`.

.. __: https://github.com/google/googletest

.. _module-pw_third_party_googletest-using_upstream:

----------------------------------------
Using upstream GoogleTest and GoogleMock
----------------------------------------
If you want to use the full upstream GoogleTest/GoogleMock, you must do the
following:

Add GoogleTest to your workspace with the following command.

.. code-block:: console

   $ git submodule add https://github.com/google/googletest third_party/googletest

Configure ``pw_unit_test`` to use upstream GoogleTest/GoogleMock.

.. tab-set::

   .. tab-item:: GN

      * Set one of the following GN variables:

        * Set ``pw_third_party_googletest_ALIAS`` to point to
          a GN target label that provides a static library or
          source set combining GoogleTest/GoogleMock.

          This is required on platforms such as Fuchsia where
          extra dependencies are required for the library to compile
          and run properly.

        * Or set ``dir_pw_third_party_googletest`` to point
          to the location of the googletest submodule instead.
          This will trigger the generation of a pw_source_set()
          target wrapping the GoogleTest sources.

      * Set the GN var ``pw_unit_test_MAIN`` to
        ``dir_pigweed + "/third_party/googletest:gmock_main"``.
      * Set the GN var ``pw_unit_test_BACKEND`` to
        ``"//pw_unit_test:googletest"``.

      Pigweed unit tests that do not work with upstream GoogleTest can be
      disabled by setting ``enable_if`` to
      ``pw_unit_test_BACKEND == "$dir_pw_unit_test:light"``.

   .. tab-item:: CMake

      * Set the ``dir_pw_third_party_googletest`` to the location of the
        GoogleTest source.
      * Set the var
        ``pw_unit_test_MAIN`` to ``pw_third_party.googletest.gmock_main``.
      * Set the var ``pw_unit_test_BACKEND`` to
        ``pw_unit_test.googletest``.

   .. tab-item:: Bazel

      Set the following :ref:`label flags <docs-build_system-bazel_flags>`,
      either in your
      :ref:`target config <docs-build_system-bazel_configuration>` or on
      the command line:

      * ``//pw_unit_test:backend`` to
        ``@pigweed//pw_unit_test:googletest``.
      * ``//pw_unit_test:main`` to ``@com_google_googletest//:gtest_main``.

      For example:

      .. code-block:: console

         $ bazel test //... \
         > --@pigweed//pw_unit_test:backend=@pigweed//pw_unit_test:googletest \
         > --@pigweed//pw_unit_test:main=@com_google_googletest//:gtest_main

.. note::

  Not all unit tests build properly with upstream GoogleTest yet. This is a
  work in progress.
