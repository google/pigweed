.. _showcase-sense-tutorial-intro:

================================
Sense tutorial (Tour of Pigweed)
================================
Welcome to the :ref:`Sense <showcase-sense>` tutorial! If you want a hands-on,
guided tour of Pigweed's key features, you're in the right place.

.. _Bazel quickstart: https://cs.opensource.google/pigweed/quickstart/bazel

.. note::

   If you'd like to start a Bazel-based project from scratch from a
   minimal starter repo, check out our `Bazel quickstart`_.

.. _showcase-sense-tutorial-intro-expectations:

--------------
What to expect
--------------
Here's an overview of what your tutorial experience will include:

#. You set up your computer (the "development host" or "host" for short)
   so that it's ready to build the project's source code, flash your Pico,
   and so on.
#. You run a simulated version of unit tests and a basic bringup program
   on your host.
#. You run more complex programs on a physical Pico and perform more
   complex tasks, e.g. communicating with the Pico through a web app and
   debugging crash snapshots.

.. _showcase-sense-tutorial-intro-prereqs:

-------------
Prerequisites
-------------
Please read over these prerequisites and make sure the tutorial is a good
fit for you:

.. _Raspberry Pi Pico: https://www.raspberrypi.com/documentation/microcontrollers/pico-series.html
.. _Pimoroni Enviro+ Pack: https://shop.pimoroni.com/products/pico-enviro-pack
.. _Debug Probe: https://www.raspberrypi.com/products/debug-probe/
.. _Omnibus: https://shop.pimoroni.com/products/pico-omnibus

* **macOS or Linux development host**: Windows isn't supported with this particular
  tutorial yet.

* **Hardware setups** (all of the following setups are supported):

  * (Recommended) `Raspberry Pi Pico`_, `Pimoroni Enviro+ Pack`_, `Debug Probe`_,
    and `Omnibus`_: You'll be able to complete 100% of the tutorial.

  * Raspberry Pi Pico and Enviro+: You'll be able to complete ~80% of the
    tutorial.

  * Raspberry Pi Pico only: You'll be able to complete ~60% of the tutorial.

  * No hardware (development host only): You'll be able to complete ~40%
    of the tutorial. Pigweed provides a way to emulate the app
    on your host. You'll need to stop at :ref:`showcase-sense-tutorial-flash`.

* **Embedded development experience**: We assume that you're comfortable
  with C++ and common tasks such as flashing a Pico over USB. You may still
  be able to complete the tutorial without this background knowledge but should
  expect the tutorial to be more challenging.

.. _showcase-sense-tutorial-intro-pico:

Supported Pico versions
=======================
You can use any version of the Pico. We support them all:

* Pico 1
* Pico 1W
* Pico 2
* Pico 2W

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
