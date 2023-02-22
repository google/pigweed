.. _seed-0102:

=====================================
0102: Consistent Module Documentation
=====================================

.. card::
   :fas:`seedling` SEED-0102: :ref:`Consistent Module Documentation<seed-0102>`

   :octicon:`comment-discussion` Status:
   :bdg-secondary-line:`Open for Comments`
   :octicon:`chevron-right`
   :bdg-secondary-line:`Last Call`
   :octicon:`chevron-right`
   :bdg-primary:`Accepted`
   :octicon:`kebab-horizontal`
   :bdg-secondary-line:`Rejected`

   :octicon:`calendar` Proposal Date: 2023-02-10

   :octicon:`code-review` CL: `pwrev/128811 <https://pigweed-review.git.corp.google.com/c/pigweed/pigweed/+/128811>`_

-------
Summary
-------
Pigweed modules ought to have documentation that is reasonably comprehensive,
that has a consistent and predictable format, and that provides the reader with
sufficient information to judge whether using the module is the right choice for
them. This SEED proposes a documentation philosophy applicable to all Pigweed
modules and a flexible yet consistent structure to which all module docs should
conform.

Definitions
-----------
In this SEED, we define *users* as developers using Pigweed in downstream
projects, and *maintainers* as developers working on upstream Pigweed. The
primary focus of this SEED is to improve the documentation experience for users.

----------
Motivation
----------
Currently, each Pigweed module is required to have, at minimum, a single
``docs.rst`` file that contains the module's documentation. This gives the
module maintainer considerable discretion to provide as much or as little
documentation as they would like. However, this approach fails for Pigweed
maintainers by providing no guidance or structure to help them write effective
documentation, and certainly fails Pigweed users who struggle to find the
information they're looking for. So a solution needs to make it easier for
Pigweed maintainers to write good documentation, thereby making Pigweed much
more accessible to its users.

Pigweed's design is inherently and intentionally modular. So documentation at
the level of the *module* is the most natural place to make impactful
improvements, while avoiding a fundamental restructuring of the Pigweed
documentation. Module docs are also what the majority of Pigweed users rely on
most. As a result, this SEED is focused exclusively on improving module
documentation.

`Diátaxis <https://diataxis.fr/>`_ proposes a four-mode framework for technical
documentation, illustrated below with terminology altered to better match
Pigweed's needs:

.. csv-table::
   :widths: 10, 20, 20

   , "**Serve our study**", "**Serve our work**"
   "**Practical steps**", "Codelabs (`learning-oriented <https://diataxis.fr/tutorials/>`_)", "Guides (`task-oriented <https://diataxis.fr/how-to-guides/>`_)"
   "**Theoretical knowledge**", "Concept & design docs (`understanding-oriented <https://diataxis.fr/explanation/>`_)", "Interface reference (`information-oriented <https://diataxis.fr/reference/>`_)"

Pigweed needs a framework that ensures modules have coverage across these four
quadrants. That framework should provide a structure that makes it easier for
maintainers to write effective documentation, and a single page that provides
the most basic information a user needs to understand the module.

Alternatives
------------
There are risks to focusing on module docs:

* The most useful docs are those that focus on tasks rather than system
  features. The module-focused approach risks producing feature-focused docs
  rather than task-focused docs, since the tasks users need to complete may not
  fit within the boundaries of a module.

* Likewise, focusing on module documentation reduces focus on content that
  integrates across multiple modules.

The justification for focusing on module documentation doesn't imply that module
docs are the *only* docs that matter. Higher level introductory and guidance
material that integrates Pigweed as a system and covers cross cutting concerns
is also important, and would arguably be more effective at bringing new
developers into the Pigweed ecosystem. However, this SEED proposes focusing on
module docs for two primary reasons:

1. Improving module docs and providing them with a consistent structure will
   have the largest impact with the least amount of investment.

2. It will be easier to construct higher-level and cross-cutting documentation
   from well-developed module docs compared to going the other direction.

While not a primary consideration, a bonus of a module-focused approach is that
modules already have owners, and those owners are natural candidates to be the
maintainers of their modules' docs.

--------
Proposal
--------
This change would require each module to have a ``docs`` subdirectory matching
this structure::

  docs/
  ├── index.rst
  ├── concepts.rst [or concepts/...] [when needed]
  ├── design.rst [or design/...] [when needed]
  ├── guides.rst [or guides/...] [when needed]
  │
  ├── codelabs/ [aspirational]
  │   ├── index.rst
  │   └── ...
  │
  ├── api.rst [or api/...] [if applicable]
  ├── cli.rst [if applicable]
  └── gui.rst [if applicable]

Fundamental module docs
-----------------------
These three documents are the minimum required of every Pigweed module.

The basics: ``index.rst``
^^^^^^^^^^^^^^^^^^^^^^^^^
Basic, structured information about the module, including what it does, what
problems it's designed solve, and information that lets a user quickly evaluate
if the module is useful to them.

This document replaces any pre-existing ``docs.rst`` file in the module guides
index.

How it works and why: ``design.rst`` & ``concepts.rst`` (understanding-oriented)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Background on the design goals, assumptions, limitations, and implementation
details of a module, and may contrast the design of the module with alternative
solutions.

This content can start in the "Design considerations" section of the index, and
grow into this separate document as the module matures. If that document becomes
too large, the single ``design.rst`` file can be replaced by a ``design``
subdirectory containing more than one nested doc.

Some modules may need documentation on fundamental concepts that are independent
of the module's solution. For example, a module that provides a reliable
transport layer may include a conceptual description of reliable transport in
general in a ``concepts.rst`` file or ``concepts`` subdirectory.

How to get stuff done: ``guides.rst`` (task-oriented)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
These are focused on specific outcomes and should be produced as soon as we see
a question being answered multiple times. Each module should have at least one
guide on integrating the module into a project, and one guide on the most common
use case.

This content can start in the "Getting started" section of the index, and grow
into this separate document as the module matures. If that document becomes too
large, it can be replaced with a ``guides`` subdirectory containing more than
one doc.

Interface docs (information-oriented)
-------------------------------------
These docs describe the module's interfaces. Each of these docs may be omitted
if the module doesn't include an applicable interface.

``api.rst``: External API reference
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Modules should have reference documentation for their user-facing APIs. Modules
that have APIs for multiple languages should replace the single ``api.rst`` with
an ``api`` subdirectory with docs for each supported language.

How API docs should be structured, generated, and maintained is a complex topic
that this SEED will not determine.

``cli.rst`` & ``gui.rst``: Developer tools reference
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
A user-facing command line interface (CLI) should be documented in ``cli.rst``
if the module provides one. It's ideal if this documentation closely matches the
output of the CLI tool's "help" command.

If the module provides a graphical user interface (GUI) (including text mode
interfaces and web front-ends), its documentation should be included in
``gui.rst``.

Codelabs (learning-oriented)
----------------------------
We keep these as separate files in ``codelabs``. These take considerable effort
to develop, so they aren't *required*, but we aspire to develop them for all but
the most trivial modules.

When one size does not fit all
------------------------------
Pigweed modules span a spectrum of complexity, from relatively simple embedded
libraries to sophisticated communication protocols and host-side developer
tooling. The structure described above should be the starting point for each
module's documentation and should be appropriate to the vast majority of
modules. But this proposal is not strictly prescriptive; modules with
documentation needs that are not met by this structure are free to deviate from
it by *adding* docs that are not mentioned here.

Examples
--------
A template for implementing this structure can be found ``docs/templates/docs``.
