.. _docs-style-rest:

============================
reStructuredText style guide
============================
.. inclusive-language: disable
.. _reStructuredText: https://docutils.sourceforge.io/rst.html
.. _Sphinx: https://www.sphinx-doc.org/en/master/
.. inclusive-language: enable

This style guide defines Pigweed's conventions for `reStructuredText`_ (reST).
``pigweed.dev`` documentation is authored in reST, not Markdown. Pigweed
uses `Sphinx`_ to convert reST into HTML.

.. tip:: ``pw format`` detects reST style issues.

.. _docs-style-rest-overview:

--------
Overview
--------

.. _docs-style-rest-scope:

Scope
=====
This style guide applies to all reST markup that's intended to be published to
``pigweed.dev``. reST markup is mainly found in ``.rst`` files but it's also
possible to embed reST within other types of files.

.. _docs-style-rest-other:

Related style guides
====================
This style guide is narrowly focused on Pigweed's usage of reStructuredText.
See :ref:`docs-contrib-docs` for other aspects of Pigweed documentation style.

.. _docs-style-rest-headings:

--------
Headings
--------
Use the following syntax for headings.

.. code-block:: rst

   ==================
   H1: Document title
   ==================
   In HTML the heading above is rendered as an H1 node. A page must have
   exactly one H1 node. The H1 text must have equals signs (``=``) above
   and below it.

   ------------
   H2: Sections
   ------------
   In HTML the heading above is rendered as an H2 node. The H2 text must have
   minus signs (``-``) above and below it. H2 headings are conceptually similar
   to chapters in a book.

   H3: Subsections
   ===============
   In HTML the heading above is rendered as an H3 node. The H3 text must
   have equals signs (``=``) below it. H3 headings are conceptually similar to
   sections within a chapter of a book.

   H4: Subsubsections
   ------------------
   In HTML the heading above is rendered as an H4 node. The H4 text must have
   minus signs (``-``) below it. H4 headings are used rarely.

   H5: Subsubsubsections
   ^^^^^^^^^^^^^^^^^^^^^
   In HTML the heading above is rendered as an H5 node. The H5 text must have
   caret symbols (``^``) below it. H5 headings are used very rarely.

   H6: Subsubsubsubsections
   ........................
   In HTML the heading above is rendered as an H6 node. The H6 text must have
   period symbols (``.``) below it. H6 headings are practically never used and
   are an indicator that a document probably needs refactoring.

.. _docs-style-rest-headings-order:

Heading levels
==============
Headings must be used in hierarchical order. No level can be skipped. For
example, you can't use an H1 heading followed by an H3. See
`Headings and titles <https://developers.google.com/style/accessibility#headings-and-titles>`_.

.. _docs-style-rest-headings-whitespace-after:

No blank lines after headings
=============================
Don't put whitespace between a heading and the content that follows it.

.. admonition:: **Yes**
   :class: checkmark

   .. code-block:: rst

      Example heading
      ===============
      This paragraph is positioned correctly.

.. admonition:: **No**
   :class: error

   .. code-block:: rst

      Example heading
      ===============

      This paragraph isn't positioned correctly. There shouldn't be a blank
      line above it.

.. _docs-style-rest-headings-whitespace-before:

One blank line before a heading
===============================
Put one blank line between a heading and the content that comes before it.

.. admonition:: **Yes**
   :class: checkmark

   .. code-block:: rst

      This paragraph is positioned correctly.

      Example heading
      ---------------
      ...

.. admonition:: **No**
   :class: error

   .. code-block:: rst

      This paragraph isn't positioned correctly. There's too much whitespace
      below it.



      Example heading
      ---------------
      ...

.. _docs-style-rest-directives:

----------
Directives
----------
Indent directive content and attributes 3 spaces so that they align
with the directive name.

.. admonition:: **Yes**
   :class: checkmark

   .. code-block:: rst

      .. my-directive::
         :my-attr: This attribute is positioned correctly.

         This paragraph is positioned correctly.

         .. my-nested-directive::

            This paragraph is positioned correctly.

.. admonition:: **No**
   :class: error

   .. code-block:: rst

      .. my-directive::

        This paragraph isn't positioned correctly. It has too few spaces.

      .. my-directive::

          This paragraph isn't positioned correctly. It has too many spaces.

Put a blank line between the directive and its content. Don't put space
between a directive name and its attributes.

.. admonition:: **Yes**
   :class: checkmark

   .. code-block:: rst

      .. my-directive::
         :my-attr: This attribute is positioned correctly.

         This paragraph is positioned correctly.

.. admonition:: **No**
   :class: error

   .. code-block:: rst

      .. my-directive::
         This paragraph isn't positioned correctly. There should be a blank
         line above it.

.. _docs-style-rest-tables:

------
Tables
------
.. _csv-table: https://docutils.sourceforge.io/docs/ref/rst/directives.html#csv-table-1
.. _list-table: https://docutils.sourceforge.io/docs/ref/rst/directives.html#list-table
.. _table: https://docutils.sourceforge.io/docs/ref/rst/directives.html#table

Prefer `list-table`_ and `csv-table`_. Avoid `table`_. ``list-table`` and
``csv-table`` are easier to maintain.

.. _docs-style-rest-includes:

-------------
Code includes
-------------
Prefer including code snippets from actual source code files that are built
and tested regularly.

To include a portion of a file:

.. code-block:: rst

   .. literalinclude:: my_source_file.py
      :start-after: my-start-text
      :end-before: my-end-text

``mw-start-text`` and ``my-end-text`` are the exclusive delimiters that must
appear verbatim in the source file.

.. _docs-style-rest-includes-python:

Python code includes
====================
.. inclusive-language: disable
.. _autodoc: https://www.sphinx-doc.org/en/master/usage/extensions/autodoc.html#directives
.. inclusive-language: enable

Use `autodoc`_.

Python code includes for CLI args
---------------------------------
Use `argparse <https://sphinx-argparse.readthedocs.io/en/latest/usage.html>`_.

.. _docs-style-rest-code-blocks:

-----------
Code blocks
-----------
.. _Languages: https://pygments.org/languages/
.. inclusive-language: disable
.. _code-block: https://www.sphinx-doc.org/en/master/usage/restructuredtext/directives.html#directive-code-block
.. inclusive-language: enable

Use `code-block`_. See `Languages`_ for the list of valid language
keywords.

If Google has a style guide for the programming language in your code block,
your code should match Google's style guide.

.. code-block:: rst

   .. code-block:: c++

      // ...

.. _docs-style-rest-toc:

-------------------------------------
Table of contents depth customization
-------------------------------------
Put ``:tocdepth: X`` on the first line of the page, where ``X`` equals how many
levels of section heading you want to show in the page's table of contents. See
``//docs/changelog.rst`` for an example.

.. _docs-style-rest-cta:

----------------------
Call-to-action buttons
----------------------
Use ``grid`` and ``grid-item-card``.

.. code-block::

   .. grid:: 2

      .. grid-item-card:: :octicon:`<ICON>` <TITLE>
         :link: <REF>
         :link-type: <TYPE>
         :class-item: <CLASS>

         <DESCRIPTION>

.. _Octicons: https://primer.style/foundations/icons
.. inclusive-language: disable
.. _ref: https://www.sphinx-doc.org/en/master/usage/referencing.html#ref-role
.. inclusive-language: enable

* See `Octicons`_ for the list of valid ``<ICON>`` values.
* ``<REF>`` can either be a `ref`_ or a full URL.
* ``<TYPE>`` must be either ``ref`` or ``url``.
* ``<CLASS>`` must be either ``sales-pitch-cta-primary`` or
  ``sales-pitch-cta-secondary``. The top-left or top card should use
  ``sales-pitch-cta-primary``; all the others should use
  ``sales-pitch-cta-secondary``. The motivation is to draw extra attention to
  the primary card.
* ``<DESCRIPTION>`` should be reStructuredText describing what kind of
  content you'll see if you click the card.

.. _docs-style-rest-tabs:

----
Tabs
----
.. _Tabs: https://sphinx-design.readthedocs.io/en/furo-theme/tabs.html

Use ``tab-set`` and ``tab-item``. See `Tabs`_.

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

.. _docs-style-rest-tabs-code:

Tabs for code-only content
==========================
Use ``tab-set-code``. See `Languages`_ for the list of
valid language keywords.

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

.. _docs-style-rest-tabs-sync:

Tab synchronization
===================
Use the ``:sync:`` attribute to synchronize tabs for non-code content. See
**Rendered output** below for an example.

``tab-set-code`` tabs automatically synchronize by code language.

.. code-block:: rst

   .. tab-set::

      .. tab-item:: Linux
         :sync: linux

         Linux instructions...

      .. tab-item:: Windows
         :sync: windows

         Windows instructions...

   .. tab-set::

      .. tab-item:: Linux
         :sync: linux

         More Linux instructions...

      .. tab-item:: Windows
         :sync: windows

         More Windows instructions...

Rendered output:

.. tab-set::

   .. tab-item:: Linux
      :sync: linux

      Linux instructions...

   .. tab-item:: Windows
      :sync: windows

      Windows instructions...

.. tab-set::

   .. tab-item:: Linux
      :sync: linux

      More Linux instructions...

   .. tab-item:: Windows
      :sync: windows

      More Windows instructions...
