.. _docs-glossary:

========
Glossary
========
This glossary defines terms that have specific meanings in the context of
Pigweed.

.. _docs-glossary-development-host:

----------------
Development host
----------------
The computer that you develop software on.

.. _docs-glossary-informed-poll:

-------------
Informed poll
-------------
The informed poll programming model is the core design philosophy behind
``pw_async2``.  The central idea is that asynchronous work is encapsulated in
objects called :doxylink:`Tasks <pw::async2::Task>`. Instead of registering
callbacks for different events, a central :doxylink:`Dispatcher
<pw::async2::Dispatcher>` *polls* these tasks to see if they can make progress.
The polling is *informed* because the task coordinates with its event source
regarding when it's ready to make more progress. The event source notifies the
dispatcher when the task is ready to proceed and therefore should be polled
again.

Learn more:

* :doc:`pw_async2/informed_poll`

.. _docs-glossary-facade:

------
Facade
------
A facade is an API contract of a module that must be satisfied at compile-time,
i.e. a swappable dependency that changes the implementation of an API at
compile-time.

Learn more:

* :ref:`docs-facades`

.. _docs-glossary-module:

------
Module
------
A Pigweed module is an open source library that addresses a common need for
embedded software developers. For example, :ref:`module-pw_string` provides
an API for string operations that is both safe and suitable for
resource-constrained embedded systems.

Modules are Pigweed's core products. Every directory that starts with ``pw_``
in the `root directory of the upstream Pigweed repository
<https://cs.opensource.google/pigweed/pigweed>`_ represents a single module.

Modules are modular in the sense that you can use one module in your project
without always needing to depend on the rest of the entire Pigweed codebase.
Some modules may depend on other modules, however. The key idea of their
modularity is that they're not tied to any "core" or platform layer.

Other general rules about modules:

* They strive to minimize policy decisions such as whether or not allocation
  is required, buffer sizing limits, etc. Projects have control over these
  decisions.
* They don't instantiate any static objects.

Learn more:

* :ref:`List of modules <docs-module-guides>`
* :ref:`docs-module-structure`
* :ref:`docs-concepts-embedded-development-libraries`

.. _docs-glossary-committers:

------------------
Pigweed committers
------------------
People who have write access to :ref:`docs-glossary-upstream`.
I.e. people who can run Pigweed's tryjobs, approve code reviews,
and merge code into upstream Pigweed.

.. _docs-glossary-upstream:

----------------
Upstream Pigweed
----------------
.. _main repository: https://cs.opensource.google/pigweed/Pigweed

Pigweed's `main repository`_.
