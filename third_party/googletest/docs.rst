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

.. code-block:: sh

   git submodule add https://github.com/google/googletest third_party/googletest

Configure ``pw_unit_test`` to use upstream GoogleTest/GoogleMock.

.. tab-set::

   .. tab-item:: GN

      * Set the GN var ``dir_pw_third_party_googletest`` to the location of the
        GoogleTest source. If you used the command above this will be
        ``//third_party/googletest``.
      * Set the GN var ``pw_unit_test_MAIN`` to
        ``dir_pigweed + "/third_party/googletest:gmock_main"``.
      * Set the GN var ``pw_unit_test_GOOGLETEST_BACKEND`` to
        ``"//third_party/googletest"``.

      Pigweed unit tests that do not work with upstream GoogleTest can be
      disabled by setting ``enable_if`` to
      ``pw_unit_test_GOOGLETEST_BACKEND == "$dir_pw_unit_test:light"``.

   .. tab-item:: CMake

      * Set the ``dir_pw_third_party_googletest`` to the location of the
        GoogleTest source.
      * Set the var
        ``pw_unit_test_MAIN`` to ``pw_third_party.googletest.gmock_main``.
      * Set the var ``pw_unit_test_GOOGLETEST_BACKEND`` to
        ``pw_third_party.googletest``.

   .. tab-item:: Bazel

      Set the following :ref:`label flags <docs-build_system-bazel_flags>`,
      either in your
      :ref:`target config <docs-build_system-bazel_configuration>` or on
      the command line:

      * ``pw_unit_test_googletest_backend`` to
        ``@com_google_googletest//:gtest``.
      * ``pw_unit_test_main`` to ``@com_google_googletest//:gtest_main``.

      For example:

      .. code-block:: sh

         bazel test //... \
            --@pigweed//targets:pw_unit_test_googletest_backend=@com_google_googletest//:gtest \
            --@pigweed//targets:pw_unit_test_main=@com_google_googletest//:gtest_main

.. note::

  Not all unit tests build properly with upstream GoogleTest yet. This is a
  work in progress.
