.. _docs-pw-style:

===========
Style Guide
===========
- :ref:`docs-pw-style-cpp`
- :ref:`python-style`
- :ref:`docs-pw-style-commit-message`
- :ref:`documentation-style`
- :ref:`owners-style`

.. tip::
   Pigweed runs ``pw format`` as part of ``pw presubmit`` to perform some code
   formatting checks. To speed up the review process, consider adding ``pw
   presubmit`` as a git push hook using the following command:
   ``pw presubmit --install``

.. _cpp-style:

.. _python-style:

------------
Python style
------------
Pigweed uses the standard Python style: PEP8, which is available on the web at
https://www.python.org/dev/peps/pep-0008/. All Pigweed Python code should pass
``pw format``, which invokes ``black`` with a couple options.

Python versions
===============
Pigweed officially supports :ref:`a few Python versions
<docs-concepts-python-version>`. Upstream Pigweed code must support those Python
versions. The only exception is :ref:`module-pw_env_setup`, which must also
support Python 2 and 3.6.

---------------
Build files: GN
---------------
Each Pigweed source module requires a GN build file named BUILD.gn. This
encapsulates the build targets and specifies their sources and dependencies.
GN build files use a format similar to `Bazel's BUILD files
<https://docs.bazel.build/versions/main/build-ref.html>`_
(see the `Bazel style guide
<https://docs.bazel.build/versions/main/skylark/build-style.html>`_).

C/C++ build targets include a list of fields. The primary fields are:

* ``<public>`` -- public header files
* ``<sources>`` -- source files and private header files
* ``<public_configs>`` -- public build configuration
* ``<configs>`` -- private build configuration
* ``<public_deps>`` -- public dependencies
* ``<deps>`` -- private dependencies

Assets within each field must be listed in alphabetical order.

.. code-block:: cpp

  # Here is a brief example of a GN build file.

  import("$dir_pw_unit_test/test.gni")

  config("public_include_path") {
    include_dirs = [ "public" ]
    visibility = [":*"]
  }

  pw_source_set("pw_sample_module") {
    public = [ "public/pw_sample_module/sample_module.h" ]
    sources = [
      "public/pw_sample_module/internal/secret_header.h",
      "sample_module.cc",
      "used_by_sample_module.cc",
    ]
    public_configs = [ ":public_include_path" ]
    public_deps = [ dir_pw_status ]
    deps = [ dir_pw_varint ]
  }

  pw_test_group("tests") {
    tests = [ ":sample_module_test" ]
  }

  pw_test("sample_module_test") {
    sources = [ "sample_module_test.cc" ]
    deps = [ ":sample_module" ]
  }

  pw_doc_group("docs") {
    sources = [ "docs.rst" ]
  }

------------------
Build files: Bazel
------------------
Build files for the Bazel build system must be named ``BUILD.bazel``. Bazel can
interpret files named just ``BUILD``, but Pigweed uses ``BUILD.bazel`` to avoid
ambiguity with other build systems or tooling.

Pigweed's Bazel files follow the `Bazel style guide
<https://docs.bazel.build/versions/main/skylark/build-style.html>`_.

.. _documentation-style:

-------------
Documentation
-------------
.. note::

   Pigweed's documentation style guide came after much of the documentation was
   written, so Pigweed's docs don't yet 100% conform to this style guide. When
   updating docs, please update them to match the style guide.

Pigweed documentation is written using the `reStructuredText
<https://docutils.sourceforge.io/rst.html>`_ markup language and processed by
`Sphinx`_. We use the `Furo theme <https://github.com/pradyunsg/furo>`_ along
with the `sphinx-design <https://sphinx-design.readthedocs.io/en/furo-theme/>`_
extension.

Syntax Reference Links
======================
.. admonition:: See also
   :class: seealso

   - `reStructuredText Primer`_

   - `reStructuredText Directives <https://docutils.sourceforge.io/docs/ref/rst/directives.html>`_

   - `Furo Reference <https://pradyunsg.me/furo/reference/>`_

   - `Sphinx-design Reference <https://sphinx-design.readthedocs.io/en/furo-theme/>`_

ReST is flexible, supporting formatting the same logical document in a few ways
(for example headings, blank lines). Pigweed has the following restrictions to
make our documentation consistent.

Headings
========
Use headings according to the following hierarchy, with the shown characters
for the ReST heading syntax.

.. code-block:: rst

   ==================================
   Document Title: Two Bars of Equals
   ==================================
   Document titles use equals ("====="), above and below. Capitalize the words
   in the title, except for 'a', 'of', and 'the'.

   ---------------------------
   Major Sections Within a Doc
   ---------------------------
   Major sections use hyphens ("----"), above and below. Capitalize the words in
   the title, except for 'a', 'of', and 'the'.

   Heading 1 - For Sections Within a Doc
   =====================================
   These should be title cased. Use a single equals bar ("====").

   Heading 2 - for subsections
   ---------------------------
   Subsections use hyphens ("----"). In many cases, these headings may be
   sentence-like. In those cases, only the first letter should be capitalized.
   For example, FAQ subsections would have a title with "Why does the X do the
   Y?"; note the sentence capitalization (but not title capitalization).

   Heading 3 - for subsubsections
   ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
   Use the caret symbol ("^^^^") for subsubsections.

   Note: Generally don't go beyond heading 3.

   Heading 4 - for subsubsubsections
   .................................
   Don't use this heading level, but if you must, use period characters
   ("....") for the heading.

Do not put blank lines after headings.
--------------------------------------
.. admonition:: **Yes**: No blank after heading
   :class: checkmark

   .. code-block:: rst

      Here is a heading
      -----------------
      Note that there is no blank line after the heading separator!

.. admonition:: **No**: Unnecessary blank line
   :class: error

   .. code-block:: rst

      Here is a heading
      -----------------

      There is a totally unnecessary blank line above this one. Don't do this.

Do not put multiple blank lines before a heading.
-------------------------------------------------
.. admonition:: **Yes**: Just one blank after section content before the next heading
   :class: checkmark

   .. code-block:: rst

      There is some text here in the section before the next. It's just here to
      illustrate the spacing standard. Note that there is just one blank line
      after this paragraph.

      Just one blank!
      ---------------
      There is just one blank line before the heading.

.. admonition:: **No**: Extra blank lines
   :class: error

   .. code-block:: rst

      There is some text here in the section before the next. It's just here to
      illustrate the spacing standard. Note that there are too many blank lines
      after this paragraph; there should be just one.



      Too many blanks
      ---------------
      There are too many blanks before the heading for this section.

Directives
==========
Indent directives 3 spaces; and put a blank line between the directive and the
content. This aligns the directive content with the directive name.

.. admonition:: **Yes**: Three space indent for directives; and nested
   :class: checkmark

   .. code-block:: none

      Here is a paragraph that has some content. After this content is a
      directive.

      .. my_directive::

         Note that this line's start aligns with the "m" above. The 3-space
         alignment accounts for the ".. " prefix for directives, to vertically
         align the directive name with the content.

         This indentation must continue for nested directives.

         .. nested_directive::

            Here is some nested directive content.

.. admonition:: **No**: One space, two spaces, four spaces, or other indents
   for directives
   :class: error

   .. code-block:: none

      Here is a paragraph with some content.

      .. my_directive::

        The indentation here is incorrect! It's one space short; doesn't align
        with the directive name above.

        .. nested_directive::

            This isn't indented correctly either; it's too much (4 spaces).

.. admonition:: **No**: Missing blank between directive and content.
   :class: error

   .. code-block:: none

      Here is a paragraph with some content.

      .. my_directive::
         Note the lack of blank line above here.

Tables
======
Consider using ``.. list-table::`` syntax, which is more maintainable and
easier to edit for complex tables (`details
<https://docutils.sourceforge.io/docs/ref/rst/directives.html#list-table>`_).

Code Snippets
=============
Use code blocks from actual source code files wherever possible. This helps keep
documentation fresh and removes duplicate code examples. There are a few ways to
do this with Sphinx.

The `literalinclude`_ directive creates a code blocks from source files. Entire
files can be included or a just a subsection. The best way to do this is with
the ``:start-after:`` and ``:end-before:`` options.

Example:

.. card::

   Documentation Source (``.rst`` file)
   ^^^

   .. code-block:: rst

      .. literalinclude:: run_doxygen.py
         :start-after: [doxygen-environment-variables]
         :end-before: [doxygen-environment-variables]

.. card::

   Source File
   ^^^

   .. code-block::

      # DOCSTAG: [doxygen-environment-variables]
      env = os.environ.copy()
      env['PW_DOXYGEN_OUTPUT_DIRECTORY'] = str(output_dir.resolve())
      env['PW_DOXYGEN_INPUT'] = ' '.join(pw_module_list)
      env['PW_DOXYGEN_PROJECT_NAME'] = 'Pigweed'
      # DOCSTAG: [doxygen-environment-variables]

.. card::

   Rendered Output
   ^^^

   .. code-block::

      env = os.environ.copy()
      env['PW_DOXYGEN_OUTPUT_DIRECTORY'] = str(output_dir.resolve())
      env['PW_DOXYGEN_INPUT'] = ' '.join(pw_module_list)
      env['PW_DOXYGEN_PROJECT_NAME'] = 'Pigweed'

Generating API documentation from source
========================================
Whenever possible, document APIs in the source code and use Sphinx to generate
documentation for them. This keeps the documentation in sync with the code and
reduces duplication.

Python
------
Include Python API documentation from docstrings with `autodoc directives`_.
Example:

.. code-block:: rst

   .. automodule:: pw_cli.log
      :members:

   .. automodule:: pw_console.embed
      :members: PwConsoleEmbed
      :undoc-members:
      :show-inheritance:

   .. autoclass:: pw_console.log_store.LogStore
      :members: __init__
      :undoc-members:
      :show-inheritance:

Include argparse command line help with the `argparse
<https://sphinx-argparse.readthedocs.io/en/latest/usage.html>`_
directive. Example:

.. code-block:: rst

   .. argparse::
      :module: pw_watch.watch
      :func: get_parser
      :prog: pw watch
      :nodefaultconst:
      :nodescription:
      :noepilog:

.. _documentation-style-doxygen:

Doxygen
-------
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

.. _Sphinx: https://www.sphinx-doc.org/

.. inclusive-language: disable

.. _reStructuredText Primer: https://www.sphinx-doc.org/en/master/usage/restructuredtext/basics.html
.. _literalinclude: https://www.sphinx-doc.org/en/master/usage/restructuredtext/directives.html#directive-literalinclude
.. _C++ cross reference link syntax: https://www.sphinx-doc.org/en/master/usage/restructuredtext/domains.html#cross-referencing
.. _C cross reference link syntax: https://www.sphinx-doc.org/en/master/usage/restructuredtext/domains.html#cross-referencing-c-constructs
.. _Python cross reference link syntax: https://www.sphinx-doc.org/en/master/usage/restructuredtext/domains.html#cross-referencing-python-objects
.. _autodoc directives: https://www.sphinx-doc.org/en/master/usage/extensions/autodoc.html#directives

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

Customize the depth of a page's table of contents
=================================================
Put ``:tocdepth: X`` on the first line of the page, where ``X`` equals how many
levels of section heading you want to show in the page's table of contents. See
``//docs/changelog.rst`` for an example.

Changelog
=========

This section explains how we update the changelog.

#. On the Friday before Pigweed Live, use
   `changelog <https://kaycebasques.github.io/changelog/>`_ to generate a first
   draft of the changelog.

#. Copy-paste the reStructuredText output from the changelog tool to the top
   of ``//docs/changelog.rst``.

#. Delete these lines from the previous update in ``changelog.rst``
   (which is no longer the latest update):

   * ``.. _docs-changelog-latest:``
   * ``.. changelog_highlights_start``
   * ``.. changelog_highlights_end``

#. Polish up the auto-generated first draft into something more readable:

   * Don't change the section headings. The text in each section heading
     should map to one of the categories that we allow in our commit
     messages, such as ``bazel``, ``docs``, ``pw_base64``, and so on.
   * Add a 1-paragraph summary to each section.
   * Focus on features, important bug fixes, and breaking changes. Delete
     internal commits that Pigweed customers won't care about.

#. Push your change up to Gerrit and kick off a dry run. After a few minutes
   the docs will get staged.

#. Copy the rendered content from the staging site into the Pigweed Live
   Google Doc.

#. Make sure to land the changelog updates the same week as Pigweed Live.

There is no need to update ``//docs/index.rst``. The ``What's new in Pigweed``
content on the homepage is pulled from the changelog (that's what the
``docs-changelog-latest``, ``changelog_highlights_start``, and
``changelog_highlights_end`` labels are for).

Why "changelog" and not "release notes"?
----------------------------------------
Because Pigweed doesn't have releases.

Why organize by module / category?
----------------------------------
Why is the changelog organized by category / module? Why not the usual
3 top-level sections: features, fixes, breaking changes?

* Because some Pigweed customers only use a few modules. Organizing by module
  helps them filter out all the changes that aren't relevant to them faster.
* If we keep the changelog section heading text fairly structured, we may
  be able to present the changelog in other interesting ways. For example,
  it should be possible to collect every ``pw_base64`` section in the changelog
  and then provide a changelog for only ``pw_base64`` over in the ``pw_base64``
  docs.
* The changelog tool is easily able to organize by module / category due to
  how we annotate our commits. We will not be able to publish changelog updates
  every 2 weeks if there is too much manual work involved.

.. _docs-site-scroll:

Site nav scrolling
==================
We have had recurring issues with scrolling on pigweed.dev. This section
provides context on the issue and fix(es).

The behavior we want:

* The page that you're currently on should be visible in the site nav.
* URLs with deep links (e.g. ``pigweed.dev/pw_allocator/#size-report``) should
  instantly jump to the target section (e.g. ``#size-report``).
* There should be no scrolling animations anywhere on the site. Scrolls should
  happen instantly.

.. _furo.js: https://github.com/pradyunsg/furo/blob/main/src/furo/assets/scripts/furo.js

A few potential issues at play:

* Our theme (Furo) has non-configurable scrolling logic. See `furo.js`_.
* There seems to be a race condition between Furo's scrolling behavior and our
  text-to-diagram tool, Mermaid, which uses JavaScript to render the diagrams
  on page load. However, we also saw issues on pages that didn't have any
  diagrams, so that can't be the site-wide root cause.

.. _scrollTop: https://developer.mozilla.org/en-US/docs/Web/API/Element/scrollTop
.. _scrollIntoView: https://developer.mozilla.org/en-US/docs/Web/API/Element/scrollIntoView
.. _scroll-behavior: https://developer.mozilla.org/en-US/docs/Web/CSS/scroll-behavior

Our current fix:

* In ``//docs/_static/js/pigweed.js`` we manually scroll the site nav and main
  content via `scrollTop`_. Note that we previously tried `scrollIntoView`_
  but it was not an acceptable fix because the main content would always scroll
  down about 50 pixels, even when a deep link was not present in the URL.
  We also manually control when Mermaid renders its diagrams.
* In ``//docs/_static/css/pigweed.css`` we use an aggressive CSS rule
  to ensure that `scroll-behavior`_ is set to ``auto`` (i.e. instant scrolling)
  for all elements on the site.

Background:

* `Tracking issue <https://issues.pigweed.dev/issues/303261476>`_
* `First fix <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/162410>`_
* `Second fix <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/162990>`_
* `Third fix <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/168555>`_
* `Fourth fix <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/178591>`_

.. _docs-pw-style-cta:

Call-to-action buttons on sales pitch pages (docs.rst)
======================================================
Use the following directive to put call-to-action buttons on a ``docs.rst``
page:

.. code-block::

   .. grid:: 2

      .. grid-item-card:: :octicon:`zap` Get started & guides
         :link: <REF>
         :link-type: ref
         :class-item: sales-pitch-cta-primary

         Learn how to integrate <MODULE> into your project and implement
         common use cases.

      .. grid-item-card:: :octicon:`info` API reference
         :link: <REF>
         :link-type: ref
         :class-item: sales-pitch-cta-secondary

         Get detailed reference information about the <MODULE> API.

   .. grid:: 2

      .. grid-item-card:: :octicon:`info` CLI reference
         :link: <REF>
         :link-type: ref
         :class-item: sales-pitch-cta-secondary

         Get usage information about <MODULE> command line utilities.

      .. grid-item-card:: :octicon:`table` Design
         :link: <REF>
         :link-type: ref
         :class-item: sales-pitch-cta-secondary

         Read up on how <MODULE> is designed.

* Remove cards for content that does not exist. For example, if the module
  doesn't have a CLI reference, remove the card for that doc.
* Replace ``<REF>`` and ``<MODULE>``. Don't change anything else. We want
  a consistent call-to-action experience across all the modules.

Copy-to-clipboard feature on code blocks
========================================

.. _sphinx-copybutton: https://sphinx-copybutton.readthedocs.io/en/latest/
.. _Remove copybuttons using a CSS selector: https://sphinx-copybutton.readthedocs.io/en/latest/use.html#remove-copybuttons-using-a-css-selector

The copy-to-clipboard feature on code blocks is powered by `sphinx-copybutton`_.

``sphinx-copybutton`` recognizes ``$`` as an input prompt and automatically
removes it.

There is a workflow for manually removing the copy-to-clipboard button for a
particular code block but it has not been implemented yet. See
`Remove copybuttons using a CSS selector`_.

Grouping related content with tabs
==================================
Use the ``tab-set`` directive to group related content together. This feature is
powered by `sphinx-design Tabs
<https://sphinx-design.readthedocs.io/en/furo-theme/tabs.html>`_

Tabs for code-only content
--------------------------
Use the ``tabs`` and ``code-tab`` directives together. Example:

.. code-block:: rst

   .. tab-set-code::

      .. code-block:: c++

         // C++ code...

      .. code-block:: python

         # Python code...

Rendered output:

.. tab-set-code::

   .. code-block:: c++

      // C++ code...

   .. code-block:: python

      # Python code...

Tabs for all other content
--------------------------
Use the ``tabs`` and ``tab-item`` directives together. Example:

.. code-block:: rst

   .. tab-set::

      .. tab-item:: Linux

         Linux instructions...

      .. tab-item:: Windows

         Windows instructions...

Rendered output:

.. tab-set::

   .. tab-item:: Linux

      Linux instructions...

   .. tab-item:: Windows

      Windows instructions...

Tab synchronization
-------------------
Tabs are synchronized in two ways:

1. ``tab-set-code::`` ``code-block`` languages names.
2. ``tab-item::`` ``:sync:`` values.

For Example:

.. code-block:: rst

   .. tabs-set-code::

      .. code-block:: c++

         // C++ code...

      .. code-block:: py

         # Python code...

   .. tabs-set-code::

      .. code-block:: c++

         // More C++ code...

      .. code-block:: py

         # More Python code...

   .. tab-set::

      .. tab-item:: Linux
         :sync: key1

         Linux instructions...

      .. tab-item:: Windows
         :sync: key2

         Windows instructions...

   .. tab-set::

      .. tab-item:: Linux
         :sync: key1

         More Linux instructions...

      .. tab-item:: Windows
         :sync: key2

         More Windows instructions...

Rendered output:

.. tab-set-code::

   .. code-block:: c++

      // C++ code...

   .. code-block:: py

      # Python code...

.. tab-set-code::

   .. code-block:: c++

      // More C++ code...

   .. code-block:: py

      # More Python code...

.. tab-set::

   .. tab-item:: Linux
      :sync: key1

      Linux instructions...

   .. tab-item:: Windows
      :sync: key2

      Windows instructions...

.. tab-set::

   .. tab-item:: Linux
      :sync: key1

      More Linux instructions...

   .. tab-item:: Windows
      :sync: key2

      More Windows instructions...

.. _owners-style:

--------------------
Code Owners (OWNERS)
--------------------
If you use Gerrit for source code hosting and have the
`code-owners <https://android-review.googlesource.com/plugins/code-owners/Documentation/backend-find-owners.html>`__
plug-in enabled Pigweed can help you enforce consistent styling on those files
and also detects some errors.

The styling follows these rules.

#. Content is grouped by type of line (Access grant, include, etc).
#. Each grouping is sorted alphabetically.
#. Groups are placed the following order with a blank line separating each
   grouping.

   * "set noparent" line
   * "include" lines
   * "file:" lines
   * user grants (some examples: "*", "foo@example.com")
   * "per-file:" lines

This plugin will, by default, act upon any file named "OWNERS".

.. toctree::
   :hidden:

   C++ <style/cpp>
   Commit message <style/commit_message>
