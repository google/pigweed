.. _module-pw_log_basic:

------------
pw_log_basic
------------
.. pigweed-module::
   :name: pw_log_basic

``pw_log_basic`` is a ``pw_log`` backend that sends logs over ``pw_sys_io`` by
default. The destination of ``pw_sys_io`` depends on the ``pw_sys_io`` backend
in use. This is controlled by the ``dir_pw_sys_io_backend`` variable in a
target's ``target_config.gni``.

Interface extension
===================
The log output may be changed from ``pw_sys_io`` to an arbitrary function by
calling ``pw::log_basic::SetOutput``.

.. cpp:namespace:: pw::log_basic

.. cpp:function:: void SetOutput(void (*log_output)(std::string_view))

  Set the log output function, which defaults ``pw_sys_io::WriteLine``. This
  function is called with each formatted log message.

Note that ``pw::log_baisc::SetOutput`` is not part of the :ref:`module-pw_log`
interface. As a result, in the Bazel build, any build target that calls this
function must:

*  Explicitly list ``@pigweed//pw_log_basic:extension`` in its ``deps``. This
   avoids a :ref:`module-pw_toolchain-bazel-layering-check` violation.
*  Mark itself ``target_compatible_with`` only configurations for which the
   `config_setting <https://bazel.build/reference/be/general#config_setting>`__
   ``@pigweed//pw_log_basic:is_active_backend`` is true. This informs the build
   system that the target is only compatible with platforms that set the
   :ref:`module-pw_log` backend to ``pw_log_basic``.

See ``//pw_hdlc:hdlc_sys_io_system_server`` for an example of such a target.

Implementation
==============
This module employs an internal buffer for formatting log strings, whose size
can be configured via the ``PW_LOG_BASIC_ENTRY_SIZE`` macro which defaults to
150 bytes. Any final log statements that are larger than
``PW_LOG_BASIC_ENTRY_SIZE - 1`` bytes (one byte used for a null terminator) will
be truncated.

.. note::
  The documentation for this module is currently incomplete.
