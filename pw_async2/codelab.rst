.. _module-pw_async2-codelab:

=======
Codelab
=======
Welcome to the ``pw_async2`` codelab!

This codelab provides a hands-on introduction to Pigweed's cooperative
asynchronous framework. You will build a simple, simulated "Vending Machine"
application, learning the core concepts of ``pw_async2`` along the way.

By the end of this codelab, you will know how to:

*   Implement a ``pw::async2::Task`` as a state machine.
*   Call asynchronous functions and manage state across suspension points.
*   Write your own pendable functions that use a ``Waker`` to handle external
    events.
*   Use ``pw::async2::OnceSender`` and ``pw::async2::OnceReceiver`` for basic
    inter-task communication.
*   Use ``pw::async2::TimeProvider`` and ``pw::async2::Select`` to implement
    timeouts.

Let's get started!
