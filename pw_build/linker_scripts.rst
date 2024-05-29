.. _module-pw_build-linker_scripts:

==============
Linker Scripts
==============
``pw_build`` provides utilities for working with linker scripts in embedded
projects. If using the ``GN`` or ``Bazel`` build systems you can preprocess your
linker script using the C preprocessor with the ``pw_linker_script`` rules.

- GN :ref:`module-pw_build-gn-pw_linker_script`
- Bazel :ref:`module-pw_build-bazel-pw_linker_script`

---------------------
Linker Script Helpers
---------------------

``PW_MUST_PLACE``
-----------------
.. doxygengroup:: pw_must_place
   :content-only:
   :members:
