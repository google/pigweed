.. _showcase-sense-tutorial-intro:

==============
Sense tutorial
==============
.. _repo: https://pigweed.googlesource.com/pigweed/showcase/sense/

Welcome to the :ref:`Sense <showcase-sense>` tutorial! If you want a hands-on,
guided tour of the Sense `repo`_, you're in the right place.

.. _Bazel quickstart: https://cs.opensource.google/pigweed/quickstart/bazel

.. note::

   If you'd like to start a Bazel-based project from scratch from a
   minimal starter repo, check out our `Bazel quickstart`_.

.. _showcase-sense-tutorial-intro-expectations:

--------------
What to expect
--------------
Here's a very high-level overview of what your tutorial experience
will include:

#. You set up your computer (the "development host" or "host" for short)
   so that it's ready to build the repo, flash binaries, and so on.
#. You run a simulated version of unit tests and a basic bringup program
   on your host.
#. You run gradually more and more complex programs on physical hardware.

.. _showcase-sense-tutorial-intro-prereqs:

-------------
Prerequisites
-------------
Please read over these prerequisites and make sure the tutorial is a good
fit for you:

.. _MicroPython Pico SDK: https://www.raspberrypi.com/documentation/microcontrollers/micropython.html
.. _C/C++ Pico SDK: https://www.raspberrypi.com/documentation/microcontrollers/c_sdk.html
.. _Pimoroni Enviro+ Pack: https://shop.pimoroni.com/products/pico-enviro-pack

* **macOS or Linux development host**: Windows isn't supported with this particular
  tutorial yet.

* **Hardware setups** (all of the following setups are supported):

  * (Recommended) Raspberry Pi Pico and `Pimoroni Enviro+ Pack`_: You'll be
    able to complete the full tutorial.

  * Raspberry Pi Pico only: You'll be able to complete most of the tutorial
    except the last parts that require an Enviro+ Pack.

  * No hardware (development host only): You can actually still try
    out some of the tutorial! Pigweed provides a way to emulate the app
    on your host. You'll need to stop at :ref:`showcase-sense-tutorial-flash`.

* **Embedded development experience**: We assume that you're comfortable
  with C++ and common tasks such as flashing a Pico over USB. You may still
  be able to complete the tutorial without this background knowledge but should
  expect the tutorial to be more challenging.

You can use either the Pico 1 or Pico 2; we support both.

.. _Pico W: https://www.raspberrypi.com/documentation/microcontrollers/raspberry-pi-pico.html#raspberry-pi-pico-w-and-pico-wh

.. caution::

   **The Pico W is untested**. We are still in the process of verifying that
   all parts of the tutorial work with the `Pico W`_. You are welcome to try
   the tutorial with a Pico W, but please remember that some things may not
   work yet.

.. _showcase-sense-tutorial-intro-summary:

-------
Summary
-------
In this tutorial you'll see many Pigweed features working together that can
help your team develop embedded systems more sustainably, robustly, and
rapidly.

Next, head over to :ref:`showcase-sense-tutorial-setup` to get your
development host ready to run Sense.

.. toctree::
   :maxdepth: 1
   :hidden:

   setup
   explore
   build
   code_intelligence
   host_tests
   host_sim
   flash
   device_tests
   rpc
   automate
   web
   factory
   bazel_cloud
   production
   crash_handler
   outro
