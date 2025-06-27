.. _docs-workflows-build-drivers:

=======================
Workflows build drivers
=======================
Workflows build drivers are tools that translate workflow configurations in a
``workflows.json`` file into a sequence of executable actions.

------------
How it Works
------------

#. **Configuration initialization**: The Workflows launcher reads a
   ``workflows.json`` configuration file to identify existing build
   configurations.
#. **Subtool identification**: The subtool requested by the user (e.g.
   ``format``) is located in the configuration, and the associated
   ``build_type`` is mapped to a registered driver.
#. **Driver invocation**: The Workflows launcher feeds the requested build
   to the driver as a ``BuildDriverRequest`` formatted as ProtoJSON.
#. **Driver response**: The driver receives the request, processes each job,
   and generates a ``BuildDriverResponse`` message that is emitted as ProtoJSON.
   This response contains a sequence of actions for each job, detailing the
   commands that should be run.
#. **Execution**: The Workflows launcher receives the ``BuildDriverResponse``,
   applies any necessary post-processing, and executes the actions in the
   specified order.

This separation of concerns allows the Workflows system to be extended with new
build systems by creating new drivers, without modifying the core launcher.

------------
Bazel Driver
------------
The Bazel driver translates ``bazel`` build types to a series of actions. It
doesn't yet offer any driver-specific configuration options.

When generating actions for a ``Build``, it includes:

#. Canonicalizing Bazel flags. This is done to ensure build targets don't leak
   into the list of build arguments.
#. Running ``bazelisk build`` with the specified targets and arguments.
#. Running ``bazelisk test`` with the same targets and arguments.

When generating actions for a ``Tool``, it includes:

#. Canonicalizing Bazel flags. This is done to ensure build targets don't leak
   into the list of build arguments.
#. Running ``bazelisk run`` with the specified target and arguments.
