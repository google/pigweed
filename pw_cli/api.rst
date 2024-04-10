.. _module-pw_cli-api:

=============
API Reference
=============
.. pigweed-module-subpage::
   :name: pw_cli

----------
pw_cli.log
----------
.. automodule:: pw_cli.log
   :members:

--------------
pw_cli.plugins
--------------
:py:mod:`pw_cli.plugins` provides general purpose plugin functionality. The
module can be used to create plugins for command line tools, interactive
consoles, or anything else. Pigweed's ``pw`` command uses this module for its
plugins.

To use plugins, create a :py:class:`pw_cli.plugins.Registry`. The registry may
have an optional validator function that checks plugins before they are
registered (see :py:meth:`pw_cli.plugins.Registry.__init__`).

Plugins may be registered in a few different ways.

* **Direct function call.** Register plugins by calling
  :py:meth:`pw_cli.plugins.Registry.register` or
  :py:meth:`pw_cli.plugins.Registry.register_by_name`.

  .. code-block:: python

     import pw_cli

     _REGISTRY = pw_cli.plugins.Registry()

     _REGISTRY.register('plugin_name', my_plugin)
     _REGISTRY.register_by_name('plugin_name', 'module_name', 'function_name')

* **Decorator.** Register using the :py:meth:`pw_cli.plugins.Registry.plugin`
  decorator.

  .. code-block:: python

     import pw_cli

     _REGISTRY = pw_cli.plugins.Registry()

     # This function is registered as the "my_plugin" plugin.
     @_REGISTRY.plugin
     def my_plugin():
         pass

     # This function is registered as the "input" plugin.
     @_REGISTRY.plugin(name='input')
     def read_something():
         pass

  The decorator may be aliased to give a cleaner syntax (e.g. ``register =
  my_registry.plugin``).

* **Plugins files.** Plugins files use a simple format:

  .. code-block::

     # Comments start with "#". Blank lines are ignored.
     name_of_the_plugin module.name module_member

     another_plugin some_module some_function

  These files are placed in the file system and apply similarly to Git's
  ``.gitignore`` files. From Python, these files are registered using
  :py:meth:`pw_cli.plugins.Registry.register_file` and
  :py:meth:`pw_cli.plugins.Registry.register_directory`.

.. automodule:: pw_cli.plugins
   :members:

-------------
pw_cli.plural
-------------
.. automodule:: pw_cli.plural
   :members:

----------------------
pw_cli.status_reporter
----------------------
.. automodule:: pw_cli.status_reporter
   :members:

------------------
pw_cli.tool_runner
------------------
.. automodule:: pw_cli.tool_runner
   :members:
   :exclude-members: ToolRunner

   .. autoclass:: pw_cli.tool_runner.ToolRunner
      :special-members: +__call__
      :private-members: +_run_tool
