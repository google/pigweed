.. _module-pw_fuzzer:

=========
pw_fuzzer
=========
.. pigweed-module::
   :name: pw_fuzzer
   :tagline: Better C++ code through easier fuzzing
   :status: unstable
   :languages: C++14, C++17, C++20

Use state of the art tools to automatically find bugs in your C++ code with 5
lines of code or less!


.. code-block:: cpp

   FUZZ_TEST(MyTestSuite, TestMyInterestingFunction)
     .WithDomains(Arbitrary<size_t>(), AsciiString());

----------
Background
----------
You've written some code. You've written unit tests for that code. The unit
tests pass. But could there be bugs in inputs or code paths the unit tests do
not cover? `Fuzzing`_ can help!

However, fuzzing requires some complex interactions between compiler-added
`instrumentation`_, `compiler runtimes`_, code under test, and the fuzzer code
itself. And to be reliably useful, fuzzers need to be part of a continuous
fuzzing infrastructure, adding even more complexity.

See :ref:`module-pw_fuzzer-concepts` to learn more about the different
components of a fuzzer and how they work together to discover hard-to-find bugs.

------------
Our solution
------------
``pw_fuzzer`` makes it easier to write, build, run, and deploy fuzzers. It
provides convenient integration with two fuzzing `engines`_:

* `libFuzzer`_, allowing integration of legacy fuzzers.

* `FuzzTest`_, allowing easy creation of fuzzers from unit tests in around 5
  lines of code.

Additionally, it produces artifacts for continuous fuzzing infrastructures such
as `ClusterFuzz`_ and `OSS-Fuzz`_, and provides the artifacts those systems need.

---------------
Who this is for
---------------
``pw_fuzzer`` is useful for authors of `wide range`_ of C++ code who have
written unit tests and are interested in finding actionable bugs in their code.

In particular, coverage-guided is effective for testing and finding bugs in code
that:

* Receives inputs from **untrusted sources** and must be secure.

* Has **complex algorithms** with some equivalence, e.g. compress and
  decompress, and must be correct.

* Handles **high volumes** of inputs and/or unreliable dependencies and must be
  stable.

Fuzzing works best when code handles inputs deterministically, that is, given
the same input it behaves the same way. Fuzzing will be less effective with code
that modifies global state or has some randomness, e.g. depends on how multiple
threads are scheduled. Simply put, good fuzzers typically resemble unit tests in
terms of scope and isolation.

--------------------
Is it right for you?
--------------------
Currently, ``pw_fuzzer`` only provides support for fuzzers that:

* Run on **host**. Sanitizer runtimes such as `AddressSanitizer`_ add
  significant memory overhead and are not suitable for running on device.
  Additionally, the currently supported engines assume the presence of certain
  POSIX features.

* Are built with **Clang**. The `instrumentation`_ used in fuzzing is added by
  ``clang``.

.. _module-pw_fuzzer-get-started:

---------------
Getting started
---------------
The first step in adding a fuzzer is to determine what fuzzing engine should you
use. Pigweed currently supports two fuzzing engines:

* **FuzzTest** is the recommended engine. It makes it easy to create fuzzers
  from your existing unit tests, but it does requires additional third party
  dependencies and at least C++17. See
  :ref:`module-pw_fuzzer-guides-using_fuzztest` for details on how to set up a
  project to use FuzzTest and on how to create and run fuzzers with it.

* **libFuzzer** is a mature, proven engine. It is a part of LLVM and requires
  code authors to implement a specific function, ``LLVMFuzzerTestOneInput``. See
  :ref:`module-pw_fuzzer-guides-using_libfuzzer` for details on how to write
  fuzzers with it.

-------
Roadmap
-------
``pw_fuzzer`` is under active development. There are a number of improvements we
would like to add, including:

* `b/282560789`_ - Document workflows for analyzing coverage and improving
  fuzzers.

* `b/280457542`_ - Add CMake support for FuzzTest.

* `b/281138993`_ - Add a ``pw_cli`` plugin for fuzzing.

* `b/281139237`_ - Develop OSS-Fuzz and ClusterFuzz workflow templates for
  downtream projects.

.. _b/282560789: https://issues.pigweed.dev/issues/282560789
.. _b/280457542: https://issues.pigweed.dev/issues/280457542
.. _b/281138993: https://issues.pigweed.dev/issues/281138993
.. _b/281139237: https://issues.pigweed.dev/issues/281139237

.. toctree::
   :maxdepth: 1
   :hidden:

   concepts
   guides/fuzztest
   guides/libfuzzer
   guides/reproducing_oss_fuzz_bugs

.. inclusive-language: disable
.. _AddressSanitizer: https://github.com/google/sanitizers/wiki/AddressSanitizer
.. _ClusterFuzz: https://github.com/google/clusterfuzz/
.. _compiler runtimes: https://compiler-rt.llvm.org/
.. _engines: https://github.com/google/fuzzing/blob/master/docs/glossary.md#fuzzing-engine
.. _Fuzzing: https://github.com/google/fuzzing/blob/master/docs/intro-to-fuzzing.md
.. _FuzzTest: https://github.com/google/fuzztest
.. _instrumentation: https://clang.llvm.org/docs/SanitizerCoverage.html#instrumentation-points
.. _libFuzzer: https://llvm.org/docs/LibFuzzer.html
.. _OSS-Fuzz: https://github.com/google/oss-fuzz
.. _wide range: https://github.com/google/fuzzing/blob/master/docs/why-fuzz.md
.. inclusive-language: enable
