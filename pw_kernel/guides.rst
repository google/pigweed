.. _module-pw_kernel-guides:

======
Guides
======
.. pigweed-module-subpage::
   :name: pw_kernel

.. note::

   This is an early draft. The content may change significantly over the
   next few months.

This section provides guides for specific features and tools within ``pw_kernel``.

.. _module-pw_kernel-guides-unit-testing:

------------
Unit testing
------------
``pw_kernel`` includes a lightweight unit testing framework designed for both
bare-metal and kernel-aware tests. This framework is crucial for ensuring the
correctness and reliability of kernel primitives and application code.

Writing a test
==============
Tests are written as standard Rust functions annotated with the ``#[test]``
attribute from the ``unittest`` crate.
The test function should return a ``unittest::Result<()>``.

Here's a simple example of a bare-metal test:

.. code-block:: rust

   // in your_module/lib.rs or your_module/tests.rs
   #[cfg(test)]
   mod tests {
       use unittest::test;

       fn add(a: u32, b: u32) -> u32 {
           a + b
       }

       #[test]
       fn test_addition() -> unittest::Result<()> {
           unittest::assert_eq!(add(2, 2), 4);
           unittest::assert_ne!(add(2, 2), 5);
           unittest::assert_true!(add(1,1) == 2);
           Ok(())
       }
   }

Assertions
==========
The ``unittest`` crate provides several assertion macros:

- ``unittest::assert_eq!(a, b)``: Asserts that ``a`` is equal to ``b``.
- ``unittest::assert_ne!(a, b)``: Asserts that ``a`` is not equal to ``b``.
- ``unittest::assert_true!(expr)``: Asserts that ``expr`` evaluates to true.
- ``unittest::assert_false!(expr)``: Asserts that ``expr`` evaluates to false.

Kernel-aware tests
==================
For tests that require kernel services (e.g., testing scheduler behavior, mutexes,
or timers), you can mark them as needing the kernel by passing the
``needs_kernel`` argument to the ``#[test]`` attribute:

.. code-block:: rust

   use kernel::sync::mutex::Mutex;
   use unittest::test;

   static MY_MUTEX: Mutex<u32> = Mutex::new(0);

   #[test(needs_kernel)]
   fn test_mutex_locking() -> unittest::Result<()> {
       let guard = MY_MUTEX.lock();
       unittest::assert_eq!(*guard, 0);
       // guard is dropped here, unlocking the mutex
       Ok(())
   }

Running tests
=============
Tests are typically run via Bazel. See :ref:`module-pw_kernel-quickstart-test`.

The test runner executes all discovered tests and reports their status.
Bare-metal tests are run first, followed by kernel-aware tests if the kernel
is initialized.

.. _module-pw_kernel-guides-panic-detector:

--------------
Panic detector
--------------
``pw_kernel`` includes a tool called ``panic detector`` to statically
analyze a compiled Rust binary (ELF file) to identify all potential panic
locations. This is crucial for ensuring reliability and can significantly
reduce code size by eliminating panic-handling overhead.

For detailed instructions on how to integrate and use this tool, see
:ref:`module-pw_kernel-tooling-panic-detector`.

.. _module-pw_kernel-guides-intrusive-lists:

----------------------
Intrusive linked lists
----------------------
.. _//pw_kernel/lib/list: https://cs.opensource.google/pigweed/pigweed/+/main:pw_kernel/lib/list/

``pw_kernel`` provides a highly efficient and safe intrusive linked list
implementation in `//pw_kernel/lib/list`_. This is a fundamental data structure
used throughout the kernel, particularly in the scheduler for managing threads
in run queues and wait queues.

Example usage
=============
.. code-block:: rust

   use kernel::lib::list::{self, Link, ForeignList, ForeignBox};
   use core::ptr::NonNull;

   // 1. Define your struct with an embedded `Link`.
   struct MyListItem {
       data: u32,
       list_link: Link, // The intrusive link
   }

   // 2. Define an adapter using the `define_adapter!` macro.
   //    This connects `MyListItem` and its `list_link` field to the list logic.
   list::define_adapter!(MyListItemAdapter => MyListItem.list_link);

   fn main_example() {
       // Create a list that can hold `MyListItem`s.
       let mut my_list = ForeignList::<MyListItem, MyListItemAdapter>::new();

       // Create some items. In a real scenario, these might be ForeignBox::new_from_ptr
       // from statically allocated buffers or a dedicated allocator.
       // For simplicity, we'll imagine they are correctly managed ForeignBox instances.
       // Note: ForeignBox requires that the underlying memory is valid for its
       // entire lifetime and that it is not deallocated by other means.
       let mut item1_storage = MyListItem { data: 10, list_link: Link::new() };
       let item1 = unsafe { ForeignBox::new_from_ptr(&mut item1_storage) };

       let mut item2_storage = MyListItem { data: 20, list_link: Link::new() };
       let item2 = unsafe { ForeignBox::new_from_ptr(&mut item2_storage) };

       // Add items to the list.
       my_list.push_back(item1);
       my_list.push_front(item2); // item2 is now at the head.

       // Iterate and access items (ForeignList provides safe iteration).
       my_list.for_each(|item| {
           // pw_log::info!("Item data: {}", item.data);
           Ok::<(), ()>(()) // Placeholder Ok for the closure
       }).unwrap();

       // Pop an item.
       if let Some(popped_item) = my_list.pop_head() {
           // pw_log::info!("Popped item data: {}", popped_item.data);
           // IMPORTANT: The popped_item (a ForeignBox) must be consumed
           // or it will panic on drop, ensuring ownership is handled.
           popped_item.consume();
       }

       // Clean up remaining items
       while let Some(item) = my_list.pop_head() {
           item.consume();
       }
   }

Considerations
--------------
.. _//pw_kernel/kernel/scheduler.rs: https://cs.opensource.google/pigweed/pigweed/+/main:pw_kernel/kernel/scheduler.rs

- **ForeignBox**: The ``ForeignBox`` type is a smart pointer that ensures the
  underlying memory is valid for its entire lifetime. It is crucial to call
  ``consume()`` on a ``ForeignBox`` when it is no longer needed to prevent panics
  on drop.
- ``UnsafeList`` **is unsafe**: Requires careful handling to prevent dangling
  pointers or double-frees if not using ``ForeignList``.
- **Single List Membership**: An item can only be part of one list at a time
  using a single ``Link`` member.

The intrusive list is a powerful tool for performance-critical data structures
within the kernel. You'll see it used in `//pw_kernel/kernel/scheduler.rs`_ for
managing thread queues.
