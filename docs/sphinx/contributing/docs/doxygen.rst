.. _doxygen:

==============================================
Working with upstream Pigweed's Doxygen system
==============================================
.. TODO: b/426012010 - Find a better way to link to the Doxygen site.
.. _official Pigweed C/C++ API reference: https://pigweed.dev/doxygen

The `official Pigweed C/C++ API reference`_ is powered by Doxygen. This guide
shows :ref:`docs-glossary-upstream` maintainers how to:

* :ref:`Author content <doxygen-author>` for the Pigweed C/C++ API reference

* :ref:`Modify the upstream Pigweed Doxygen system <doxygen-modify>` itself

.. note:: This document is a work in progress.

.. _doxygen-author:

-----------------
Authoring content
-----------------
#. Add a ``@module`` declaration to your module:

   .. code-block:: none

      /// @module{pw_function,Embedded-friendly std::function}

   .. note::

      This is a custom alias that we've created for upstream Pigweed to ensure
      that all Pigweed modules are documented in the same way. The alias is
      defined in ``//docs/doxygen/Doxyfile``. The example above expands to
      this:

      .. code-block:: none

         /// @defgroup pw_function pw_function
         /// @brief Embedded-friendly std::function: https://pigweed.dev/pw_function

#. To be continued…

.. _doxygen-modify:

-----------------
Modifying Doxygen
-----------------
To be continued…
