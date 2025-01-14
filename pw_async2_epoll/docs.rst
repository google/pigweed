.. _module-pw_async2_epoll:

===============
pw_async2_epoll
===============
.. pigweed-module::
   :name: pw_async2_epoll

.. _epoll: https://man7.org/linux/man-pages/man7/epoll.7.html

This is a simple backend for ``pw_async2`` that uses a ``Dispatcher`` backed
by Linux's `epoll`_ notification system.
