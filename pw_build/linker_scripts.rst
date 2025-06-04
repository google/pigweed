.. _module-pw_build-linker_scripts:

==============
Linker Scripts
==============
.. pigweed-module-subpage::
   :name: pw_build

``pw_build`` provides utilities for working with linker scripts in embedded
projects. If using the ``GN`` or ``Bazel`` build systems you can preprocess your
linker script using the C preprocessor with the ``pw_linker_script`` rules.

- GN :ref:`module-pw_build-gn-pw_linker_script`
- Bazel :ref:`module-pw_build-bazel-pw_linker_script`

---------------------
Linker Script Helpers
---------------------

``PW_MUST_PLACE``
=================
.. doxygengroup:: pw_must_place
   :content-only:
   :members:

``PW_MUST_PLACE_SIZE``
======================
.. doxygengroup:: pw_must_place_size
   :content-only:
   :members:

``PW_MUST_NOT_PLACE``
=====================
.. doxygengroup:: pw_must_not_place
   :content-only:
   :members:

``LinkerSymbol``
================
.. doxygenclass:: pw::LinkerSymbol
   :members:

.. note::

   ``LinkerSymbol`` does not support, and is not necessary for, symbols that
   communicate a pointer value (i.e. an address). For those, simply define an
   extern variable of the pointed-to type, e.g.:

   .. code-block:: cpp

      extern "C" uint32_t PTR_SYM;

``LinkerSymbol`` is superior to the traditional ``extern uint8_t FOO;``
``(uint32_t)&FOO`` method because it catches subtle errors:

* Missing ``extern`` specifier:

  .. code-block:: none

     error: use of deleted function 'pw::build::LinkerSymbol::LinkerSymbol()'
     | LinkerSymbol oops;
     |              ^~~~

* Missing ``&`` operator:

  .. code-block:: none

     error: invalid cast from type 'pw::build::LinkerSymbol' to type 'uint32_t' {aka 'long unsigned int'}
     |  uint32_t val = (uint32_t)FOO_SYM;
     |                 ^~~~~~~~~~~~~~~~~
