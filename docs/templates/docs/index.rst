.. _module-MODULE_NAME:

===========
MODULE_NAME
===========
..
  A short description of the module. Aim for three sentences at most that
  clearly convey what the module does.

.. card::
   :octicon:`comment-discussion` Status:
   :bdg-primary:`Experimental`
   :octicon:`chevron-right`
   :bdg-secondary-line:`Unstable`
   :octicon:`chevron-right`
   :bdg-secondary-line:`Stable`
   :octicon:`kebab-horizontal`
   :bdg-primary:`Current`
   :octicon:`chevron-right`
   :bdg-secondary-line:`Deprecated`

   :octicon:`multi-select` Backends: :ref:`module-pw_rpc`

   :octicon:`copy` Facade: :ref:`module-pw_rpc`

----------
Background
----------
..
  Describe *what* problem this module solves. You can make references to other
  existing solutions to highlight why their approaches were not suitable, but
  avoid describing *how* this module solves the problem in this section.

  For some modules, this section may need to include fundamental concepts about
  the problem being solved. Those should be mentioned briefly here, but covered
  more extensively in ``concepts.rst``.

------------
Our solution
------------
..
  Describe *how* this module addresses the problems highlighted in section
  above.  This is a great place to include artifacts other than text to more
  effectively deliver the message. For example, consider including images or
  animations for UI-related modules, or code samples illustrating a superior API
  in libraries.

Design considerations
---------------------
..
  Briefly explain any pertinent design considerations and trade-offs. For
  example, perhaps this module optimizes for very small RAM and Flash usage at
  the expense of additional CPU time, where other solutions might make a
  different choice.

  Many modules will have more extensive documentation on design and
  implementation details. If so, that additional content should be in
  ``design.rst`` and linked to from this section.

Size report
-----------
..
  If this module includes code that runs on target, include the size report(s)
  here.

Roadmap
-------
..
  What are the broad future plans for this module? What remains to be built, and
  what are some known issues? Are there opportunities for contribution?

---------------
Who this is for
---------------
..
  Highlight the use cases that are appropriate for this module. This is the
  place to make the sales pitch-why should developers use this module, and what
  makes this module stand out from alternative solutions that try to address
  this problem.

--------------------
Is it right for you?
--------------------
..
  This module may not solve all problems or be appropriate for all
  circumstances.  This is the place to add those caveats. Highly-experimental
  modules that are under very active development might be a poor fit for any
  developer right now.  If so, mention that here.

---------------
Getting started
---------------
..
  Explain how developers use this module in their project, including any
  prerequisites and conditions. This section should cover the fastest and
  simplest ways to get started (for example, "add this dependency in GN",
  "include this header"). More complex and specific use cases should be covered
  in guides that are linked in this section.

--------
Contents
--------
.. toctree::
   :maxdepth: 1

   concepts
   design
   guides
   api
   cli
   gui

.. toctree::
   :maxdepth: 2

   codelabs/index
