.. _contrib-doxygen:

===========================
Doxygen contributor's guide
===========================
This guide shows :ref:`docs-glossary-upstream` maintainers how to contribute
content to Pigweed's :cc:`C/C++ API reference <index>`, which is powered by
`Doxygen`_.

For Doxygen style rules, see :ref:`style-doxygen`.

.. _contrib-doxygen-quickstart:

----------
Quickstart
----------
#. Check if your module is defined in :cs:`docs/doxygen/modules.h`. If not,
   define it now.

   .. literalinclude:: ../../docs/doxygen/modules.h
      :language: cpp
      :start-at: /// @defgroup pw_bytes pw_bytes
      :end-at: /// @endmaindocs

   ``@defgroup`` repeats the module name to ensure proper capitalization.

   ``@brief`` should contain the module's tagline as defined in
   :cs:`docs/sphinx/module_metadata.json`. If your module doesn't have a
   tagline, now is a good time to create one!

   The content between ``@maindocs`` and ``@endmaindocs`` should be a complete
   list of links to the module's Sphinx docs.

#. (Optional) If your module has a large, complex API, define "submodule"
   groups (such as the ``pw_bytes_ptr`` subgroup shown below) in
   :cs:`docs/doxygen/modules.h` to help further organize your module's API
   reference:

   .. literalinclude:: ../../docs/doxygen/modules.h
      :language: cpp
      :start-at: /// @defgroup pw_bytes_ptr Packed pointers
      :end-at: /// @brief Store data in unused pointer bits

   See :cc:`pw_bytes` to view the rendered example. The ``pw_bytes`` group
   controls the module's API reference landing page. The main page is
   organized by "submodules". The ``pw_bytes_ptr`` group is one of the
   submodules. On that page, all APIs related to packed pointers are
   presented.

#. In your module's main ``BUILD.bazel`` file (e.g. ``//pw_bytes/BUILD.bazel``)
   create a ``filegroup`` named ``doxygen``. For ``srcs``, list all headers
   that Doxygen should process:

   .. literalinclude:: ../../pw_bytes/BUILD.bazel
      :language: cpp
      :start-after: # DOCSTAG: [doxygen]
      :end-before: # DOCSTAG: [doxygen]

   .. important::

      The Doxygen build is hermetic and all inputs are explicitly defined. If
      you forget to include a header at this stage, Doxygen won't know that the
      header exists.

#. In :cs:`docs/doxygen/BUILD.bazel` add your target to the ``filegroup`` named
   ``srcs``:

   .. code-block:: py

      filegroup(
          name = "srcs",
          srcs = [
              # …
              "//pw_bytes:doxygen",
              # …
          ]
      )

#. In each of the headers that you've listed in your module's ``doxygen``
   target, add ``@module`` or ``@submodule`` annotations.

   In both cases, make sure to use ``@}`` to close off your annotation.
   If you don't, Doxygen may pick up internal symbols that you did not intend
   to publish to the C/C++ API reference.

   For simple modules where you want the entire API listed on a single page,
   use ``@module``:

   .. code-block:: cpp

      namespace pw {

      /// @module{pw_foo}

      // …

      /// @}

      }  // namespace pw

   For more complex modules with large APIs, use ``@submodule``:

   .. code-block:: cpp

      namespace pw {

      /// @submodule{pw_bytes,ptr}

      // …

      /// @}

      }  // namespace pw

   ``@submodule{pw_bytes,ptr}`` will organize the API under the
   ``pw_bytes_ptr`` group that's defined in :cs:`docs/doxygen/modules.h`.

   The lack of whitespace between ``pw_bytes`` and ``ptr`` is important!

   Remember to use ``@}`` to close off your annotation!

#. Follow the style rules in :ref:`style-doxygen`.

.. _contrib-doxygen-doxylink:

---------------------------
Link from Sphinx to Doxygen
---------------------------
To link from the Sphinx docs to a Doxygen page, use the ``:cc:`` role. See
:ref:`docs-style-rest-doxylink` for usage and style guidance.

.. _Doxygen: https://www.doxygen.nl/index.html

.. _contrib-doxygen-links:

---------------------------
Link from Doxygen to Sphinx
---------------------------
Use normal Markdown links with relative paths. There is not yet any utility
like ``:cc:`` for managing Doxygen-to-Sphinx links.

.. literalinclude:: ../../docs/doxygen/modules.h
   :language: cpp
   :start-at: /// [Home](../../pw_bytes/docs.html)
   :end-at: /// [Home](../../pw_bytes/docs.html)

.. tip::

   Doxygen outputs all files into a single directory, so the Sphinx docs are
   always available two directories below (``../..``).

.. _Doxygen: https://www.doxygen.nl/index.html
