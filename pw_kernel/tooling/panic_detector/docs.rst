.. _module-pw_kernel-tooling-panic-detector:

==============
panic_detector
==============
.. pigweed-module-subpage::
   :name: pw_kernel

.. _module-pw_kernel-tooling:

panic_detector will scan a rust binary and through static analysis display a list of all panic call sites.

Currently only riscv32 elf files are supported.

-----
Usage
-----

panic_detector can be integrated into the bazel build as follows:

1. Add panic handler
--------------------

Add a panic handler to the rust binary which will be used by panic_detector to find the panic call sites.

.. code-block:: rust

   #[panic_handler]
   fn panic_handler(info: &core::panic::PanicInfo) -> ! {
       if let Some(location) = info.location() {
           panic_is_possible(location.file().as_ptr(), location.file().len(), location.line(), location.column());
       } else {
           panic_is_possible(core::ptr::null(), 0, 0, 0);
       }
   }

   #[no_mangle]
   #[inline(never)]
   extern "C" fn panic_is_possible(filename: *const u8, filename_len: usize, line: u32, col: u32) -> !{
       // The arguments to this function are reverse-engineereddocs.html
       // from the machine code by static analysis to
       // display a list of all the panic call-sites to the
       // user in the mutask_no_panic_test() error message.
       // See welder/src/check_panic.rs for more details.

       // If this symbol exists in the binary, panics are
       // possible. Presubmit tests can ensure that this symbol
       // does not exist in the final binary.  Do not rename or
       // remove this function.
       core::hint::black_box(filename);
       core::hint::black_box(filename_len);
       core::hint::black_box(line);
       core::hint::black_box(col);
       loop {}
   }

2. Add a test target
--------------------

Add a test target to bazel for the binary to validate.

.. code-block:: bazel

   rust_binary(
       name = "example",
       srcs = ["main.rs"],
   )

   load("//pw_kernel/tooling/panic_detector:rust_binary_no_panics_test.bzl", "rust_binary_no_panics_test")

   rust_binary_no_panics_test(
       name = "example_no_panic_test",
       binary = ":example",
   )

3. Run the bazel test
---------------------

If any panics are detected panic_detector will print the locations to stdout.

.. code-block:: shell

   $ bazel test //:example_no_panic_test
   <snip>
   Found panic ufmt-0.2.0/src/helpers.rs line 58 column 13. Branch trace:
     00103896 lui      a0,0x104           (task0_entry)
     0010389e jal      ra,-1734           (task0_entry)
     0010320a jalr     ra,ra,-82          (panic_const_add_overflow)
     001031d2 jalr     ra,ra,-78          (core::panicking::panic_fmt)
     00103192 jal      ra,2               (rust_begin_unwind)
     00103194 addi     sp,sp,-16          (panic_is_possible)
     <snip>
