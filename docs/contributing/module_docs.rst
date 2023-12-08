.. _docs-contrib-moduledocs:

======================
Module Docs Guidelines
======================
This page provides guidelines on how to write documentation for a single
Pigweed module.

These guidelines are a work-in-progress. Most of the guidelines are
recommendations, not requirements, because we're still figuring out which
guidelines are truly universal to all modules and which ones only work for
certain types of modules.

---------------------
Choose your adventure
---------------------
Our guidance for you changes depending on the nature of the module you're
documenting. This flowchart summarizes the decision path:

.. mermaid::

   flowchart TD
     A[Start] --> B{Simple module or complex?}
     B -->|Complex| C[Talk to the Pigweed team]
     B -->|Simple| D{A little content or a lot?}
     D -->|A little| E[Use the single-page approach]
     D -->|A lot| F[Use the multi-page approach]

.. _docs-contrib-moduledocs-moduletypes:

-------------------------------------
Simple modules versus complex modules
-------------------------------------
We're currently focused on updating the docs for "simple" modules. Here's our
general definition for a simple module:

* The module's API is end-user-facing or HAL-facing, not
  infrastructure-facing.
* The module does not use :ref:`facades <docs-glossary-facade>`.
* The module's API surface is small or medium in size.
* The module's API is in a single programming language.

If your module doesn't meet these criteria, it's probably a "complex" module.
`Create an issue <https://issues.pigweed.dev/issues/new>`_ before attempting
to refactor a complex module's docs.

.. note::

   Why the focus on simple modules? We tried refactoring complex modules to
   adhere to :ref:`SEED-0102 <seed-0102>`. The migrations were difficult and
   the resulting docs weren't an obvious improvement. We learned that it's more
   effective to focus on simple modules for now and take more time to figure out
   what works for complex modules.

Examples of simple modules:

* :ref:`module-pw_string`
* :ref:`module-pw_i2c`

Examples of complex modules:

* :ref:`module-pw_tokenizer`

.. _docs-contrib-moduledocs-simple:

------------------------
Simple module guidelines
------------------------
Follow these guidelines if you're writing docs for a
:ref:`simple module <docs-contrib-moduledocs-moduletypes>`.

Single-page approach versus multi-page approach
===============================================
If your module meets the following criteria then you should *probably* use
the :ref:`docs-contrib-moduledocs-singlepage`:

* There is less than 1000 words of content in total.
* The API has 2 classes or less.
* The API has 10 methods or less.

If your module doesn't meet all these criteria, then you should *probably*
use the :ref:`docs-contrib-moduledocs-multipage`. As you can tell by our use of
*probably*, this is just a soft guideline. E.g. if you have 2000 words of
content but you feel strongly that the single-page approach is better for your
module, then go for it!

The content that you write mostly stays the same whether you use the single-page
or multi-page approach. All modules must have
:ref:`docs-contrib-moduledocs-sales` content for example. The only
difference is that in the single-page approach this is the first *section* of
content whereas in the multi-page approach it's the first *page* of content.

.. _docs-contrib-moduledocs-singlepage:

Single-page approach
====================
When using the single-page approach, this is the default ordering of
sections in ``docs.rst``:

* :ref:`docs-contrib-moduledocs-sales`
* :ref:`docs-contrib-moduledocs-getstarted`
* :ref:`docs-contrib-moduledocs-guides`
* :ref:`docs-contrib-moduledocs-reference`
* :ref:`docs-contrib-moduledocs-design`
* :ref:`docs-contrib-moduledocs-roadmap`
* :ref:`docs-contrib-moduledocs-size`

The sales pitch must come first, followed by the getting started instructions.
Everything else beyond that is optional. The sections can be re-arranged if
you feel strongly about it, but we've found this is an intuitive ordering.

The file must be located at ``//pw_<name>/docs.rst``, where ``<name>`` is
replaced with the actual name of your module.

Examples:

* :ref:`module-pw_alignment`
* :ref:`module-pw_perf_test`

.. _docs-contrib-moduledocs-multipage:

Multi-page approach
===================
When using the multi-page approach, this is the default ordering of
pages:

.. list-table::
   :header-rows: 1

   * - Page Title
     - Filename
     - Description
   * - ``pw_<name>``
     - ``docs.rst``
     - The :ref:`docs-contrib-moduledocs-sales` content.
   * - ``Get Started & Guides``
     - ``guides.rst``
     - The :ref:`docs-contrib-moduledocs-getstarted` content followed by the
       :ref:`docs-contrib-moduledocs-guides` content. See the note below.
   * - ``API Reference``
     - ``api.rst``
     - The :ref:`docs-contrib-moduledocs-reference` content.
   * - ``Design & Roadmap``
     - ``design.rst``
     - The :ref:`docs-contrib-moduledocs-design` content. See the note below.
   * - ``Code Size Analysis``
     - ``size.rst``
     - The :ref:`docs-contrib-moduledocs-size` content.

The sales pitch and getting started instructions are required. Everything else
is optional. The sections can be re-arranged if you feel strongly about it,
but we've found that this is an intuitive ordering.

You can split ``Get Started & Guides`` into 2 docs if that works better for
your module. The filenames should be ``get_started.rst`` and ``guides.rst``.

``Design & Roadmap`` can also be split into 2 docs. The filenames should be
``design.rst`` and ``roadmap.rst``.

Link each doc to all other docs in the set
------------------------------------------
Each doc must link to all other docs in the set. Here's how we currently do it.
We may adjust our approach in the future if we find a better solution.

#. Create a grid of nav cards at the bottom of ``docs.rst``. Example:

   .. code-block::

      .. pw_<name>-nav-start

      .. grid:: 1

         .. grid-item-card:: :octicon:`rocket` Get started & guides
            :link: module-pw_emu-guide
            :link-type: ref
            :class-item: sales-pitch-cta-primary

            How to set up and use ``pw_<name>``

      .. grid:: 2

         .. grid-item-card:: :octicon:`terminal` CLI reference
            :link: module-pw_emu-cli
            :link-type: ref
            :class-item: sales-pitch-cta-secondary

            Reference details about the ``pw_<name>`` command line interface

         .. grid-item-card:: :octicon:`code-square` API reference
            :link: module-pw_emu-api
            :link-type: ref
            :class-item: sales-pitch-cta-secondary

            Reference details about the ``pw_<name>`` Python API

      .. pw_<name>-nav-end

   Note the ``.. pw_<name>-nav-start`` and ``.. pw_<name>-nav-end`` comments.
   You'll use these in the next step.

#. Add the following H2 section to the bottom of every other doc in the set.

   .. code-block::

      -------------------
      More pw_<name> docs
      -------------------
      .. include:: docs.rst
         :start-after: .. pw_<name>-nav-start
         :end-before: .. pw_<name>-nav-end

Examples:

* :ref:`module-pw_emu`

------------------
Content guidelines
------------------
The following sections provide instructions on how to write each content type.

.. note::

   We call them "content types" because in the
   :ref:`docs-contrib-moduledocs-singlepage` each of these things represent a
   section of content on ``docs.rst`` whereas in the
   :ref:`docs-contrib-moduledocs-multipage` they might be an entire page of
   content or a section within a page.

.. _docs-contrib-moduledocs-sales:

Sales pitch
===========
The sales pitch should:

* Assume that the reader is an embedded developer.
* Clearly explain how the reader's work as an embedded developer
  will improve if they adopt the module.
* Provide a code sample demonstrating one of the most important
  problems the module solves. (Only required for modules that expose
  an API.)

Examples:

* :ref:`module-pw_string`
* :ref:`module-pw_tokenizer`

.. _docs-contrib-moduledocs-getstarted:

Get started
===========
The get started instructions should:

* Show how to get set up in Bazel, GN, and CMake.
* Present Bazel instructions first.
* Clearly state when a build system isn't supported.
* Format the instructions with the ``.. tab-set::`` directive. See
  ``//pw_string/guide.rst`` for an example. The Bazel instructions are
  presented in the first tab, the GN instructions in the next, and so on.
* Demonstrate how to complete a common use case. See the next paragraph.

If your get started content is on the same page as your guides, then the get
started section doesn't need to demonstrate a common use case. The reader can
just scroll down and see how to complete common tasks. If your get started
content is a standalone page, it should demonstrate how to complete a common
task. The reader shouldn't have to dig around multiple docs just to figure out
how to do something useful with the module.

Examples:

* :ref:`module-pw_string-get-started` (pw_string)

.. _docs-contrib-moduledocs-guides:

Guides
======
The guides should:

* Focus on how to solve real-world problems with the module. See
  `About how-to guides <https://diataxis.fr/how-to-guides/>`_.

Examples:

* :ref:`module-pw_string-guide-stringbuilder`

.. _docs-contrib-moduledocs-reference:

API reference
=============
The API reference should:

* Be auto-generated from :ref:`docs-pw-style-doxygen` (for C++ / C APIs) or
  autodoc (for Python APIs).
* Provide a code example demonstrating how to use the class, at minimum.
  Consider whether it's also helpful to provide more granular examples
  demonstrating how to use each method, variable, etc.

The typical approach is to order everything alphabetically. Some module docs
group classes logically according to the tasks they're related to. We don't
have a hard guideline here because we're not sure one of these approaches is
universally better than the other.

Examples:

* :ref:`module-pw_string-api` (pw_string)
* :ref:`module-pw_tokenizer-api` (pw_tokenizer)

.. _docs-contrib-moduledocs-design:

Design
======
The design content should:

* Focus on `theory of operation <https://en.wikipedia.org/wiki/Theory_of_operation>`_
  or `explanation <https://diataxis.fr/explanation/>`_.

Examples:

* :ref:`module-pw_string-design-inlinestring` (pw_string)

.. _docs-contrib-moduledocs-roadmap:

Roadmap
=======
The roadmap should:

* Focus on things known to be missing today that could make sense in the
  future. The reader should be encouraged to talk to the Pigweed team.

The roadmap should not:

* Make very definite guarantees that a particular feature will ship by a
  certain date. You can get an exception if you really need to do this, but
  it should be avoided in most cases.

Examples:

* :ref:`module-pw_string-roadmap` (pw_string)

.. _docs-contrib-moduledocs-size:

Size analysis
=============
The size analysis should:

* Be auto-generated. See the ``pw_size_diff`` targets in ``//pw_string/BUILD.gn``
  for examples.

We elevate the size analysis to its own section or page because it's a very
important consideration for many embedded developers.

Examples:

* :ref:`module-pw_string-size-reports` (pw_string)
