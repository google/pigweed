.. _docs-pw-style-doxygen:

===========================
Doxygen documentation style
===========================
Doxygen comments in C, C++, and Java are surfaced in Sphinx using `Breathe
<https://breathe.readthedocs.io/en/latest/index.html>`_.

.. note::

   Sources with doxygen comment blocks must be added to the
   ``_doxygen_input_files`` list in ``//docs/BUILD.gn`` to be processed.

Breathe provides various `directives
<https://breathe.readthedocs.io/en/latest/directives.html>`_ for bringing
Doxygen comments into Sphinx. These include the following:

- `doxygenfile
  <https://breathe.readthedocs.io/en/latest/directives.html#doxygenfile>`_ --
  Documents a source file. May limit to specific types of symbols with
  ``:sections:``.

  .. code-block:: rst

     .. doxygenfile:: pw_rpc/internal/config.h
        :sections: define, func

- `doxygenclass
  <https://breathe.readthedocs.io/en/latest/directives.html#doxygenclass>`_ --
  Documents a class and optionally its members.

  .. code-block:: rst

     .. doxygenclass:: pw::sync::BinarySemaphore
        :members:

- `doxygentypedef
  <https://breathe.readthedocs.io/en/latest/directives.html#doxygentypedef>`_ --
  Documents an alias (``typedef`` or ``using`` statement).

  .. code-block:: rst

     .. doxygentypedef:: pw::Function

- `doxygenfunction
  <https://breathe.readthedocs.io/en/latest/directives.html#doxygenfunction>`_ --
  Documents a source file. Can be filtered to limit to specific types of
  symbols.

  .. code-block:: rst

     .. doxygenfunction:: pw::tokenizer::EncodeArgs

- `doxygendefine
  <https://breathe.readthedocs.io/en/latest/directives.html#doxygendefine>`_ --
  Documents a preprocessor macro.

  .. code-block:: rst

     .. doxygendefine:: PW_TOKENIZE_STRING

.. admonition:: See also

  `All Breathe directives for use in RST files <https://breathe.readthedocs.io/en/latest/directives.html>`_

Example Doxygen Comment Block
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Start a Doxygen comment block using ``///`` (three forward slashes).

.. code-block:: cpp

   /// This is the documentation comment for the `PW_LOCK_RETURNED()` macro. It
   /// describes how to use the macro.
   ///
   /// Doxygen comments can refer to other symbols using Sphinx cross
   /// references. For example, @cpp_class{pw::InlineBasicString}, which is
   /// shorthand for @crossref{cpp,class,pw::InlineBasicString}, links to a C++
   /// class. @crossref{py,func,pw_tokenizer.proto.detokenize_fields} links to a
   /// Python function.
   ///
   /// @param[out] dest The memory area to copy to.
   /// @param[in]  src  The memory area to copy from.
   /// @param[in]  n    The number of bytes to copy
   ///
   /// @retval OK KVS successfully initialized.
   /// @retval DATA_LOSS KVS initialized and is usable, but contains corrupt data.
   /// @retval UNKNOWN Unknown error. KVS is not initialized.
   ///
   /// @rst
   /// The ``@rst`` and ``@endrst`` commands form a block block of
   /// reStructuredText that is rendered in Sphinx.
   ///
   /// .. warning::
   ///    this is a warning admonition
   ///
   /// .. code-block:: cpp
   ///
   ///    void release(ptrdiff_t update = 1);
   /// @endrst
   ///
   /// Example code block using Doxygen markup below. To override the language
   /// use `@code{.cpp}`
   ///
   /// @code
   ///   class Foo {
   ///    public:
   ///     Mutex* mu() PW_LOCK_RETURNED(mu) { return &mu; }
   ///
   ///    private:
   ///     Mutex mu;
   ///   };
   /// @endcode
   ///
   /// @b The first word in this sentence is bold (The).
   ///
   #define PW_LOCK_RETURNED(x) __attribute__((lock_returned(x)))

Doxygen Syntax
^^^^^^^^^^^^^^
Pigweed prefers to use RST wherever possible, but there are a few Doxygen
syntatic elements that may be needed.

Common Doxygen commands for use within a comment block:

- ``@rst`` To start a reStructuredText block. This is a custom alias for
  ``\verbatim embed:rst:leading-asterisk``. This must be paired with
  ``@endrst``.
- `@code <https://www.doxygen.nl/manual/commands.html#cmdcode>`_ Start a code
  block. This must be paired with ``@endcode``.
- `@param <https://www.doxygen.nl/manual/commands.html#cmdparam>`_ Document
  parameters, this may be repeated.
- `@pre <https://www.doxygen.nl/manual/commands.html#cmdpre>`_ Starts a
  paragraph where the precondition of an entity can be described.
- `@post <https://www.doxygen.nl/manual/commands.html#cmdpost>`_ Starts a
  paragraph where the postcondition of an entity can be described.
- `@return <https://www.doxygen.nl/manual/commands.html#cmdreturn>`_ Single
  paragraph to describe return value(s).
- `@retval <https://www.doxygen.nl/manual/commands.html#cmdretval>`_ Document
  return values by name. This is rendered as a definition list.
- `@note <https://www.doxygen.nl/manual/commands.html#cmdnote>`_ Add a note
  admonition to the end of documentation.
- `@b <https://www.doxygen.nl/manual/commands.html#cmdb>`_ To bold one word.

.. tip:

   Did you add Doxygen comments and now your build is failing because Doxygen
   says it can't find the class you decorated? Make sure your ``@code`` blocks
   are paired with ``@endcode`` blocks and your ``@rst`` blocks are paired
   with ``@endrst`` blocks!

Doxygen provides `structural commands
<https://doxygen.nl/manual/docblocks.html#structuralcommands>`_ that associate a
comment block with a particular symbol. Example of these include ``@class``,
``@struct``, ``@def``, ``@fn``, and ``@file``. Do not use these unless
necessary, since they are redundant with the declarations themselves.

One case where structural commands are necessary is when a single comment block
describes multiple symbols. To group multiple symbols into a single comment
block, include a structural commands for each symbol on its own line. For
example, the following comment documents two macros:

.. code-block:: cpp

   /// @def PW_ASSERT_EXCLUSIVE_LOCK
   /// @def PW_ASSERT_SHARED_LOCK
   ///
   /// Documents functions that dynamically check to see if a lock is held, and
   /// fail if it is not held.

.. seealso:: `All Doxygen commands <https://www.doxygen.nl/manual/commands.html>`_

Cross-references
^^^^^^^^^^^^^^^^
Pigweed provides Doxygen aliases for creating Sphinx cross references within
Doxygen comments. These should be used when referring to other symbols, such as
functions, classes, or macros.

.. inclusive-language: disable

The basic alias is ``@crossref``, which supports any `Sphinx domain
<https://www.sphinx-doc.org/en/master/usage/restructuredtext/domains.html>`_.
For readability, aliases for commonnly used types are provided.

.. inclusive-language: enable

- ``@crossref{domain,type,identifier}`` Inserts a cross reference of any type in
  any Sphinx domain. For example, ``@crossref{c,func,foo}`` is equivalent to
  ``:c:func:`foo``` and links to the documentation for the C function ``foo``,
  if it exists.
- ``@c_macro{identifier}`` Equivalent to ``:c:macro:`identifier```.
- ``@cpp_func{identifier}`` Equivalent to ``:cpp:func:`identifier```.
- ``@cpp_class{identifier}`` Equivalent to ``:cpp:class:`identifier```.
- ``@cpp_type{identifier}`` Equivalent to ``:cpp:type:`identifier```.

.. note::

   Use the `@` aliases described above for all cross references. Doxygen
   provides other methods for cross references, but Sphinx cross references
   offer several advantages:

   - Sphinx cross references work for all identifiers known to Sphinx, including
     those documented with e.g. ``.. cpp:class::`` or extracted from Python.
     Doxygen references can only refer to identifiers known to Doxygen.
   - Sphinx cross references always use consistent formatting. Doxygen cross
     references sometimes render as plain text instead of code-style monospace,
     or include ``()`` in macros that shouldn't have them.
   - Sphinx cross references can refer to symbols that have not yet been
     documented. They will be formatted correctly and become links once the
     symbols are documented.
   - Using Sphinx cross references in Doxygen comments makes cross reference
     syntax more consistent within Doxygen comments and between RST and
     Doxygen.

Create cross reference links elsewhere in the document to symbols documented
with Doxygen using standard Sphinx cross references, such as ``:cpp:class:`` and
``:cpp:func:``.

.. code-block:: rst

   - :cpp:class:`pw::sync::BinarySemaphore::BinarySemaphore`
   - :cpp:func:`pw::sync::BinarySemaphore::try_acquire`

.. seealso::
   - `C++ cross reference link syntax`_
   - `C cross reference link syntax`_
   - `Python cross reference link syntax`_

.. inclusive-language: disable

.. _C++ cross reference link syntax: https://www.sphinx-doc.org/en/master/usage/restructuredtext/domains.html#cross-referencing
.. _C cross reference link syntax: https://www.sphinx-doc.org/en/master/usage/restructuredtext/domains.html#cross-referencing-c-constructs
.. _Python cross reference link syntax: https://www.sphinx-doc.org/en/master/usage/restructuredtext/domains.html#cross-referencing-python-objects

.. inclusive-language: enable

Status codes in Doxygen comments
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Use the following syntax when referring to ``pw_status`` codes in Doxygen
comments:

.. code-block::

   @pw_status{YOUR_STATUS_CODE_HERE}

Replace ``YOUR_STATUS_CODE_HERE`` with a valid ``pw_status`` code.

This syntax ensures that Doxygen links back to the status code's
reference documentation in :ref:`module-pw_status`.

.. note::

   The guidance in this section only applies to Doxygen comments in C++ header
   files.
