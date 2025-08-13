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
.. TODO: b/426012010 - Explain how to integrate into the Bazel build.

.. _member group: https://www.doxygen.nl/manual/grouping.html#memgroup

#. Check if your module is defined in ``//docs/doxygen/modules.h``. If not,
   define it now.

   .. code-block:: none

      /// @defgroup pw_function pw_function
      /// @brief Embedded-friendly `std::function`: https://pigweed.dev/pw_function

   The ``@defgroup`` line repeats the module name to ensure proper capitalization.
   ``@defgroup pw_function`` results in Doxygen rendering the module name as
   ``Pw_function``.

   ``@brief`` should match this pattern: ``<tagline>: <url>`` where ``<tagline>``
   is the module's tagline as defined in ``//docs/sphinx/module_metadata.json``
   and ``<url>`` is the URL to the module's homepage.

#. Add a ``@module`` declaration within each header file that you want Doxygen
   to process.

   .. code-block:: none

      namespace pw {

      /// @module{pw_function}

      …

   The ``@module`` declaration must be nested within the ``pw`` namespace.
   It can't be outside of the ``pw`` namespace.

#. If needed, explicitly close the `member group`_ that the ``@module`` declaration
   created for you behind-the-scenes:

   .. code-block:: none

      /// @}

   In most cases you can skip this step. You only need to explicitly close the group
   when the header contains internal APIs that you don't want to publish to
   the Doxygen site. If there's more public API after the internal block, you can repeat
   the ``@module`` declaration again. Example:

   .. code-block:: none

      namespace pw {

      /// @module{pw_function)

      <public API stuff>

      /// @}

      namespace internal_stuff {

        <internal stuff>

      }

      /// @module{pw_function)

      <more public API stuff>

   .. tip::

      You can also avoid this step by moving your internal APIs to an internal
      directory, e.g. ``//pw_function/public/pw_function/internal/``.

#. To be continued…

.. _doxygen-modify:

-----------------
Modifying Doxygen
-----------------
To be continued…
