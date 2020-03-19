.. _chapter-docs:

.. default-domain:: cpp

.. highlight:: sh

----
docs
----
The Pigweed docs target assembles Pigweed's reStructuredText and markdown
documentation into a collection of HTML pages.

Building
========
To build for this target, change the ``pw_target_config`` GN build arg to point
to this target's configuration file.

.. code:: sh

  $ gn gen --args='pw_target_config = "//targets/docs/target_config.gni"' out/docs
  $ ninja -C out/docs

or

.. code:: sh

  $ gn gen out/docs
  $ gn args
  # Modify and save the args file to update the pw_target_config.
  pw_target_config = "//targets/docs/target_config.gni"
  $ ninja -C out/docs

Output
======
Final HTML documentation will be placed in the ``gen/docs/html`` directory of
the build.
