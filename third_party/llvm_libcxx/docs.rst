.. _module-pw_third_party_llvm_libcxx:

===========
LLVM libcxx
===========
The ``$dir_pw_third_party/llvm_libcxx/`` module provides various helpers to
optionally use LLVM ``libcxx`` with :ref:`module-pw_libcxx`.

---------------------------
Using upstream LLVM libcxx
---------------------------
If you want to use LLVM ``libcxx``, you must do the following:

Submodule
=========
Add LLVM ``libcxx`` to your workspace with the following command.

.. code-block:: console

   $ git submodule add https://llvm.googlesource.com/llvm-project/libcxx \
   > third_party/llvm_libcxx/src

Note, this git repository is maintained by Google and is a slice of upstream
LLVM including only the ``libcxx`` subdirectory.

GN
==
* Set the GN var ``dir_pw_third_party_llvm_libcxx`` to the location of the LLVM
  ``libcxx`` source. If you used the command above, this will be
  ``//third_party/llvm_libcxx/src``.

  This can be set in your ``args.gn`` or ``.gn`` file:

  .. code-block:: text

     dir_pw_third_party_llvm_libcxx = "//third_party/llvm_libcxx_src"

------
Status
------
The llvm_libcxx integration in ``pw_libcxx`` is under active development.
