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

      /// @defgroup pw_async2 pw_async2
      /// @brief Cooperative async tasks for embedded.
      /// @details Main docs: [Home](../../pw_async2/docs.html) |
      /// [Quickstart](../../pw_async2/quickstart.html) |
      /// [Codelab](../../pw_async2/codelab.html) |
      /// [Guides](../../pw_async2/guides.html) |
      /// [Code size analysis](../../pw_async2/code_size.html) |
      /// [Coroutines](../../pw_async2/coroutines.html) |
      /// [Backends](../../pw_async2/backends.html)

   ``@defgroup`` repeats the module name to ensure proper capitalization.

   ``@brief`` should contain the module's tagline as defined in
   ``//docs/sphinx/module_metadata.json``.

   ``@details`` should be a complete list to the module's main docs.

#. Create `@submodule` groups to help further categorize your module's API:

   .. code-block:: none

      /// @defgroup pw_async2_adapters Pendable adapters
      /// @ingroup pw_async2
      /// @brief Pendable wrappers and helpers

#. Add a ``@submodule`` declaration within each header file that you want
   Doxygen to process.

   .. code-block:: none

      namespace pw {

      /// @submodule{pw_async2,adapters}

      …

   The ``@submodule`` declaration must be nested within the ``pw`` namespace.
   It can't be outside of the ``pw`` namespace.

   The lack of whitespace between ``pw_async2`` and ``adapters`` is important!

#. Explicitly close the `member group`_ that the ``@submodule`` declaration
   created for you behind-the-scenes:

   .. code-block:: none

      /// @}

#. To be continued…

.. Mention PW_EXCLUDE_FROM_DOXYGEN

.. _doxygen-modify:

-----------------
Modifying Doxygen
-----------------
To be continued…
