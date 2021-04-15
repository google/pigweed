.. _module-pw_thread:

=========
pw_thread
=========
The ``pw_thread`` module contains utilities for thread creation and thread
execution.

.. contents::
   :local:
   :depth: 2

.. Warning::
  This module is still under construction, the API is not yet stable.

---------------
Thread Creation
---------------
The class ``pw::thread::Thread`` can represent a single thread of execution.
Threads allow multiple functions to execute concurrently.

The Thread's API is C++11 STL
`std::thread <https://en.cppreference.com/w/cpp/thread/thread>`_ like, meaning
the object is effectively a thread handle and not an object which contains the
thread's context. Unlike ``std::thread``, the API requires
``pw::thread::Options`` as an argument and is limited to only work with
``pw::thread::ThreadCore`` objects and functions which match the
``pw::thread::Thread::ThreadRoutine`` signature.

Threads may begin execution immediately upon construction of the associated
thread object (pending any OS scheduling delays), starting at the top-level
function provided as a constructor argument. The return value of the
top-level function is ignored. The top-level function may communicate its
return value by modifying shared variables (which may require
synchronization, see :ref:`module-pw_sync`)

Thread objects may also be in the state that does not represent any thread
(after default construction, move from, detach, or join), and a thread of
execution may be not associated with any thread objects (after detach).

No two Thread objects may represent the same thread of execution; Thread is
not CopyConstructible or CopyAssignable, although it is MoveConstructible and
MoveAssignable.

.. list-table::

  * - *Supported on*
    - *Backend module*
  * - FreeRTOS
    - :ref:`module-pw_thread_freertos`
  * - ThreadX
    - :ref:`module-pw_thread_threadx`
  * - embOS
    - Planned
  * - STL
    - :ref:`module-pw_thread_stl`
  * - Zephyr
    - Planned
  * - CMSIS-RTOS API v2 & RTX5
    - Planned


Options
=======
The ``pw::thread::Options`` contains the parameters or attributes needed for a
thread to start.

Pigweed does not generalize options, instead we strive to give you full control
where we provide helpers to do this.

Options are backend specific and ergo the generic base class cannot be
directly instantiated.

The attributes which can be set through the options are backend specific
but may contain things like the thread name, priority, scheduling policy,
core/processor affinity, and/or an optional reference to a pre-allocated
Context (the collection of memory allocations needed for a thread to run).

Options shall NOT permit starting as detached, this must be done explicitly
through the Thread API.

Options must not contain any memory needed for a thread to run (TCB,
stack, etc.). The Options may be deleted or re-used immediately after
starting a thread.

Please see the thread creation backend documentation for how their Options work.

.. Note::
  Options have a default constructor, however default options are not portable!
  Default options can only work if threads are dynamically allocated by default,
  meaning default options cannot work on backends which require static thread
  allocations. In addition on some schedulers, default options will not work
  for other reasons.

Detaching & Joining
===================
The ``Thread::detach()`` API is always available, to let you separate the
thread of execution from the thread object, allowing execution to continue
independently.

The joining API, more specifically ``Thread::join()``, is conditionally
available depending on the selected backend for thread creation and how it is
configured. The backend is responsible for providing the
``PW_THREAD_JOINING_ENABLED`` macro through
``pw_thread_backend/thread_native.h``. This ensures that any users which include
``pw_thread/thread.h`` can use this macro if needed.

Please see the selected thread creation backend documentation for how to
enable joining if it's not already enabled by default.

.. Warning::
  A constructed ``pw::thread::Thread`` which represents a thread of execution
  must be EITHER detached or joined, else the destructor will assert!

ThreadRoutine & ThreadCore
==========================
Threads must either be invoked through a
``pw::thread::Thread::ThreadRoutine``` style function or implement the
``pw::thread::ThreadCore`` interface.

.. code-block:: cpp

  namespace pw::thread {

  // This function may return.
  using Thread::ThreadRoutine = void (*)(void* arg);

  class ThreadCore {
   public:
    virtual ~ThreadCore() = default;

    // The public API to start a ThreadCore, note that this may return.
    void Start() { Run(); }

   private:
    // This function may return.
    virtual void Run() = 0;
  };

  }  // namespace pw::thread;


To use the ``pw::thread::Thread::ThreadRoutine``, your function must have the
following signature:

.. code-block:: cpp

  void example_thread_entry_function(void *arg);


To invoke a member method of a class a static lambda closure can be used
to ensure the dispatching closure is not destructed before the thread is
done executing. For example:

.. code-block:: cpp

  class Foo {
   public:
    void DoBar() {}
  };
  Foo foo;

  static auto invoke_foo_do_bar = [](void *void_foo_ptr) {
      //  If needed, additional arguments could be set here.
      static_cast<Foo*>(void_foo_ptr)->DoBar();
  };

  // Now use the lambda closure as the thread entry, passing the foo's
  // this as the argument.
  Thread thread(options, invoke_foo_do_bar, &foo);
  thread.detach();


Alternatively, the aforementioned ``pw::thread::ThreadCore`` interface can be
be implemented by an object by overriding the private
``void ThreadCore::Run();`` method. This makes it easier to create a thread, as
a static lambda closure or function is not needed to dispatch to a member
function without arguments. For example:

.. code-block:: cpp

  class Foo : public ThreadCore {
   private:
    void Run() override {}
  };
  Foo foo;

  // Now create the thread, using foo directly.
  Thread(options, foo).detach();

.. Warning::
  Because the thread may start after the pw::Thread creation, an object which
  implements the ThreadCore MUST meet or exceed the lifetime of its thread of
  execution!
