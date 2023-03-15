.. _module-pw_async_basic:

================
pw_async_basic
================

This module includes basic implementations of pw_async's Dispatcher and
FakeDispatcher.

---
API
---
.. doxygenclass:: pw::async::BasicDispatcher
   :members:

-----
Usage
-----
First, set the following GN variables:

.. code-block::

  pw_async_TASK_BACKEND="$dir_pw_async_basic:task"
  pw_async_FAKE_DISPATCHER_BACKEND="$dir_pw_async_basic:fake_dispatcher"


Next, create a target that depends on ``//pw_async_basic:dispatcher``:

.. code-block::

  pw_executable("hello_world") {
    sources = [ "hello_world.cc" ]
    deps = [
      "//pw_async_basic:dispatcher",
    ]
  }

Next, construct and use a ``BasicDispatcher``.

.. code-block:: cpp

  #include "pw_async_basic/dispatcher.h"

  void DelayedPrint(pw::async::Dispatcher& dispatcher) {
    dispatcher.PostAfter([](auto&){
       printf("hello world\n");
    }, 5s);
  }

  int main() {
    pw::async::BasicDispatcher dispatcher;
    DelayedPrint(dispatcher);
    dispatcher.RunFor(10s);
    return 0;
  }

-----------
Size Report
-----------
.. include:: docs_size_report
