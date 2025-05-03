.. _module-pw_third_party_llvm_libc:

=========
LLVM libc
=========
The ``$pw_external_llvm_libc/`` module provides various helpers to
optionally use LLVM ``libc`` with :ref:`module-pw_libc`.

------------------------
Using upstream LLVM libc
------------------------
If you want to use LLVM ``libc``, you must do the following:

Submodule
=========
Add LLVM ``libc`` to your workspace with the following command.

.. code-block:: console

   $ git submodule add https://llvm.googlesource.com/llvm-project/libc \
   > third_party/llvm_libc/src

.. note::

   This Git repository is maintained by Google and is a slice of upstream
   LLVM including only the ``libc`` subdirectory.

GN
==
* Set the GN var ``dir_pw_third_party_llvm_libc`` to the location of the LLVM
  ``libc`` source. If you used the command above, this will be
  ``//third_party/llvm_libc/src``.

  This can be set in your ``args.gn`` or ``.gn`` file like:

  .. code-block:: text

     dir_pw_third_party_llvm_libc = "//third_party/llvm_libc_src"

------
Status
------
Currently, the llvm-libc integration in ``pw_libc`` only provides a
``pw_libc.a`` and is not suitable as a full replacement for ``libc``. Not all
functions used by projects are currently available to use from llvm-libc.
Moreover, headers are provided from the sysroot ``libc``. Startup files are also
provided from the sysroot.

In the future, we hope to be able to fully replace the sysroot ``libc``.
