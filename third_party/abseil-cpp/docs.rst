.. _module-pw_third_party_abseil_cpp:

==========
Abseil C++
==========
The ``$pw_external_abseil_cpp/`` module provides build files to allow
optionally including upstream Abseil C++.

.. _module-pw_third_party_abseil_cpp-using_upstream:

-------------------------
Using upstream Abseil C++
-------------------------
If you want to use Abseil C++, you must do the following:

Submodule
=========
Add Abseil C++ to your workspace with the following command.

.. code-block:: console

   $ git submodule add https://github.com/abseil/abseil-cpp.git \
   > third_party/abseil-cpp/src

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
Currently, all dependencies on Abseil-C++ are indirect and via other third-party
modules:

* FuzzTest

The GN build files for Abseil-C++ will be updated when the build files for those
modules are updated. See those modules for instructions on updating.
