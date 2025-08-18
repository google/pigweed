.. _module-pw_async2-backends:

========
Backends
========
The :doxylink:`pw_async2 interface <pw_async2>` must be fulfilled
by a concrete implementation. You can use one of the Pigweed-provided backends
listed below or roll your own. If you roll your own, please consider
:ref:`contributing <docs-contributing>` it to upstream Pigweed!

.. _epoll: https://man7.org/linux/man-pages/man7/epoll.7.html

* :ref:`module-pw_async2_basic`. A backend that uses a
  thread-notification-based :doxylink:`pw::async2::Dispatcher`.
* :ref:`module-pw_async2_epoll`. A backend that uses a
  :doxylink:`pw::async2::Dispatcher` backed by Linux's `epoll`_ notification
  system.

.. toctree::
   :maxdepth: 1
   :hidden:

   Basic <../pw_async2_basic/docs>
   Linux epoll <../pw_async2_epoll/docs>
