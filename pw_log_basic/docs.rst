.. _chapter-pw-log-basic:

.. default-domain:: cpp

.. highlight:: sh

------------
pw_log_basic
------------
``pw_log_basic`` is a ``pw_log backend`` that sends logs over ``pw_sys_io``. The
destination of ``pw_sys_io`` depends on the ``pw_sys_io`` backend in use. This
is controlled by the ``dir_pw_sys_io_backend`` variable in a target's
``target_config.gni``.
information.

This module employs an internal buffer for formatting log strings, and currently
has a fixed size of 150 bytes. Any final log statements that are larger than
149 bytes (one byte used for a null terminator) will be truncated.

.. note::
  The documentation for this module is currently incomplete.
