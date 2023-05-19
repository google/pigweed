.. _module-pw_third_party_re2:

===
RE2
===
The ``$dir_pw_third_party/re2/`` module provides build files to allow
optionally including upstream RE2.

.. _module-pw_third_party_re2-using_upstream:

------------------
Using upstream RE2
------------------
If you want to use RE2, you must do the following:

Submodule
=========
Add RE2 to your workspace with the following command.

.. code-block:: sh

  git submodule add https://github.com/google/re2.git \
    third_party/re2/src

GN
==
* Set the GN var ``dir_pw_third_party_re2`` to the location of the
  RE2 source.

  If you used the command above, this will be
  ``//third_party/re2/src``

  This can be set in your args.gn or .gn file like:
  ``dir_pw_third_party_re2 = "//third_party/re2/src"``

Updating
========
The GN build files are generated from the third-party Bazel build files using
$dir_pw_build/py/pw_build/generate_3p_gn.py.

The script uses data taken from ``$dir_pw_third_party/re2/repo.json``.

The script should be re-run whenever the submodule is updated or the JSON file
is modified. Specify the location of the Bazel repository can be specified using
the ``-w`` option, e.g.

.. code-block:: sh

  python pw_build/py/pw_build/generate_3p_gn.py \
    -w third_party/re2/src

Version
=======
The update script was last run for revision `c9cba76`_.

.. _c9cba76: https://github.com/google/re2/tree/c9cba76063cf4235c1a15dd14a24a4ef8d623761
