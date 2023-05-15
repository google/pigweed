.. _module-pw_third_party_fuzztest:

========
FuzzTest
========
The ``$dir_pw_third_party/fuzztest/`` module provides build files to allow
optionally including upstream FuzzTest.

-----------------------
Using upstream FuzzTest
-----------------------
If you want to use FuzzTest, you must do the following:

Submodule
=========
Add FuzzTest to your workspace with the following command.

.. code-block:: sh

  git submodule add https://github.com/google/fuzztest.git \
    third_party/fuzztest

.. tab-set::

   .. tab-item:: GN

      Set the GN var ``dir_pw_third_party_fuzztest`` to the location of the
      FuzzTest source.

      If you used the command above, this will be ``//third_party/fuzztest``.

      This can be set in your args.gn or .gn file:
      ``dir_pw_third_party_fuzztest = "//third_party/fuzztest"``

   .. tab-item:: CMake

      Set the following CMake variables:

      * Set ``dir_pw_third_party_fuzztest`` to the location of the
        FuzzTest source.

      * Set ``dir_pw_third_party_googletest`` to the location of the
        GoogleTest source.

      * Set ``pw_unit_test_GOOGLETEST_BACKEND`` to ``pw_third_party.fuzztest``.

   .. tab-item:: Bazel

      Set the following `label flags`_, either in your `target config`_ or on
      the command line:

      * ``pw_fuzzer_fuzztest_backend`` to ``@com_google_fuzztest//fuzztest``.

      For example:

      .. code-block:: sh

         bazel test //... \
            --@pigweed_config//:pw_fuzzer_fuzztest_backend=@com_google_fuzztest//fuzztest

.. _target config: :ref:`_docs-build_system-bazel_configuration`
.. _label flags: :ref:`_docs-build_system-bazel_flags`

Updating
========
The GN build files are generated from the third-party Bazel build files using
$dir_pw_build/py/pw_build/generate_3p_gn.py.

The script uses data taken from ``$dir_pw_third_party/fuzztest/repo.json``.

The script should be re-run whenever the submodule is updated or the JSON file
is modified. Specify the location of the Bazel repository can be specified using
the ``-w`` option, e.g.

.. code-block:: sh

  python pw_build/py/pw_build/generate_3p_gn.py \
    -w third_party/fuzztest/src

Version
=======
The update script was last run for revision `3c77f97`_.

.. _3c77f97: https://github.com/google/fuzztest/tree/3c77f97183a1270796d25db1a8956706a25af238
