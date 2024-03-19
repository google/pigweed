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

.. code-block:: sh

   git submodule add https://github.com/google/fuzztest.git \
     third_party/fuzztest

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

      * Set ``dir_pw_third_party_re2`` to the location of the
        :ref:`module-pw_third_party_re2` source.

      This can be set in your ``args.gn`` or ``.gn`` file. For example:

      .. code-block::

         # Set build arguments here. See `gn help buildargs`.
         dir_pw_third_party_abseil_cpp="//third_party/abseil-cpp"
         dir_pw_third_party_fuzztest="//third_party/fuzztest"
         dir_pw_third_party_googletest="//third_party/googletest"
         dir_pw_third_party_re2="//third_party/re2"

   .. tab-item:: CMake

      Set the following CMake variables:

      * Set ``dir_pw_third_party_fuzztest`` to the location of the
        FuzzTest source.

      * Set ``dir_pw_third_party_googletest`` to the location of the
        :ref:`module-pw_third_party_googletest` source.

      * Set ``pw_unit_test_BACKEND`` to ``pw_third_party.fuzztest``.

   .. tab-item:: Bazel

      Set the following `label flags`_, either in your `target config`_ or on
      the command line:

      * ``pw_fuzzer_fuzztest_backend`` to ``@com_google_fuzztest//fuzztest``.

      For example:

      .. code-block:: sh

         bazel test //... \
            --@pigweed//targets:pw_fuzzer_fuzztest_backend=@com_google_fuzztest//fuzztest

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

Additionally, the ``fuzztest.bazelrc`` file should regenerated. From this
directory, run:

.. code-block:: sh

   bazel run @com_google_fuzztest//bazel:setup_configs > fuzztest.bazelrc

.. DO NOT EDIT BELOW THIS LINE. Generated section.

Version
=======
The update script was last run for revision `6eb010c7`_.

.. _6eb010c7: https://github.com/google/fuzztes/tree/6eb010c7223a6aa609b94d49bfc06ac88f922961
