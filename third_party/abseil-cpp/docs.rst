.. _module-pw_third_party_abseil-cpp:

==========
Abseil C++
==========
The ``$dir_pw_third_party/abseil-cpp/`` module provides build files to allow
optionally including upstream Abseil C++.

-------------------------
Using upstream Abseil C++
-------------------------
If you want to use Abseil C++, you must do the following:

Submodule
=========
Add Abseil C++ to your workspace with the following command.

.. code-block:: sh

  git submodule add https://github.com/abseil/abseil-cpp.git \
    third_party/abseil-cpp/src

GN
==
* Set the GN var ``dir_pw_third_party_abseil-cpp`` to the location of the
  Abseil C++ source.

  If you used the command above, this will be
  ``//third_party/abseil-cpp/src``

  This can be set in your args.gn or .gn file like:
  ``dir_pw_third_party_abseil_cpp = "//third_party/abseil-cpp/src"``

Updating
========
The GN build files are generated from the third-party Bazel build files using
$dir_pw_build/py/pw_build/generate_3p_gn.py.

The script uses data taken from ``$dir_pw_third_party/abseil-cpp/repo.json``.

The script should be re-run whenever the submodule is updated or the JSON file
is modified. Specify the location of the Bazel repository can be specified using
the ``-w`` option, e.g.

.. code-block:: sh

  python pw_build/py/pw_build/generate_3p_gn.py \
    -w third_party/abseil-cpp/src

Version
=======
The update script was last run for revision `67f9650`_.

.. _67f9650: https://github.com/abseil/abseil-cpp/tree/67f9650c93a4fa04728a5b754ae8297d2c55d898
