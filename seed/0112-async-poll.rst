.. _seed-0112:

======================
0112: Async Poll Model
======================
.. seed::
   :number: 0112
   :name: Async Poll Model
   :status: Accepted
   :proposal_date: 2023-9-19
   :cl: 168337

-------
Summary
-------
This SEED proposes the development of a new “informed-Poll”-based pw::async
library. The “informed Poll” model, popularized by
`Rust’s Future trait, <https://doc.rust-lang.org/std/future/trait.Future.html>`_
offers an alternative to callback-based APIs. Rather than invoking a separate
callback for every event, the informed Poll model runs asynchronous ``Task`` s.

A ``Task`` is an asynchronous unit of work. Informed Poll-based asynchronous
systems use ``Task`` s similar to how synchronous systems use threads.
Users implement the ``Poll`` function of a ``Task`` in order to define the
asynchronous behavior of a routine.

.. code-block:: cpp

   class Task {
    public:
     /// Does some work, returning ``Complete`` if done.
     /// If not complete, returns ``Pending`` and arranges for `cx.waker()` to be
     /// awoken when `Task:Poll` should be invoked again.
     virtual pw::MaybeReady<Complete> Poll(pw::async::Context& cx);
   };

Users can start running a ``Task`` by ``Post`` ing it to a ``Dispatcher``.
``Dispatcher`` s are asynchronous event loops which are responsible for calling
``Poll`` every time the ``Task`` indicates that it is ready to make progress.

This API structure allows Pigweed async code to operate efficiently, with low
memory overhead, zero dynamic allocations, and simpler state management.

Pigweed’s new async APIs will enable multi-step asynchronous operations without
queuing multiple callbacks. Here is an example in which a proxy object receives
data and then sends it out before completing:

.. code-block:: cpp

   class ProxyOneMessage : public Task {
    public:
     /// Proxies one ``Data`` packet from a ``Receiver`` to a ``Sender``.
     ///
     /// Returns:
     ///   ``pw::async::Complete`` when the task has completed. This happens
     ///     after a ``Data`` packet has been received and sent, or an error
     ///     has occurred and been logged.
     ///   ``pw::async::Pending`` if unable to complete. ``cx.waker()`` will be
     ///     awoken when ``Poll`` should be re-invoked.
     pw::async::MaybeReady<pw::async::Complete> Poll(pw::async::Context& cx) {
       if (!send_future_) {
         // ``PollRead`` checks for available data or errors.
         pw::async::MaybeReady<pw::Result<Data>> new_data = receiver_.PollRead(cx);
         if (new_data.is_pending()) {
           return pw::async::Pending;
         }
         if (!new_data->ok()) {
           PW_LOG_ERROR("Receiving failed: %s", data->status().str());
           return pw::async::Complete;
         }
         Data& data = **new_data;
         send_future_ = sender_.Send(std::move(data));
       }
       // ``PollSend`` attempts to send `data_`, returning `Pending` if
       // `sender_` was not yet able to accept `data_`.
       pw::async::MaybeReady<pw::Status> sent = send_future_.Poll(cx);
       if (sent.is_pending()) {
         return pw::async::Pending;
       }
       if (!sent->ok()) {
         PW_LOG_ERROR("Sending failed: %s", sent->str());
       }
       return pw::async::Complete;
     }

    private:
     // ``SendFuture`` is some type returned by `Sender::Send` that offers a
     // ``Poll`` method similar to the one on ``Task``.
     std::optional<SendFuture> send_future_;
     // `receiver_` and `sender_` are provided by the `ProxyOneMessage` constructor.
     Receiver receiver_;
     Sender sender_;
   };

   // --- Usage ---
   // ``static`` is used for simplicity, but real ``Task`` s can have temporary
   // lifetimes.
   static ProxyOneMessage proxy(receiver, sender);

   // Runs `proxy` until it completes, either by successfully receiving and
   // sending a message, or by exiting early after logging an error.
   dispatcher.Post(proxy);

--------
Proposal
--------
This SEED proposes that Pigweed develop a set of async APIs and utilities
designed around the informed Poll model. If early trials with partner teams are
successful, this new library will be used as the basis for future async code in
Pigweed.

-----
Goals
-----
The goals of this SEED are as follows:

* Establish community consensus that informed ``Poll`` is the best async model
  for Pigweed to pursue.
* Outline an initial API for ``Dispatcher`` implementors (platform authors) and
  top-level ``Task`` writers.

----------
Motivation
----------
The purpose of this SEED is to gather agreement that ``Poll``-based async
APIs are worth pursuing. We believe that these APIs provide the needed support
for:

* Small code size
* Environments without dynamic allocation
* Creating reusable building blocks and high-level modules

The current ``Task`` API is limited in these respects: a single ``Task`` must
be created and stored for every individual asynchronous event. ``Task`` s
cannot be reused, and the memory allocated for a ``Task`` can only be reclaimed
after a ``Task`` has been completed or cancelled, resulting in complex
semantics for multithreaded environments or those with interrupt-driven events.

Completing a sequence of events therefore requires either dynamic allocation
or statically saving a separate ``Task`` worth of memory for every kind of
event that may occur.

Additionally, every asynchronous layer requires introducing another round of
callbacks whose semantics may be unclear and whose captures may add lifetime
challenges.

This proposal resolves these issues by choosing an alternative approach.

-----------
API Summary
-----------

A Note On Specificity
=====================
This SEED provides API outlines in order to more clearly explain the intended
API direction. The specific function signatures shown here are not meant to be
authoritative, and are subject to change. As the implementation develops
support for more platforms and features, some additions, changes, or removals
may be necessary and will be considered as part of the regular CL review
process.

With that in mind, asynchronous ``Task`` s in this model could adopt an API
like the following:

The ``MaybeReady`` Type
=======================
Functions return ``MaybeReady<T>`` to indicate that their result may or may
not be available yet. ``MaybeReady<T>`` is a generic sum type similar to
``std::optional<T>``. It has two variants, ``Ready(T)`` or ``Pending``.

The API is similar to ``std::optional<T>``, but ``MaybeReady<T>`` provides extra
semantic clarification that the absense of a value means that it is not ready
yet.

Paired with the ``Complete`` type, ``MaybeReady<Complete>`` acts like
``bool IsComplete``, but provides more semantic information to the user than
returning a simple ``bool``.

.. code-block:: cpp

   /// A value that is ready, and
   template<typename T>
   struct Ready<T> { value: T };

   /// A content-less struct that indicates a not-ready value.
   struct Pending {};

   /// A value of type `T` that is possibly available.
   ///
   /// This is similar to ``std::optional<T>``, but provides additional
   /// semantic indication that the value is not ready yet (still pending).
   /// This can aid in making type signatures such as
   /// ``MaybeReady<std::optional<Item>>`` easier to understand, and provides
   /// clearer naming like `IsReady` (compared to ``has_value()``).
   template<typename T>
   class MaybeReady {
    public:
     /// Implicitly converts from ``T``,  ``Ready<T>`` or ``Pending``.
     MaybeReady(T);
     MaybeReady(Ready<T>);
     MaybeReady(Pending);
     bool IsReady();
     T Value() &&;
     ...
   };

   /// A content-less struct that indicates completion.
   struct Complete {};

Note that the ``Pending`` type takes no type arguments, and so can be created
and returned from macros that don't know which ``T`` is returned by the
function they are in. For example:

.. code-block:: cpp

   // Simplified assignment macro
   #define PW_ASSIGN_IF_READY(lhs, expr) \
     auto __priv = (expr);               \
     if (!__priv.IsReady()) {            \
       return pw::async::Pending;        \
     }                                   \
     lhs = std::move(__priv.Value())     \

   MaybeReady<Bar> PollCreateBar(Context& cx);

   Poll<Foo> DoSomething(Context& cx) {
     PW_ASSIGN_IF_READY(Bar b, PollCreateBar(cx));
     return CreateFoo();
   }

This is similar to the role of the ``std::nullopt_t`` type.

The ``Dispatcher`` Type
=======================
Dispatchers are the event loops responsible for running ``Task`` s. They sleep
when there is no work to do, and wake up when there are ``Task`` s ready to
make progress.

On some platforms, the ``Dispatcher`` may also provide special hooks in order
to support single-threaded asynchronous I/O.

.. code-block:: cpp

   class Dispatcher {
    public:
     /// Tells the ``Dispatcher`` to run ``Task`` to completion.
     /// This method does not block.
     ///
     /// After ``Post`` is called, ``Task::Poll`` will be invoked once.
     /// If ``Task::Poll`` does not complete, the ``Dispatcher`` will wait
     /// until the ``Task`` is "awoken", at which point it will call ``Poll``
     /// again until the ``Task`` completes.
     void Post(Task&);
     ...
   };

The ``Waker`` Type
==================
A ``Waker`` is responsible for telling a ``Dispatcher`` when a ``Task`` is
ready to be ``Poll`` ed again. This allows ``Dispatcher`` s to intelligently
schedule calls to ``Poll`` rather than retrying in a loop (this is the
"informed" part of "informed Poll").

When a ``Dispatcher`` calls ``Task::Poll``, it provides a ``Waker`` that will
enqueue the ``Task`` when awoken. ``Dispatcher`` s can implement this
functionality by having ``Waker`` add the ``Task`` to an intrusive linked list,
add a pointer to the ``Task`` to a ``Dispatcher``-managed vector, or by pushing
a ``Task`` ID onto a system-level async construct such as ``epoll``.

.. code-block:: cpp

   /// An object which can respond to asynchronous events by queueing work to
   /// be done in response, such as placing a ``Task`` on a ``Dispatcher`` loop.
   class Waker {
    public:
     /// Wakes up the ``Waker``'s creator, alerting it that an asynchronous
     /// event has occurred that may allow it to make progress.
     ///
     /// ``Wake`` operates on an rvalue reference (``&&``) in order to indicate
     /// that the event that was waited on has been completed. This makes it
     /// possible to track the outstanding events that may cause a ``Task`` to
     /// wake up and make progress.
     void Wake() &&;

     /// Creates a second ``Waker`` from this ``Waker``.
     ///
     /// ``Clone`` is made explicit in order to allow for easier tracking of
     /// the different ``Waker``s that may wake up a ``Task``.
     Waker Clone(Token wait_reason_indicator) &;
     ...
   };

The ``Wake`` function itself may be called by any system with knowledge that
the ``Task`` is now ready to make progress. This can be done from an interrupt,
from a separate task, from another thread, or from any other function that
knows that the `Poll`'d type may be able to make progress.

The ``Context`` Type
====================
``Context`` is a bundle of arguments supplied to ``Task::Poll`` that give the
``Task`` information about its asynchronous environment. The most important
parts of the ``Context`` are the ``Dispatcher``, which is used to ``Post``
new ``Task`` s, and the ``Waker``, which is used to tell the ``Dispatcher``
when to run this ``Task`` again.

.. code-block:: cpp

   class Context {
    public:
     Context(Dispatcher&, Waker&);
     Dispatcher& Dispatcher();
     Waker& Waker();
     ...
   };

The ``Task`` Type
=================
Finally, the ``Task`` type is implemented by users in order to run some
asynchronous work. When a new asynchronous "thread" of execution must be run,
users can create a new ``Task`` object and send it to be run on a
``Dispatcher``.

.. code-block:: cpp

   /// A task which may complete one or more asynchronous operations.
   ///
   /// ``Task`` s should be actively ``Poll`` ed to completion, either by a
   /// ``Dispatcher`` or by a parent ``Task`` object.
   class Task {
    public:
     MaybeReady<Complete> Poll(Context&);
     ...
    protected:
     /// Returns whether or not the ``Task`` has completed.
     ///
     /// If the ``Task`` has not completed, `Poll::Pending` will be returned,
     /// and `context.Waker()` will receive a `Wake()` call when the ``Task``
     /// is ready to make progress and should be ``Poll`` ed again.
     virtual MaybeReady<Complete> DoPoll(Context&) = 0;
     ...
   };

This structure makes it possible to run complex asynchronous ``Task`` s
containing multiple concurrent or sequential asynchronous events.

------------------------------------
Relationship to Futures and Promises
------------------------------------
The terms "future" and "promise" are unfortunately quite overloaded. This SEED
does not propose a "method chaining" API (e.g. ``.AndThen([](..) { ... }``), nor
is creating reference-counted, blocking handles to the output of other threads
a la ``std::future``.

Where this SEED refers to ``Future`` types (e.g. ``SendFuture`` in the summary
example), it means only a type which offers a ``Poll(Context&)`` method and
return some ``MaybeReady<T>`` value. This common pattern can be used to build
various asynchronous state machines which optionally return a value upon
completion.

---------------------------------------------
Usage In The Rust Ecosystem Shows Feasability
---------------------------------------------
The ``Poll``-based ``Task`` approach suggested here is similar to the one
adopted by Rust's
`Future type <https://doc.rust-lang.org/stable/std/future/trait.Future.html>`_.
The ``Task`` class in this SEED is analogous to Rust's ``Future<Output = ()>``
type. This model has proven usable on small environments without dynamic allocation.

Due to compiler limitations, Rust's ``async fn`` language feature will often
generate ``Future`` s which suffer from code size issues. However,
manual implementations of Rust's ``Future`` trait (not using ``async fn``) do
not have this issue.

We believe the success of Rust's ``Poll``-based ``Future`` type demonstrates
that the approach taken in this SEED can meet the needs of Pigweed users.

---------
Code Size
---------
`Some experiments have been done
<https://pigweed-review.googlesource.com/c/pigweed/experimental/+/154570>`_
to compare the size of the code generated by
a ``Poll``-based approach with code generated with the existing ``pw::async``
APIs. These experiments have so far found that the ``Poll``-based approach
creates binaries with smaller code size due to an increased opportunity for
inlining, static dispatch, and a smaller number of separate ``Task`` objects.

The experimental ``pw_async_bench`` examples show that the ``Poll``-based
approach offers more than 2kB of savings on a small ``Socket``-like example.

------------------------
The ``pw::async`` Facade
------------------------
This SEED proposes changing ``Dispatcher`` from a virtual base into a
platform-specific concrete type.

The existing ``pw::async::Dispatcher`` class is ``virtual`` in order to support
use of an alternative ``Dispatcher`` implementation in tests. However, this
approach assumes that ``Task`` s are capable of running on arbitrary
implementations of the ``Dispatcher`` virtual interface. In practice, this is
not the case.

Different platforms will use different native ``Dispatcher`` waiting primitives
including ``epoll``, ``kqueue``, IOCP, Fuchsia's ``libasync``/``zx_port``, and
lower-level waiting primitives such as Zephyr's RTIO queue.

Each of these primitives is strongly coupled with native async events, such as
IO or buffer readiness. In order to support ``Dispatcher``-native IO events,
IO objects must be able to guarantee that they are running on a compatible
``Dispatcher``. In Pigweed, this can be accomplished through the use of the
facade pattern.

The facade patterns allows for concrete, platform-dependent definitions of the
``Task``, ``Context``, ``Waker``, and ``Dispatcher`` types. This allows these
objects to interact with one another as necessary to implement fast scheduling
with minimal in-memory or code size overhead.

This approach enables storing platform-specific per- ``Task`` scheduling details
inline with the ``Task`` itself, enabling zero-allocation ``Task`` scheduling
without the need for additional resource pools.

This also allows for native integration with platform-specific I/O primitives
including ``epoll``, ``kqueue``, IOCP, and others, but also lower-level
waiting primitives such as Zephyr's RTIO queue.

Testing
=======
Moving ``Dispatcher`` to a non-virtual facade means that the previous approach
of testing with a ``FakeDispatcher`` would require a separate toolchain in
order to provide a different instantiation of the ``Dispatcher`` type. However,
we can adopt a simpler approach: the ``Dispatcher`` type can offer minimial
testing primitives natively:

.. code-block:: cpp

   class Dispatcher {
    public:
     ...

     /// Runs tasks until none are able to make immediate progress.
     ///
     /// Returns whether a ``Task`` was run.
     bool RunUntilStalled();

     /// Enable mock time, initializing the mock timer to some "zero"-like
     /// value.
     void InitializeMockTime();

     /// Advances the mock timer forwards by ``duration``.
     void AdvanceMockTime(chrono::SystemClock::duration duration);
   };

These primitives are sufficient for testing with mock time. They allow
test authors to avoid deadlocks, timeouts, or race conditions.

Downsides of Built-in Testing Functions
---------------------------------------
Requiring concrete ``Dispatcher`` types to include the testing functions above
means that the production ``Dispatcher`` implementations will have code in them
that is only needed for testing.

However, these additions are minimal: mocking time introduces a single branch
for each timer access, which is still likely to be more efficient than the
virtual function call that was required under the previous model.

Advantages of Built-in Testing Functions
----------------------------------------
Testing with a "real" ``Dispatcher`` implementation ensures that:

* All ``pw::async`` platforms provide support for testing
* The ``Dispatcher`` used for testing will support the same I/O operations and
  features provided by the production ``Dispatcher``
* Tests will run under conditions as-close-to-production as possible. This will
  allow catching bugs that are caused by the interaction of the code and the
  particular ``Dispatcher`` on which it runs.

Enabling Dynamic ``Task`` Lifetimes
===================================
While some ``Task`` s may be static, others may not be. For these, we need a
mechanism to ensure that:

* ``Task`` resources are not destroyed while ``Waker`` s that may post them
  to a ``Dispatcher`` remain.
* ``Task`` resources are not destroyed while the ``Task`` itself is running
  or is queued to run.

In order to enable this, platforms should clear all ``Waker`` s referencing a
``Task`` when the ``Task`` completes: that ``Task`` will make no further
progress, so ``Wake`` ing it serves no purpose.

Once all ``Waker`` s have been cleared and the ``Task`` has finished running
on the ``Dispatcher``, the ``Dispatcher`` should call that ``Task`` s
``Cleanup`` function so that the ``Task`` can free any associated dynamic
resources. During this ``Cleanup`` function, no other resources of ``Task``
may be accessed by the application author until the ``Task`` has been
re-initialized. If the memory associated with the ``Task`` is to be reused,
the ``Task`` object itself must be reinitialized by invoking the ``Init``
function.

.. code-block:: cpp

   class Task {
    public:
     ...
     void Init();
     virtual void Cleanup();
     ...
   };

This allows downstream ``Task`` inheritors to implement dynamic free-ing of
``Task`` resources, while also allowing the ``Dispatcher`` implementation the
opportunity to clean up its own resources stored inside of the ``Task`` base
class.

Waker
=====
``Waker`` s will at first only be created via the ``Dispatcher``
implementation, via cloning, or by the null constructor. Later on, the API may
be expanded to allow for waking sub-tasks. The necessity of this at Pigweed's
scale has not yet been determined.

Timer
=====
``pw::async`` will additionally provide a ``Timer`` type. A ``Timer`` can be
``Poll``'d by a ``Task`` in order to determine if a certain amount of time has
passed. This can be used to implement timeouts or to schedule work.

One possible ``Timer`` API would be as follows:

.. code-block:: cpp

   class Timer {
    public:
     Timer(Context&, chrono::SystemClock::time_point deadline);
     Timer(Context&, chrono::SystemClock::duration delay);
     pw::MaybeReady<Complete> Poll(Context&);
     ...
   };

In order to enable this, the ``Dispatcher`` base class will include the
following functions which implementations should use to trigger timers:

.. code-block:: cpp

   class DispatcherBase {
    public:
     ...
    protected:
     /// Returns the time of the earliest timer currently scheduled to fire.
     std::optional<chrono::SystemClock::time_point> EarliestTimerExpiry();

     /// Marks all ``Timer`` s with a time before ``time_point`` as complete,
     /// and awakens any associated tasks.
     ///
     /// Returns whether any ``Timer`` objects were marked complete.
     bool AwakenTimersUpTo(chrono::SystemClock::time_point);

     /// Invoked when a new earliest ``Timer`` is created.
     ///
     /// ``Dispatcher`` implementations can override this to receive
     /// notifications when a new timer is added.
     virtual void NewEarliestTimer();
     ...
   };

---------------------
C++ Coroutine Support
---------------------
The informed ``Poll`` approach is well-suited to
`C++20's coroutines <https://en.cppreference.com/w/cpp/language/coroutines>`_.
Coroutines using the ``co_await`` and ``co_return`` expressions can
automatically create and wait on ``Task`` types, whose base class will
implement the ``std::coroutine_traits`` interface on C++20 and later.

Dynamic Allocation
==================
Note that C++ coroutines allocate their state dynamically using
``operator new``, and therefore are not usable on systems in which dynamic
allocation is not available or where recovery from allocation failure is
required.

------------
Rust Interop
------------
Rust uses a similar informed ``Poll`` model for its ``Future`` trait. This
allows ``pw::async`` code to invoke Rust-based ``Future`` s by creating a
Rust ``Waker`` which invokes the C++ ``Waker``, and performing cross-language
``Poll`` ing.

Rust support is not currently planned for the initial version of ``pw::async``,
but will likely come in the future as Pigweed support for Rust expands.

------------------------------------------------
Support for Traditional Callback-Style Codebases
------------------------------------------------
One concern is interop with codebases which adopt a more traditional
callback-driven design, such as the one currently supported by ``pw::async``.
These models will continue to be supported under the new design, and can be
modeled as a ``Task`` which runs a single function when ``Poll`` ed.

---------
Migration
---------
For ease of implementation and in order to ensure a smooth transition, this API
will initially live alongside the current ``pw::async`` interface. This API
will first be tested with one or more trial usages in order to stabilize the
interface and ensure its suitability for Pigweed users.

Following that, the previous ``pw::async`` implementation will be deprecated.
A shim will be provided to allow users of the previous API to easily migrate
their code onto the new ``pw::async`` implementation. After migrating to the
new implementation, users can gradually transition to the new ``Poll``-based
APIs as-desired. It will be possible to intermix legacy-style and
``Poll``-based async code within the same dispatcher loop, allowing legacy
codebases to adopt the ``Poll``-based model for new subsystems.
