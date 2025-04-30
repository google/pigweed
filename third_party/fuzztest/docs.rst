.. _module-pw_third_party_fuzztest:

========
FuzzTest
========
The ``$dir_pw_third_party/fuzztest/`` module provides build files to allow
optionally including upstream FuzzTest.

.. _module-pw_third_party_fuzztest-using_upstream:

-----------------------
Using upstream FuzzTest
-----------------------
If you want to use FuzzTest, you must do the following:

Submodule
=========
Add FuzzTest to your workspace with the following command.

.. code-block:: console

   $ git submodule add https://github.com/google/fuzztest.git \
   > third_party/fuzztest

.. tab-set::

   .. tab-item:: GN

      Set the GN following GN bauild args:

      * Set ``dir_pw_third_party_fuzztest`` to the location of the FuzzTest
        source. If you used the command above, this will be
        ``//third_party/fuzztest``.

      * Set ``dir_pw_third_party_abseil_cpp`` to the location of the
        :ref:`module-pw_third_party_abseil_cpp` source.

      * Set ``dir_pw_third_party_googletest`` to the location of the
        :ref:`module-pw_third_party_googletest` source.

      This can be set in your ``args.gn`` or ``.gn`` file. For example:

      .. code-block::

         # Set build arguments here. See `gn help buildargs`.
         dir_pw_third_party_abseil_cpp="//third_party/abseil-cpp"
         dir_pw_third_party_fuzztest="//third_party/fuzztest"
         dir_pw_third_party_googletest="//third_party/googletest"

   .. tab-item:: CMake

      Set the following CMake variables:

      * Set ``dir_pw_third_party_fuzztest`` to the location of the
        FuzzTest source.

      * Set ``dir_pw_third_party_googletest`` to the location of the
        :ref:`module-pw_third_party_googletest` source.

      * Set ``pw_unit_test_BACKEND`` to ``pw_third_party.fuzztest``.

   .. tab-item:: Bazel

      Set the following :ref:`label flags <docs-build_system-bazel_flags>`,
      either in your :ref:`target config
      <docs-build_system-bazel_configuration>` or on the command line:

      * ``pw_fuzzer_fuzztest_backend`` to ``@com_google_fuzztest//fuzztest``.

      For example:

      .. code-block:: console

         $ bazel test //... \
         > --@pigweed//targets:pw_fuzzer_fuzztest_backend=@com_google_fuzztest//fuzztest

Updating
========
The GN build files are generated from the third-party Bazel build files using
$dir_pw_build/py/pw_build/bazel_to_gn.py.

The script uses data taken from a ``bazel_to_gn.json`` file for this module and
for each third party module that this module depends on, e.g.
``$PW_ROOT/third_party/fuzztest/bazel_to_gn.json``.

The script should be re-run whenever the submodule is updated or the JSON file
is modified. Specify the location of the Bazel repository using ``gn args``,
then run:

.. code-block:: console

   $ bazelisk run //pw_build/py:bazel_to_gn fuzztest
