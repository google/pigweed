.. _docs-style-doxygen:

===================
Doxygen style guide
===================
.. _Doxygen: https://www.doxygen.nl/index.html

``pigweed.dev`` uses `Doxygen`_ to auto-generate C and C++ API references
from code comments in header files. These specially formatted comments are
called **Doxygen comments**. This style guide shows you how to format Doxygen
comments and other related documentation.

.. _docs-style-doxygen-quickstart:

----------
Quickstart
----------
Auto-generating an API reference on ``pigweed.dev`` requires interacting with
a few different tools. This section provides an overview of when you interact
with each tool, using ``pw_i2c`` as an example.

.. inclusive-language: disable

.. _//pw_i2c/public/pw_i2c/device.h: https://cs.opensource.google/pigweed/pigweed/+/4c1a7158b663f114c297d9c0a806f412768e73f8:pw_i2c/public/pw_i2c/device.h
.. _Breathe directives: https://breathe.readthedocs.io/en/latest/directives.html
.. _Sphinx: https://www.sphinx-doc.org/en/master/
.. _//pw_i2c/reference.rst: https://cs.opensource.google/pigweed/pigweed/+/4c1a7158b663f114c297d9c0a806f412768e73f8:pw_i2c/reference.rst;l=44
.. _//docs/BUILD.gn: https://cs.opensource.google/pigweed/pigweed/+/4c1a7158b663f114c297d9c0a806f412768e73f8:docs/BUILD.gn;l=192

.. inclusive-language: enable

#. Annotate your header file using `Doxygen`_ syntax. All of the comments
   that start with triple slashes (``///``) are Doxygen comments. Doxygen
   ignores double slash (``//``) comments.

   Example: `//pw_i2c/public/pw_i2c/device.h`_

#. Include the API reference content into a reStructuredText file using
   `Breathe directives`_. Breathe is the bridge between Doxygen and `Sphinx`_,
   the documentation generator that powers ``pigweed.dev``. See
   :ref:`docs-style-doxygen-breathe-overview` for more explanation.

   Example: `//pw_i2c/reference.rst`_

#. Add your header file's path to the ``_doxygen_input_files`` list in
   ``//docs/BUILD.gn``. The docs build system throws a "symbol not found"
   errors if you forget this step.

   Example: `//docs/BUILD.gn`_

.. _docs-style-doxygen-writing:

---------------------------
API reference writing style
---------------------------
.. _API reference code comments: https://developers.google.com/style/api-reference-comments

Follow the guidance in `API reference code comments`_.

.. _docs-style-doxygen-comment-style:

---------------------
Doxygen comment style
---------------------
This section explains how you should style the comments within header files
that Doxygen converts into API references.

.. _docs-style-doxygen-comment-syntax:

Comment syntax
==============
Use the triple slash (``///``) syntax.

.. admonition:: **Yes**
   :class: checkmark

   .. code-block:: none

      /// ...
      /// ...

.. _docs-style-doxygen-special-command-syntax:

Special command syntax
======================
.. _Special commands: https://www.doxygen.nl/manual/commands.html

`Special commands`_ like ``@code`` and ``@param`` are the core annotation
tools of Doxygen. Doxygen recognizes words that begin with either backslashes
(``\``) or at symbols (``@``) as special commands. For example, ``\code`` and
``@code`` are synonyms. Always use the at symbol (``@``) format.

.. admonition:: **Yes**
   :class: checkmark

   .. code-block:: none

      /// @param[out] dest The memory area to copy to.

.. admonition:: **No**
   :class: error

   .. code-block:: none

      /// \param dest The memory area to copy to.

.. _docs-style-doxygen-structural-commands:

Structural commands
===================
.. _Doxygen structural commands: https://doxygen.nl/manual/docblocks.html#structuralcommands

`Doxygen structural commands`_ like ``@struct``, ``@fn``, ``@class``, and ``@file``
associate a comment to a symbol. Don't use structural commands if they're not
needed. In other words, if your Doxygen comment renders correctly without the
structural command, don't use it.

Code examples
=============
Use ``@code{.cpp}`` followed by the code example followed by ``@endcode``.

.. admonition:: **Yes**
   :class: checkmark

   .. code-block:: none

      /// @code{.cpp}
      ///   #include "pw_kvs/key_value_store.h"
      ///   #include "pw_kvs/flash_test_partition.h"
      ///
      ///   ...
      /// @endcode

Omit or change ``{.cpp}`` if your code example isn't C++ code.

.. admonition:: **Yes**
   :class: checkmark

   .. code-block:: none

      /// @code
      ///   START + I2C_ADDRESS + WRITE(0) + TX_BUFFER_BYTES + STOP
      ///   START + I2C_ADDRESS + READ(1) + RX_BUFFER_BYTES + STOP
      /// @endcode

.. _docs-style-doxygen-params:

Parameters
==========
Use ``@param[<direction>] <name> <description>`` for each parameter.

.. admonition:: **Yes**
   :class: checkmark

   .. code-block:: none

      /// @param[out] dest The memory area to copy to.
      /// @param[in] src The memory area to copy from.
      /// @param[in] n The number of bytes to copy.

Put a newline between the parameters if you need multi-line descriptions.

.. admonition:: **Yes**
   :class: checkmark

   .. code-block:: none

      /// @param[out] dest Lorem ipsum dolor sit amet, consectetur adipiscing
      /// elit, sed do eiusmod tempor incididunt ut labore et dolore magna...
      ///
      /// @param[in] src The memory area to copy from.
      ///
      /// @param[in] n The number of bytes to copy.

The direction annotation is required.

.. admonition:: **No**
   :class: error

   .. code-block:: none

      /// @param dest The memory area to copy to.
      /// @param src The memory area to copy from.
      /// @param n The number of bytes to copy.

   ``<direction>`` must be specified and the value must be either ``in`` or
   ``out``.

.. _docs-style-doxygen-pre:

Preconditions
=============
Use ``@pre <description>``.

.. admonition:: **Yes**
   :class: checkmark

   .. code-block:: none

      /// @pre Description of a precondition that must be satisifed before
      /// invoking this function.

.. _docs-style-doxygen-pw_status:

pw_status codes
===============
Use the following syntax when referring to ``pw_status`` codes in Doxygen
comments:

.. admonition:: **Yes**
   :class: checkmark

   .. code-block:: none

      @pw_status{...}

Replace ``...`` with a valid ``pw_status`` code. See
:ref:`module-pw_status-quickref`.

Doxygen converts this alias into a link to the status code's reference
documentation.

.. _docs-style-doxygen-doxygen-pw_status-return:

Methods that return pw_status codes
===================================
Use ``@return A `pw::Status` object with one of the following statuses:``
followed by a bullet list of the relevant statuses and a description
of each scenario.

.. admonition:: **Yes**
   :class: checkmark

   .. code-block:: none

      /// @returns A `pw::Status` object with one of the following statuses:
      /// * @pw_status{...} - <description of scenario>
      /// * @pw_status{...} - <description of scenario>
      /// * @pw_status{...} - <description of scenario>

.. admonition:: **No**
   :class: error

   .. code-block:: none

      /// @returns A `pw::Status` object with one of the following statuses:
      ///
      /// * @pw_status{...} - <description of scenario>
      /// * @pw_status{...} - <description of scenario>
      /// * @pw_status{...} - <description of scenario>

   Don't put a blank line between the ``@returns`` line and the line after it.
   The list won't render in the correct place.

.. _docs-style-doxygen-namespaces:

Use fully namespaced names
==========================
In general, write out the full namespace to Pigweed classes, methods, and
so on. If you're writing a code sample, and that code sample clearly shows
where the item comes from via a ``using`` statement, you don't need to use
full namespacing.

.. admonition:: Discussion

   Pigweed has over 160 modules. It can be overwhelming for beginners
   to figure out where an item is coming from.

.. _docs-style-doxygen-multisymbol:

Single comment block for multiple symbols
=========================================
Use ``@def <symbol>`` followed by the comment block.

.. admonition:: **Yes**
   :class: checkmark

   .. code-block:: cpp

      /// @def PW_ASSERT_EXCLUSIVE_LOCK
      /// @def PW_ASSERT_SHARED_LOCK
      ///
      /// Documents functions that dynamically check to see if a lock is held, and
      /// fail if it is not held.

.. _docs-style-doxygen-xrefs:

Cross-references (x-refs)
=========================

.. _docs-style-doxygen-xrefs-comments:

X-refs in Doxygen comments
--------------------------
For C or C++ x-refs, use one of the following aliases:

.. csv-table::
   :header: Alias, reStructuredText equivalent

   ``@c_macro{<identifier>}``, ``:c:macro:`<identifier>```
   ``@cpp_func{<identifier>}``, ``:cpp:func:`<identifier>```
   ``@cpp_class{<identifier>}``, ``:cpp:class:`<identifier>```
   ``@cpp_type{<identifier>}``, ``:cpp:type:`<identifier>```

.. inclusive-language: disable

.. _Sphinx Domain: https://www.sphinx-doc.org/en/master/usage/domains/index.html

.. inclusive-language: enable

For all other x-refs, use Pigweed's custom basic alias,
``@crossref{<domain>,<type>,<identifier>}``. ``<domain>`` must be a valid
`Sphinx Domain`_ and ``<type>`` must be a valid type within that domain.
``@crossref`` can be used with any domain.

Avoid Doxygen x-refs
^^^^^^^^^^^^^^^^^^^^
Always use Pigweed's custom aliases for x-refs unless you have specific
reasons not to do so. Pigweed's x-ref aliases are built on top of Sphinx.
Doxygen provides its own features for x-refs but they should be avoided
because Sphinx's are better:

* Sphinx x-refs work for all identifiers known to Sphinx, including
  those documented with directives like ``.. cpp:class::`` or extracted from
  Python. Doxygen references can only refer to identifiers known to Doxygen.
* Sphinx x-refs always use consistent formatting. Doxygen
  x-refs sometimes render as plaintext instead of code-style
  monospace, or include ``()`` in macros that shouldn’t have them.
* Sphinx x-refs can refer to symbols that haven't yet been documented.
  They will be formatted correctly and become links once the symbols are
  documented.

Using Sphinx x-refs in Doxygen comments makes x-ref syntax more consistent
within Doxygen comments and between reStructuredText and Doxygen.

.. _docs-style-doxygen-xrefs-rest:

Cross-references in reST to Doxygen symbols
-------------------------------------------
After you've used Doxygen to generate API references, you can link to those
symbols from your reStructuredText with standard Sphinx x-ref
syntax.

.. admonition:: **Yes**
   :class: checkmark

   .. code-block:: rst

      :cpp:class:`pw::sync::BinarySemaphore::BinarySemaphore`

.. inclusive-language: disable

.. _domain: https://www.sphinx-doc.org/en/master/usage/domains/index.html
.. _C++ Domain: https://www.sphinx-doc.org/en/master/usage/domains/cpp.html
.. _C Domain: https://www.sphinx-doc.org/en/master/usage/domains/c.html
.. _Python Domain: https://www.sphinx-doc.org/en/master/usage/domains/python.html

.. inclusive-language: enable

In the Sphinx docs the reference documentation for x-ref syntax is
provided in the `domain`_ docs:

* `C++ Domain`_
* `C Domain`_
* `Python Domain`_

.. _docs-style-doxygen-embedded-rest:

Embedded reStructuredText
=========================
To use reStructuredText (reST) within a Doxygen comment, wrap the reST
in ``@rst`` and ``@endrst``.

.. _docs-style-doxygen-breathe:

-------
Breathe
-------
.. _Breathe: https://breathe.readthedocs.io

This section provides guidance on how `Breathe`_ should and shouldn't be used
when authoring ``pigweed.dev`` docs.

.. _docs-style-doxygen-breathe-overview:

Overview
========
.. inclusive-language: disable

.. _Breathe directives: https://breathe.readthedocs.io/en/latest/directives.html
.. _Sphinx: https://www.sphinx-doc.org/en/master/

.. inclusive-language: enable

After you annotate your header comments as Doxygen comments, you need to
specify where to render the API reference within the ``pigweed.dev`` docs.
The reStructuredText files distributed across the main Pigweed repo are
the source code for ``pigweed.dev``. Updating these ``.rst`` files is how
you surface the API reference on ``pigweed.dev``. Doxygen doesn't natively
interact with `Sphinx`_, the documentation generator that powers
``pigweed.dev``. `Breathe`_ is the bridge and API that enables ``pigweed.dev``
and Doxygen to work together.

.. _docs-style-doxygen-breathe-doxygenclass:

doxygenclass
============
.. _doxygenclass: https://breathe.readthedocs.io/en/latest/directives.html#doxygenclass

OK to use. `doxygenclass`_ documents a class and its members.

.. admonition:: **Yes**
   :class: checkmark

   .. code-block:: rst

      .. doxygenclass:: pw::sync::BinarySemaphore
         :members:

Classes that are a major part of a Pigweed module's API should have their
own section so that they're easy to find in the right-side page nav on
``pigweed.dev``.

.. admonition:: **Yes**
   :class: checkmark

   .. code-block:: rst

      .. _module-pw_<name>-reference:

      =========
      Reference
      =========
      .. pigweed-module-subpage::
         :name: pw_<name>

      ...

      .. _module-pw_<name>-reference-<class>:

      -------------------
      pw::<name>::<class>
      -------------------
      .. doxygenclass:: pw::<name>::<class>
         :members:

.. _docs-style-doxygen-breathe-doxygenfunction:

doxygenfunction
===============
.. _doxygenfunction: https://breathe.readthedocs.io/en/latest/directives.html#doxygenfunction

OK to use. `doxygenfunction` documents a single function or method.

.. admonition:: **Yes**
   :class: checkmark

   .. code-block:: rst

      .. doxygenfunction:: pw::tokenizer::EncodeArgs

.. _docs-style-doxygen-breathe-doxygendefine:

doxygendefine
=============
.. _doxygendefine: https://breathe.readthedocs.io/en/latest/directives.html#doxygendefine

OK to use. `doxygendefine`_ documents a preprocessor macro.

.. admonition:: **Yes**
   :class: checkmark

   .. code-block:: rst

      .. doxygendefine:: PW_TOKENIZE_STRING

.. _docs-style-doxygen-breathe-doxygengroup:

doxygengroup
============
.. _doxygengroup: https://breathe.readthedocs.io/en/latest/directives.html#doxygengroup

`doxygengroup`_ lets you manually mark a set of symbols as belonging to the same
conceptual group.

``doxygengroup`` is OK to use when a simple
:ref:`docs-style-doxygen-breathe-doxygenclass`-based organization
doesn't work well for your module.

.. _@defgroup: https://www.doxygen.nl/manual/commands.html#cmddefgroup
.. _@addtogroup: https://www.doxygen.nl/manual/commands.html#cmdaddtogroup
.. _@ingroup: https://www.doxygen.nl/manual/commands.html#cmdingroup

To create a group, annotate your Doxygen comments with `@defgroup`_,
`@addtogroup`_, and `@ingroup`_. You can wrap a set of contiguous comments
in ``@{`` and ``@}`` to indicate that they all belong to a group.

.. admonition:: **Yes**
   :class: checkmark

   .. code-block:: cpp

      /// @defgroup <name> <description>
      /// @{
      /// ...
      /// @}

.. _issue #772: https://github.com/breathe-doc/breathe/issues/772

Don't include namespaces in ``doxygengroup`` because Breathe doesn't handle
them correctly. See `issue #772`_.

.. admonition:: **Yes**
   :class: checkmark

   .. code-block:: rst

      .. cpp:namespace:: my_namespace

      .. doxygengroup:: my_group
         :content-only:
         :members:

.. _docs-style-doxygen-breathe-doxygentypedef:

doxygentypedef
==============
.. _doxygentypedef: https://breathe.readthedocs.io/en/latest/directives.html#doxygentypedef

OK to use. `doxygentypedef`_ documents a ``typedef`` or ``using`` statement.

.. admonition:: **Yes**
   :class: checkmark

   .. code-block:: rst

      .. doxygentypedef:: pw::Function

.. _docs-style-doxygen-breathe-doxygenfile:

doxygenfile
===========
.. _doxygenfile: https://breathe.readthedocs.io/en/latest/directives.html#doxygenfile

Don't use `doxygenfile`_. Use :ref:`docs-style-doxygen-breathe-doxygengroup`
instead.

.. _docs-style-doxygen-disabled-include:

-----------------------------------------------
Disabled auto-generated ``#include`` statements
-----------------------------------------------
.. note::

   This is an FYI section. There's no prescriptive rule here.

Doxygen and Breathe have the ability to auto-generate ``#include`` statements
for class references. These have been disabled because:

* The auto-generated paths are inaccurate. E.g. the ``#include`` for
  ``pw::string::RandomGenerator`` was generated as ``#include <random.h>``
  when it should be ``#include "pw_random/random.h"``.
* The auto-generation is not consistent. They seem to only get generated when
  using the ``doxygennamespace`` directive but ``pigweed.dev`` frequently
  uses ``doxygenclass``, ``doxygenfunction``, etc.

In the future, if it's decided that these ``#include`` statements are needed,
there is a way to manually override each one. The code block below shows how
it's done. This approach wouldn't be great because it adds a lot of noise to
the header files.

.. code-block::

   /// @class RandomGenerator random.h "pw_random/random.h"``

See `b/295023422 <https://issues.pigweed.dev/issues/295023422>`_.
