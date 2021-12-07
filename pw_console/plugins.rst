.. _module-pw_console-plugins:

============
Plugin Guide
============

Pigweed Console supports extending the user interface with custom widgets. For
example: Toolbars that display device information and provide buttons for
interacting with the device.

---------------
Writing Plugins
---------------

Creating new plugins has a few high level steps:

1. Create a new Python class inheriting from either `WindowPane`_ or
   `WindowPaneToolbar`_.

   - Optionally inherit from The ``PluginMixin`` class as well for running
     background tasks.

2. Enable the plugin before pw_console startup by calling ``add_window_plugin``,
   ``add_top_toolbar`` or ``add_bottom_toolbar``. See the
   :ref:`module-pw_console-embedding-plugins` section of the
   :ref:`module-pw_console-embedding` for an example.

3. Run the console and enjoy!

   - Debugging Plugin behavior can be done by logging to a dedicated Python
     logger and viewing in-app. See `Debugging Plugin Behavior`_ below.

Background Tasks
================

Plugins may need to have long running background tasks which could block or slow
down the Pigweed Console user interface. For those situations use the
``PluginMixin`` class. Plugins can inherit from this and setup the callback that
should be executed in the background.

.. autoclass:: pw_console.plugin_mixin.PluginMixin
    :members:
    :show-inheritance:

Debugging Plugin Behavior
=========================

If your plugin uses background threads for updating it can be difficult to see
errors. Often, nothing will appear to be happening and exceptions may not be
visible. When using ``PluginMixin`` you can specify a name for a Python logger
to use with the ``plugin_logger_name`` keyword argument.

.. code-block:: python

   class AwesomeToolbar(WindowPaneToolbar, PluginMixin):

       def __init__(self, *args, **kwargs):
           super().__init__(*args, **kwargs)
           self.update_count = 0

           self.plugin_init(
               plugin_callback=self._background_task,
               plugin_callback_frequency=1.0,
               plugin_logger_name='my_awesome_plugin',
           )

       def _background_task(self) -> bool:
           self.update_count += 1
           self.plugin_logger.debug('background_task_update_count: %s',
                                    self.update_count)
           return True

This will let you open up a new log window while the console is running to see
what the plugin is doing. Open up the logger name provided above by clicking in
the main menu: :guilabel:`File > Open Logger > my_awesome_plugin`.

--------------
Sample Plugins
--------------

Pigweed Console will provide a few sample plugins to serve as templates for
creating your own plugins. These are a work in progress at the moment and not
available at this time.

.. _WindowPane: https://cs.opensource.google/pigweed/pigweed/+/main:pw_console/py/pw_console/widgets/window_pane.py
.. _WindowPaneToolbar: https://cs.opensource.google/pigweed/pigweed/+/main:pw_console/py/pw_console/widgets/window_pane_toolbar.py
