.. _blog-08-bazel-docgen:

===============================================
Pigweed Blog #8: Migrating pigweed.dev to Bazel
===============================================
*Published on 2025-02-03 by Kayce Basques*

``pigweed.dev`` is now built with Bazel! This blog post covers:

* :ref:`blog-08-bazel-docgen-why`
* :ref:`blog-08-bazel-docgen-compare`
* :ref:`blog-08-bazel-docgen-good`
* :ref:`blog-08-bazel-docgen-challenges`
* :ref:`blog-08-bazel-docgen-next`

.. important::

   Pigweed still supports GN and will continue to do so for a long time. This
   post merely discusses how we migrated the build for our own docs site
   (``pigweed.dev``) from GN to Bazel.

.. _blog-08-bazel-docgen-why:

---------------------------------
Why pigweed.dev switched to Bazel
---------------------------------
.. _hermetic: https://bazel.build/basics/hermeticity

Back in October 2023, Pigweed adopted Bazel as its :ref:`primary build system
<seed-0111>`. For the first 12 months or so, the team focused on critical
embedded development features like automated toolchain setup and `hermetic`_
cross-platform builds.  Most of that work is done now so we shifted our focus
to the last major part of Pigweed not using Bazel: our docs.

.. note::

   "Hermeticity" essentially means that the build system runs in an isolated,
   reproducible environment to guarantee that the build always produces the
   exact same outputs for all teammates. It's one of Bazel's key value
   propositions.

.. _blog-08-bazel-docgen-compare:

-------------------------------------------------
Comparing the new Bazel build to the old GN build
-------------------------------------------------
We eventually ended up with this architecture for the new Bazel-based docs
build system:

.. mermaid::

   flowchart LR

     Doxygen --> Breathe
     Breathe --> reST
     reST --> Sphinx
     Rust --> Sphinx
     Python --> Sphinx

The GN build had roughly the same architecture, but the architecture is
much more explicit and well-defined now.

Here's an overview of what each component does and how they connect together:

* C/C++ API references. We auto-generate our C/C++ API references with
  **Doxygen** and then use **Breathe** to insert the reference content
  into our main docs files, which are authored in **reST** (reStructuredText).
  Example: :ref:`pw_unit_test C++ API reference <module-pw_unit_test-cpp>`

* Rust API references. We auto-generate our Rust API references with
  ``rustdoc`` and upload these docs to their own subsite.
  Example: `Crate pw_bytes <https://pigweed.dev/rustdoc/pw_bytes/>`_

* reST and Sphinx. We author most of our docs in **reST** (reStructuredText)
  and convert them into HTML with **Sphinx**. This is the heart of our
  docs system. Example: :ref:`Tour of Pigweed <showcase-sense-tutorial-intro>`

* Python API references. We auto-generate our Python API references with
  Sphinx's ``autodoc`` feature. Example: :ref:`pw_rpc Python client API <module-pw_rpc-py>`

The next few sections provide more detail about how we used to run some of
these components in GN and how we now run them in Bazel. (The Python API
references component is skipped.)

.. note::

   I personally did most of the migration. I'm a technical writer. This
   migration was my first time working with Bazel in-depth and was the largest
   software engineering project I've ever done. My Pigweed teammates Alexei
   Frolov, Ted Pudlik, Dave Roth, and Rob Mohr provided a lot of help and
   guidance. Check out :bug:`318892911` for a granular breakdown of all the
   work that was done.

.. _blog-08-bazel-docgen-compare-doxygen:

Generating C/C++ API references with Doxygen
============================================
In the GN build we needed a custom script to run Doxygen.
The script manually cleaned output directories, calculated the
absolute paths to all the headers that Doxygen should parse, and
then ran Doxygen non-hermetically. I.e. Doxygen had access to
all files in the Pigweed repository rather than only the ones it
actually needed.

In the Bazel build all of our Doxygen logic resides within the ``MODULE.bazel``
file at the root of our repo and the ``BUILD.bazel`` files distributed
throughout the codebase. We use `rules_doxygen
<https://github.com/TendTo/rules_doxygen>`_ to hermetically run Doxygen.
We just provide ``rules_doxygen`` with a Doxygen executable, tell it
what headers to process, and it handles the rest.

We chose ``rules_doxygen`` because it's actively maintained and supports `Bazel
modules <https://bazel.build/external/module>`_ (the future of external
dependency management in Bazel). Initially the repo was missing support for
hermetic builds and macOS (Apple Silicon). I worked with the repo owner,
`Ernesto Casablanca <https://github.com/TendTo>`_, to get these features
implemented. It was one of my first proper engineering collaborations on an
open source project and it was a really rewarding experience. Thank you,
Ernesto!

.. _blog-08-bazel-docgen-compare-rust:

Generating Rust API references with rustdoc
===========================================
In the GN build there is no equivalent to this step. We have always generated
our Rust API references through Bazel.  We use `rules_rust
<https://github.com/bazelbuild/rules_rust>`_ to run ``rustdoc`` from within
Bazel. Previously our docs builder would generate the Rust API references with
Bazel, then use GN to build the rest of the docs, then upload the two
disconnected outputs to production.  Now, the docs builder just runs a single
Bazel command and everything is generated together. Long-term, this will
probably make it easier to integrate the Rust docs more thoroughly with the
rest of the site.

.. _blog-08-bazel-docgen-compare-sphinx:

Building the reStructuredText docs
==================================
.. inclusive-language: disable
.. _Sphinx: https://www.sphinx-doc.org/en/master/
.. inclusive-language: enable

This is the heart of our docs system. We author our docs in `reStructuredText
<https://docutils.sourceforge.io/rst.html>`_ (reST) and transform them
into HTML with `Sphinx`_. We currently have around 440 reST files
distributed throughout the Pigweed codebase.

In the GN build, we basically had to implement all core docs workflows
with our own custom scripts. E.g. we had a custom script for building
the docs with Sphinx, another for locally previewing the docs, etc.
We also had a lot of custom code for gathering up the reStructuredText
files distributed throughout the codebase and reorganizing them into a
structure that's easy for Sphinx to process.

In the Bazel build, we no longer need any of this custom code.
`rules_python <https://rules-python.readthedocs.io/en/latest/>`_
provides almost all of our core docs workflows now.
See :ref:`blog-08-bazel-docgen-good-sources` and
:ref:`blog-08-bazel-docgen-good-rules_python` for more details.

.. _blog-08-bazel-docgen-compare-verify:

Verifying the outputs
=====================
Our goal was to switch from GN to Bazel without ``pigweed.dev`` readers
noticing any change. With over 440 pages of documentation, it was infeasible to
manually verify that the Bazel build was producing the same outputs as the GN
build. I ended up automating the verification workflow like this:

#. Build the docs with the old GN-based system.

#. Build them again with the new Bazel-based system.

#. Traverse the output that GN produced and check that Bazel has produced
   the exact same set of HTML files.

#. Read each HTML file produced by GN as a string, then read the equivalent
   HTML file produced by Bazel as a string, then compare the strings to verify
   that their contents match exactly.

#. When they're not equal, use ``diff`` to manually pinpoint mismatches.

For final verification, I set up a visual diffing workflow:

#. Use `Playwright <https://playwright.dev/python/>`_ to take screenshots of
   each GN-generated HTML file and its Bazel-built equivalent.

#. Visually diff the screenshots with `pixelmatch-py
   <https://github.com/whtsky/pixelmatch-py>`_.

.. _blog-08-bazel-docgen-good:

--------------
What went well
--------------
We kicked off the migration project in mid-September 2024 and started using
Bazel in production in mid-January 2025. If we were in a rush, we probably
could have finished in 2 months. When you add up the work I did as well as the
help I got from others, it was about 120 hours of work. I.e. one full-time
employee working 15 full days. We expected this project to drag on for much
longer.

.. _blog-08-bazel-docgen-good-sources:

Built-in support for reorganizing sources
=========================================
Our docs are stored alongside the rest of Pigweed's code in a single
repository. To make it easier to keep the docs in-sync with code changes, each
doc lives close to its related code, like this:

.. code-block:: text

   .
   ├── a
   │   ├── a.cpp
   │   └── a.rst
   ├── b
   │   ├── b.cpp
   │   └── b.rst
   └── docs
       ├── conf.py
       └── index.rst

Sphinx, however, is easiest to work with when you have a structure
like this:

.. code-block:: text

   .
   ├── a
   │   └── a.cpp
   ├── b
   │   └── b.cpp
   └── docs
       ├── a
       │   └── a.rst
       ├── b
       │   └── b.rst
       ├── conf.py
       └── index.rst

By default, Sphinx considers the directory containing ``conf.py`` to
be the root docs directory. All ``*.rst`` (reST) files should be at or
below the root docs directory.

In the old GN-based system we had to hack together this reorganization
logic ourselves. Bazel has built-in support for source reorganization via
its ``prefix`` and ``strip_prefix`` features.

.. _blog-08-bazel-docgen-good-rules_python:

rules_python did the heavy lifting
==================================
We now get almost all of :ref:`our core docs workflows <contrib-docs-build>`
for free from ``rules_python``, thanks to the great work that `Richard
Levasseur <https://github.com/rickeylev>`_ has been doing. In this regard the
switch to Bazel has significantly reduced complexity in the Pigweed codebase
because our docs system now needs much less custom code.

.. _blog-08-bazel-docgen-good-speed:

Faster cold start builds
========================
Currently, building the docs from scratch in Bazel is about 27% faster than
building them from scratch in GN. However, there's still one major docs feature
being migrated over to Bazel so it's not an apples-to-apples comparison yet.

.. _blog-08-bazel-docgen-challenges:

----------
Challenges
----------
Overall the migration was a success, but I did get some scars!

.. _blog-08-bazel-docgen-challenges-incremental:

Incremental builds (or lack thereof)
====================================
Incremental builds aren't working. You change one line in one reStructuredText
file, and it takes 30-60 seconds to regenerate the docs. Unacceptable! Bazel
and Sphinx both separately support incremental builds, so we're hopeful that
we can find a path forward without opening a huge can of worms.

.. _blog-08-bazel-docgen-challenges-skylib:

Core utilities were hard to find
================================
At one point I needed to copy a directory containing generated outputs.  I
searched the Bazel docs, but couldn't find a built-in mechanism for this basic
task, so I created a `genrule
<https://bazel.build/reference/be/general#genrule>`_.  During code review, I
learned that there is indeed a core utility for this: `copy_directory
<https://github.com/bazelbuild/bazel-skylib/blob/main/rules/copy_directory.bzl>`_.
I was quite surprised that ``copy_directory`` is not mentioned in the official
Bazel docs.

.. _blog-08-bazel-docgen-challenges-deps:

Dependency hell
===============
Pigweed's CI/CD testing is rigorous. Before new code is allowed to merge into
Pigweed, all of Pigweed is built and tested in 10-100 different environments
(the exact number depends on what code you've touched). There's a check that
builds Pigweed with Bazel on macOS (Apple Silicon), another one that builds
Pigweed with GN on Windows (x86), and so on. We also have a bunch of
integration tests to ensure that changes to Pigweed don't break our customers'
builds or unit tests.

The :ref:`rules_python <blog-08-bazel-docgen-good-rules_python>` features that
we rely on were introduced in a fairly new version of the module, v0.36.  When
I upgraded Pigweed to v0.36, I saw the dreaded red wall of integration test
results.  In other words, upgrading to ``rules_python`` v0.36 would break the
builds for many Pigweed customers. The only path forward was to independently
upgrade each customer's codebase to support v0.36. My Pigweed teammate, Dave
Roth, saved the day by doing exactly that. Thank you, Dave, for helping me
escape `dependency hell <https://en.wikipedia.org/wiki/Dependency_hell>`_!

.. _blog-08-bazel-docgen-challenges-graphs:

Explicit build graphs were time consuming
=========================================
Like the rest of Pigweed's codebase, I opted to explicitly list all
sources and dependencies in the docs build rules, like this:

.. code-block:: py

   sphinx_docs_library(
       name = "docs",
       srcs = [
           "api.rst",
           "code_size.rst",
           "design.rst",
           "docs.rst",
           "guide.rst",
       ],
       # …
   )

For the initial prototyping, using globs would have been much
faster:

.. code-block:: py

   sphinx_docs_library(
       name = "docs",
       srcs = glob([
           "*.rst",
       ]),
       # …
   )

.. _blog-08-bazel-docgen-challenges-starlark:

Uncanny valley experiences with Starlark
========================================
.. _Starlark: https://github.com/bazelbuild/starlark?tab=readme-ov-file#starlark
.. _dialect: https://en.wikipedia.org/wiki/Programming_language#Dialects,_flavors_and_implementations

`Starlark`_ naturally looks and feels a lot like Python, since it's a `dialect`_
of Python. During the migration I had a few `uncanny valley
<https://en.wikipedia.org/wiki/Uncanny_valley>`_ experiences where I expected
some Python idiom to work, and then eventually figured out that Starlark
doesn't allow it. For example, to build out a dict in Python, I sometimes
use code like this:

.. code-block:: py

   output_group_info = {}
   for out in ctx.attr.outs:
       output_group_info[out] = ctx.actions.declare_directory(out)

But this is not allowed in Starlark because dicts are immutable.
It is OK, however, to rebind the entire variable, like this:

.. code-block:: py

   output_group_info = {}
   for out in ctx.attr.outs:
       output_group_info |= {out: ctx.actions.declare_directory(out)}

.. _blog-08-bazel-docgen-next:

-----------
What's next
-----------
Our top priorities are figuring out incremental builds and turning
down the old GN-based build.

Thank you for reading! If you'd like to discuss any of this with me, you can
find me in the ``#docs`` channel of `Pigweed's Discord
<https://discord.com/channels/691686718377558037/691686718377558040>`_.
