.. _docs-pw-style-sphinx:

==========================
Sphinx documentation style
==========================
.. note::

   Pigweed's documentation style guide came after much of the documentation was
   written, so Pigweed doesn't entirely conform to our own style guide. When
   updating docs, please update them to match the style guide.

.. note::

   We are moving to the `Google Developer Documentation Style Guide (GDDSG)
   <https://developers.google.com/style>`_ for the English language conventions
   (rather than technical style for RST usage, etc). Currently, most of our
   documentation does not adhere.

Pigweed documentation is written using the `reStructuredText
<https://docutils.sourceforge.io/rst.html>`_ markup language and processed by
`Sphinx`_. We use the `Furo theme <https://github.com/pradyunsg/furo>`_ along
with the `sphinx-design <https://sphinx-design.readthedocs.io/en/furo-theme/>`_
extension.

.. _Sphinx: https://www.sphinx-doc.org/

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

.. inclusive-language: disable

.. _reStructuredText Primer: https://www.sphinx-doc.org/en/master/usage/restructuredtext/basics.html

.. inclusive-language: enable

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

.. inclusive-language: disable

.. _literalinclude: https://www.sphinx-doc.org/en/master/usage/restructuredtext/directives.html#directive-literalinclude

.. inclusive-language: enable

Generating API documentation from source
========================================
Whenever possible, document APIs in the source code and use Sphinx to generate
documentation for them. This keeps the documentation in sync with the code and
reduces duplication.

Python
------
Include Python API documentation from docstrings with `autodoc directives`_.
Example:

.. inclusive-language: disable

.. _autodoc directives: https://www.sphinx-doc.org/en/master/usage/extensions/autodoc.html#directives

.. inclusive-language: enable

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

Auto-generated source code and issues URLS
==========================================
In the site nav there's a ``Source code`` and ``Issues`` URL for each module.
These links are auto-generated. The auto-generation logic lives in
``//pw_docgen/py/pw_docgen/sphinx/module_metadata.py``.

Breadcrumbs
===========
.. _breadcrumbs: https://en.wikipedia.org/wiki/Breadcrumb_navigation

The `breadcrumbs`_ at the top of each page (except the homepage) is implemented
in ``//docs/layout/page.html``. The CSS for this UI is in
``//docs/_static/css/pigweed.css`` under the ``.breadcrumbs`` and
``.breadcrumb`` classes.
