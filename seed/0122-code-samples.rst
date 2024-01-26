.. _seed-0122:

===================================
0122: Organize Pigweed code samples
===================================
.. seed::
   :number: 122
   :name: Organize Pigweed code samples
   :status: open_for_comments
   :proposal_date: 2024-01-09
   :cl: 187121
   :facilitator: Kayce Basques

-------
Summary
-------
This SEED describes how Pigweed code samples will be organized.

----------
Motivation
----------
Pigweed users need to see code samples that demonstrate Pigweed in action.
However, there are a few different use cases for such code samples.
Historically, we've not been explicit about what the use cases are, and how
they should be served. This SEED will fill this gap.

--------
Proposal
--------

Repositories
============
Pigweed will maintain the following types of code sample repositories:

* **Quickstart**: `minimal, complete, reproducible
  <https://stackoverflow.com/help/minimal-reproducible-example>`_ projects you
  can clone to understand the basics of Pigweed and start a new `greenfield
  project <https://en.wikipedia.org/wiki/Greenfield_project>`_.  There will be
  one Quickstart repository for each build system that Pigweed supports.
* **Examples**: granular, comprehensive code examples of every Pigweed
  module's API. The Examples repository can also demonstrate core use cases
  that require using multiple modules together. There will only be one Examples
  repository.
* **Showcase**: Showstopper examples that demonstrate how to use Pigweed
  to implement complex real-world applications. There will be many Showcase
  repositories, each demonstrating a different application. These repositories
  should inspire users, show them how to implement complex integrations, and
  convince them that Pigweed is the best way to develop embedded projects.

Standard board
==============
The Quickstart and Examples repositories will eventually focus on a single
:ref:`seed-0122-standard-board`. But selecting the standard board is out of the
scope of this SEED.

---------------
Detailed design
---------------

.. _seed-0122-quickstart:

Quickstart repository
=====================
* **User journeys** these repositories serve:

  * "I want to clone a minimal repository to create my own project from
    scratch."
  * "I want to understand the smallest set of steps needed to add Pigweed as a
    dependency of my own existing project."
  * "I'm just getting started with Pigweed and want a tutorial that will
    put *some* code on device".

* **Repository contents**: One repository per build system that demonstrates
  the minimal set of Pigweed features:

  #.  Bootstrapping (downloading required tools, third-party dependencies, etc).
  #.  Building the minimal, viable, complete (MVC) application for
      :ref:`target-host` and for the :ref:`seed-0122-standard-board`.
  #.  Flashing the application to the board.
  #.  Communicating with the board via :ref:`module-pw_console`.

  Determining the exact content of the MVC app is out-of-scope for this SEED.
  This is the subject of `SEED-0106
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/155430>`_.
  However, every Quickstart repository will implement the same set of MVC
  features.

  There will be tutorial documentation associated with each of these
  repositories, but it will be hosted in pigweed.git. Documentation generation
  with :ref:`module-pw_docgen` falls outside the scope of Quickstart.

  We may also add separate Quickstart repositories for boards other than the
  "standard board".

* **Locations**:

  * https://pigweed.googlesource.com/pigweed/quickstart/bazel.git
  * https://pigweed.googlesource.com/pigweed/quickstart/gn.git
  * (No CMake quickstart. We offer support for integrating Pigweed into an
    existing CMake project, but don't recommend CMake for a greenfield Pigweed
    project and so have no quickstart repository for it.)

.. _seed-0122-examples:

Examples repository
===================
* **User journeys** this repository serves:

  * "I want to see an example of how a module's API is used in the context of a
    full, building project."
  * "I want to use tokenized logging, how do I hook this up?"
  * "I read you can customize the :ref:`module-pw_console`, can I see an example?"
  * "I want to see how to configure a backend of this module."
  * "How do I use method X from Pigweed module Y?"
  * "How do I use Pigweed modules A and B together?"

* **Repository contents**: One repository that contains a bunch of directories.
  Each directory is meant to be consulted in isolation. Common use cases such
  as logging over UART or blinking an LED, may get their own directories.
  Related examples (e.g., examples for an individual module) may be grouped
  into a directory hierarchy for clarity.

  The individual examples may define their own build targets, but they are not
  expected to set up independent builds. They're all part of one top-level build.

  In general, it should be possible to build the examples using all of
  Pigweed's supported build systems (Bazel, CMake, GN). However, if the
  functionality being exemplified is only supported in some build systems (e.g.
  :ref:`module-pw_docgen` is only supported in GN as of this writing), then its
  examples can be restricted to those build systems.

  Unlike Quickstart, this repository is a reference work: the user is not
  expected to read through it "from beginning to end". Rather, they will come
  here to view a specific example. We eventually expect to have hundreds or
  even thousands of examples.

  The documentation for the Examples repo will be hosted at http://pigweed.dev,
  but will be built from source within the ``examples.git`` repo. Building
  documentation using :ref:`module-pw_docgen` is one of the things we're
  exemplifying.

* **Location**: https://pigweed.googlesource.com/pigweed/examples.git

Showcase repositories
=====================
* **User journeys** these repositories serve:

  * "I want to see a cool project built using Pigweed."
  * "I'm looking for a full-fledged demo of what Pigweed is capable of."
  * "I'm looking for a Show HN submission."
  * "I want to convince my team Pigweed is powerful enough for our use case."
  * "I need proof that Pigweed is a better way to develop embedded projects."
  * "I need proof that Pigweed is production-ready."

* **Repository contents**: One repository per showcase project. These projects
  are standalone, with documentation that explains what they do. They're not
  necessarily easy to stand up yourself, and may require custom hardware that's
  hard to source. :ref:`Kudzu <docs-blog-01-kudzu>` and `Gonk
  <https://pigweed.googlesource.com/gonk.git>`_ are examples of showcase
  projects.

* **Locations**: Showcase projects can be hosted anywhere. Open-source projects
  created by third parties and not hosted on https://pigweed.googlesource.com
  can also be showcases.

  We will host a list of Showcase projects, with brief descriptions, at
  https://pigweed.dev/showcase.

.. _seed-0122-standard-board:

Standard board
==============
Pigweed will select a "standard board" for use in our code samples. Today, the
de facto standard board is the STM32F429I-DISC1, but we expect to select a
different standard board in the near future. Standard board selection will be
discussed in a separate followup SEED.

----
FAQs
----

.. _seed-0122-existing-samples:

How do existing code samples map to this scheme?
================================================
* `example/echo <https://pigweed.googlesource.com/pigweed/example/echo.git>`_
  will be renamed to become :ref:`seed-0122-quickstart` for Bazel.
* `sample_project
  <https://pigweed.googlesource.com/pigweed/sample_project.git>`_ will be
  renamed and reorganized to become the :ref:`seed-0122-examples`.
* `Kudzu <https://pigweed.googlesource.com/pigweed/kudzu.git>`_ and `Gonk
  <https://pigweed.googlesource.com/gonk.git>`_ are Showcase projects.


What about inline code samples in the documentation?
====================================================
We will continue to provide inline code samples in the documentation, and later
in 2024 may prototype solutions for ensuring they compile and pass assertions.
But this is out of scope for this SEED.

--------------
Open questions
--------------
A followup SEED will discuss the selection of the "standard board" for Pigweed
Quickstart and Examples repositories.

The exact feature set of the MVC application demonstrated by the Quickstart
repos is the subject of `SEED-0106
<https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/155430>`_.
