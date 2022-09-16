.. _module-pw_ide:

------
pw_ide
------
This module provides tools for supporting code editor and IDE features for
Pigweed projects.

Configuration
=============
Pigweed IDE settings are stored in the project root in ``.pw_ide.yaml``, in which
these options can be configured:

* ``working_dir``: The working directory for compilation databases and caches
  (by default this is `.pw_ide` in the project root). This directory shouldn't
  be committed to your repository or be a directory that is routinely deleted or
  manipulated by other processes.
