.. _module-pw_thread:

=========
pw_thread
=========
The ``pw_thread`` module contains utilities for thread creation and thread
execution.

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

We recognize that the C++11's STL ``std::thread``` API has some drawbacks where
it is easy to forget to join or detach the thread handle. Because of this, we
offer helper wrappers like the ``pw::thread::DetachedThread``. Soon we will
extend this by also adding a ``pw::thread::JoiningThread`` helper wrapper which
will also have a lighter weight C++20 ``std::jthread`` like cooperative
cancellation contract to make joining safer and easier.

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
    - :ref:`module-pw_thread_embos`
  * - STL
    - :ref:`module-pw_thread_stl`
  * - Zephyr
    - Planned
  * - CMSIS-RTOS API v2 & RTX5
    - Planned

Module Configuration Options
============================
The following configurations can be adjusted via compile-time configuration of
this module, see the
:ref:`module documentation <module-structure-compile-time-configuration>` for
more details.

.. c:macro:: PW_THREAD_CONFIG_LOG_LEVEL

  The log level to use for this module. Logs below this level are omitted.

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

DetachedThread
==============
To make it slightly easier and cleaner to spawn detached threads without having
to worry about thread handles, a wrapper ``DetachedThread()`` function is
provided which creates a ``Thread`` and immediately detaches it. For example
instead of:

.. code-block:: cpp

  Thread(options, foo).detach();

You can instead use this helper wrapper to:

.. code-block:: cpp

   DetachedThread(options, foo);

The arguments are directly forwarded to the Thread constructor and ergo exactly
match the Thread constuctor arguments for creating a thread of execution.


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

-----------------------
pw_snapshot integration
-----------------------
``pw_thread`` provides some light, optional integration with pw_snapshot through
helper functions for populating a ``pw::thread::Thread`` proto. Some of these
are directly integrated into the RTOS thread backends to simplify the thread
state capturing for snapshots.

SnapshotStack()
===============
The ``SnapshotStack()`` helper captures stack metadata (stack pointer and
bounds) into a ``pw::thread::Thread`` proto. After the stack bounds are
captured, execution is passed off to the thread stack collection callback to
capture a backtrace or stack dump. Note that this function does NOT capture the
thread name: that metadata is only required in cases where a stack overflow or
underflow is detected.

Python processor
================
Threads captured as a Thread proto message can be dumped or further analyzed
using using ``pw_thread``'s Python module. This is directly integrated into
pw_snapshot's processor tool to automatically provide rich thread state dumps.

The ``ThreadSnapshotAnalyzer`` class may also be used directly to identify the
currently running thread and produce symbolized thread dumps.

.. Warning::
  Snapshot integration is a work-in-progress and may see significant API
  changes.
