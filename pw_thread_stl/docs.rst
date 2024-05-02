.. _module-pw_thread_stl:

=============
pw_thread_stl
=============
This is a set of backends for pw_thread based on the C++ STL.

-------------------
Compatibility notes
-------------------
Windows
=======
* `b/317922402 <https://issues.pigweed.dev/317922402>`_\:
  ``pw::thread::Thread::detach()`` can cause indefinite hangs on Windows
  targets. For now, this functionality has been disabled. Attempting to detach
  a thread on Windows will cause the following link-time error to be emitted:

  .. code-block::

     ld.exe: pw_strict_host_gcc_debug/obj/pw_thread/test_thread_context_facade_test.lib.test_thread_context_facade_test.cc.o: in function `pw::thread::Thread::detach()':
     pw_thread_stl/public/pw_thread_stl/thread_inline.h:55: undefined reference to `pw::thread::internal::ErrorAttemptedToInvokeStdThreadDetachOnMinGW()'
