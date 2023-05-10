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

  git submodule add git@github.com:nopsledder/fuzztest.git \
    third_party/fuzztest/src

.. tab-set::

   .. tab-item:: GN

      Set the GN var ``dir_pw_third_party_fuzztest`` to the location of the
      FuzzTest source.

      If you used the command above, this will be ``//third_party/fuzztest``.

      This can be set in your args.gn or .gn file:
      ``dir_pw_third_party_fuzztest = "//third_party/fuzztest"``

   .. tab-item:: CMake

      CMake support is in development.

   .. tab-item:: Bazel

      Bazel support is in development.

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
The update script was last run for revision:
f2e9e2a19a7b16101d1e6f01a87e639687517a1c
