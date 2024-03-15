.. _module-pw_log_android:

==============
pw_log_android
==============
.. pigweed-module::
   :name: pw_log_android

``pw_log_android`` is a ``pw_log`` backend for Android.

--------
Overview
--------
``pw_log_android`` is a ``pw_log`` backend that directs logs to the `Android
logging system <https://developer.android.com/tools/logcat>`_ (``liblog``).
It's the default logging backend selected by the ``pw_android_common_backends``
rule. See :ref:`module-pw_build_android-common-backends`.

.. _module-pw_log_android-stderr:

Logging to standard error
=========================
By default, logs are written to ``logd`` (the logger daemon) and can be viewed
using the ``logcat`` command-line tool. This is useful for system services.

Logs can alternatively be written directly to *standard error* (stderr), which
is useful for short-lived command-line utilities. Pigweed facilitates this by
providing the ``pw_log_android_stderr`` library. Include it in your executable
via ``whole_static_libs`` as seen here:

.. code-block:: javascript

   cc_binary {
       name: "my_app",
       host_supported: true,
       vendor: true,
       defaults: [
           "pw_android_common_backends",
       ],
       srcs: [
           "main.cc",
       ],
       whole_static_libs: [
            "pw_log_android_stderr",
       ],
   }

.. warning::

   ``pw_log_android_stderr`` should only be used in executables
   (``cc_binary``).

   Using it with a library would affect the logging behavior for all consumers
   of that library.


Log level mapping
=================
This table shows the mapping of Pigweed log levels to
`Android log levels <https://developer.android.com/ndk/reference/group/logging>`_.

.. list-table:: Log level mapping
   :align: left
   :header-rows: 1

   * - Pigweed level
     - Android level
   * - DEBUG
     - ``ANDROID_LOG_DEBUG``
   * - INFO
     - ``ANDROID_LOG_INFO``
   * - WARN
     - ``ANDROID_LOG_WARN``
   * - ERROR
     - ``ANDROID_LOG_ERROR``
   * - CRITICAL
     - ``ANDROID_LOG_ERROR``
   * - FATAL
     - ``ANDROID_LOG_FATAL``
