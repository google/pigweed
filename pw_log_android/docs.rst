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
