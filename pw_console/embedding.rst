.. _module-pw_console-embedding:

Embedding Guide
===============

Using embed()
-------------

``pw console`` is invoked by calling the ``embed()`` function in your own
Python script.

.. automodule:: pw_console.console_app
    :members: embed
    :undoc-members:
    :show-inheritance:

Adding Log Metadata
-------------------

``pw_console`` can display log messages in a table with justified columns for
metadata fields provided by :ref:`module-pw_log_tokenized`.

It is also possible to manually add values that should be displayed in columns
using the ``extra`` keyword argument when logging from Python. See the `Python's
logging documentation`_ for how ``extra`` works. A dict of name, value pairs can
be passed in as the ``extra_metadata_fields`` variable. For example, the
following code will create a log message with two custom columns titled
``module`` and ``timestamp``.

.. code-block:: python

  import logging

  LOG = logging.getLogger('log_source_1')

  LOG.info(
      'Hello there!',
      extra={
          'extra_metadata_fields': {
              'module': 'cool',
              'timestamp': 1.2345,
          }
      }
  )

.. _Python's logging documentation: https://docs.python.org/3/library/logging.html#logging.Logger.debug
