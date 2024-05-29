.. _module-pw_clock_tree-impl:

===============
Implementations
===============
.. pigweed-module-subpage::
   :name: pw_clock_tree

To use the ``pw_clock_tree`` API, you need to specify in your build system an
implementation of the platform specific implementation of the ``pw_clock_tree``
virtual interface.

------------------------
Existing implementations
------------------------
The following Pigweed modules are ready-made ``pw_clock_tree`` implementations that
you can use in your projects:

* :ref:`module-pw_clock_tree_mcuxpresso` for the NXP MCUXpresso SDK.

.. toctree::
   :maxdepth: 1
   :hidden:

   MCUXpresso <../pw_clock_tree_mcuxpresso/docs>
