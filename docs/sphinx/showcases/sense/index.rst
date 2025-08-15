.. _showcase-sense:

=========================================
Sense: An interactive tour though Pigweed
=========================================
Welcome to Pigweed Sense: **a tour of key Pigweed components experienced through
an imagined air quality monitor product**. Since Pigweed is intended for larger
teams and higher-complexity products, it’s important to experience Pigweed in a
medium-size project like Sense that shows a lot of Pigweed components working
together.

.. _showcase-sense-figure-1:

.. figure:: https://storage.googleapis.com/pigweed-media/sense/20240802/hero.png

   Figure 1. :ref:`showcase-sense-product-concept`

**Bringing a consumer electronics product from concept to mass production requires
more than just good firmware**. Shipping an electronics product also requires
support software for other product phases like prototyping, bringup,
manufacturing, integration testing & QA, hardware validation, and so on. Product
development also includes authoring large amounts of non-firmware code that runs
on developer, factory, and hardware engineer machines. Large scale products also
need significant investment in testing to facilitate large teams collaborating
and building towards a product vision. Sense illustrates a micro version of a
consumer electronics lifecycle, from the perspective of the firmware team,
leveraging many of the Pigweed components designed for large-scale projects.

The components you will experience through the
:ref:`Sense tutorial <showcase-sense-tutorial-intro>` have helped ship
millions of products to millions of people, by teams who were able to focus on
their core product value proposition instead of the low-level embedded plumbing
concerns Pigweed solves like portable RTOS primitives, test frameworks, RPC
comms from device to host, crash handling, interactive console, scripting
solutions, etc.

.. grid:: 2

   .. grid-item-card:: :octicon:`rocket` Tutorial
      :link: showcase-sense-tutorial-intro
      :link-type: ref
      :class-item: sales-pitch-cta-primary

      Walk through the Sense tutorial to see all of Sense's features
      in action, such as hermetic building, authoring in Visual Studio
      Code, viewings logs and sending commands over RPC, and more.

   .. grid-item-card:: :octicon:`code` Source code
      :link: https://cs.opensource.google/pigweed/showcase/sense
      :link-type: url
      :class-item: sales-pitch-cta-secondary

      Browse Sense's source code. If you like what you see, you are
      welcome to use Sense as the starting point for your own products.

.. _showcase-sense-overview:

----------------------
What is Pigweed Sense?
----------------------
Sense is three things:

#. :ref:`showcase-sense-product-concept` - A simplistic air quality monitor,
   described in more detail below. This is not a real product, but is instead a
   subset of functionality from an imagined full air quality sensor product.
   Despite not being real, it captures realistic flows such as integrating
   multiple inputs (sensors, buttons) and outputs (RGB LED), state machine
   management, and more. Sense will also soon include a web app, to
   graphically display air quality.

#. :ref:`showcase-sense-codebase` - A collection of C++ on-device firmware
   code and tests, including a port to the developer host, and factory tooling to
   illustrate how Pigweed’s tooling could help in manufacturing. This is a small
   subset of what a large scale project using Pigweed might look like.

#. :ref:`showcase-sense-tutorial` - An interactive tour through
   Pigweed components in the context of a small-scale, semi-realistic product
   that demonstrates how larger products might leverage Pigweed. This page
   you're reading now is the start of the tutorial.

.. note::

   Pigweed Sense, or Sense for short, is a fake product concept purely for
   illustration purposes, and has no connection to any real products, from Google
   or otherwise.

.. _showcase-sense-product-concept:

--------------------------
Sense, the product concept
--------------------------
The Sense product is a basic air quality monitor that watches the levels of
volatile organic compounds (VOCs) in the air, and alerts via the RGB LED if the
VOCs are too high and the user should take an action like opening a window.
The RGB LED also passively indicates air quality on a color spectrum.

The core interactions for the product are to observe the RGB LED showing green
(nominal, OK), observe the RGB LED blinking red (VOC alarm), adjust the VOC
threshold up/down via button press, and silencing/snoozing an active alarm with
a button press. Additionally, the RGB LED’s brightness is adjusted to match
ambient light to avoid being too distracting in low light conditions.

.. _Raspberry Pi Pico 1: https://www.raspberrypi.com/products/raspberry-pi-pico/
.. _Pimoroni Enviro+ Pack: https://shop.pimoroni.com/products/pico-enviro-pack
.. _Raspberry Pi Debug Probe: https://www.raspberrypi.com/products/debug-probe/
.. _Pimoroni Pico Omnibus (Dual Expander): https://shop.pimoroni.com/products/pico-omnibus?variant=32369533321299

While there is no real Pigweed Sense product, the imagined product can
be assembled from a `Raspberry Pi Pico 1`_ (or the new Pico 2) and a
`Pimoroni Enviro+ Pack`_. It is useful to also have a
`Raspberry Pi Debug Probe`_ and a `Pimoroni Pico Omnibus (Dual Expander)`_.
See :ref:`Figure 1 <showcase-sense-figure-1>`.

.. note::

   While the Enviro+ pack has a display, at time of writing (August 2024)
   Pigweed’s display subsystem isn’t ready yet, so there is no display use.

.. caution::

   **The Pico 1W and Pico 2W aren't supported.**

.. _showcase-sense-codebase:

-------------------
Sense, the codebase
-------------------
The Sense codebase covers different parts of a typical consumer electronics
lifecycle: initial development, prototyping & bringup, manufacturing, and
production. Since the software has multiple users across multiple teams (e.g.
software engineering, quality assurance, product management, returned
merchandise authorization, etc.) there is a fair amount of software beyond just
the production image.

On-device code
==============
* **Applications** (C++) - These are the various applications that exist to
  support different phases of the product, including bringup (``blinky``),
  development (``production``), and manufacturing (``factory``). Applications
  are built by instantiating and leveraging portable instances of
  :ref:`modules <docs-glossary-module>`. Applications are
  like ``.exe`` files for normal computers, but instead run on our Sense hardware.
  Only the ``production`` application would be used by end customers (if
  this was a real product).

* **Modules** (C++) - These are the portable, core abstractions that the
  applications are composed of, such as the state manager, sensor filtering
  code, and other hardware-agnostic functionality.

* **Drivers & targets** (C++) - This code covers hardware-specific
  resources like drivers that sample air quality or proximity.

* **Unit tests & scaffolding** (C++) - The unit tests ensure that the Sense firmware
  works as expected, even as further changes are made. The tests cover the core
  functionality of Sense, such as the state manager that coordinates the LED in
  response to sensor events.

Host code (for developers only)
===============================

* **Console** (Python) - Sense’s interactive console is built on top of
  :ref:`module-pw_console`, but has project-custom extensions. The console
  facilitates general development, debugging, and bringup.

* **Flashing** (Python) - Flashing device firmware is often fairly custom,
  necessitating project-specific scripts and associated build machinery.
  This is still the case in Sense, but Sense does demonstrate a powerful
  property of Bazel-based embedded projects: the ability to express flashing
  as just another part of the build graph. In Sense it's possible to clone
  the repo and jump immediately to flashing. When Bazel detects that the
  flashing target's firmware has not yet been built, Bazel builds it
  automatically and then proceeds to run the flashing script.

* **Manufacturing** (Python) - Manufacturing typically requires a series
  of custom scripts and programs for each step of the manufacturing flow.
  Sense has a 1-station "fake factory" flow you can run at your desk.

Web interface (for end customers)
=================================
.. _Web Serial API: https://developer.mozilla.org/en-US/docs/Web/API/Web_Serial_API

* **Sense web UI** (TypeScript) - The Sense web app lets you monitor
  and control a connected Sense device from a web browser. The
  web app and the Sense device communicate with each other via
  :ref:`module-pw_rpc` and the `Web Serial API`_; the Sense device
  does not run a web server.

.. _showcase-sense-tutorial:

-------------------
Sense, the tutorial
-------------------
The :ref:`tutorial <showcase-sense-tutorial-intro>` provides a
guided, hands-on walkthrough of everything we've discussed here, including:

* Hermetic building, flashing and testing with Bazel,
  :ref:`Pigweed's primary build system <seed-0111>`.

* Authoring in Visual Studio Code with full C++ code intelligence
  through :ref:`pw_ide <module-pw_ide>`.

* Hooking into :ref:`pw_system <module-pw_system>`, Pigweed's application
  framework.

* Viewing device logs through :ref:`pw_console <module-pw_console>`.

* Sending commands to the device over RPC with :ref:`pw_rpc <module-pw_rpc>`.

* Running host-side and on-device unit tests with
  :ref:`pw_unit_test <module-pw_unit_test>`.

* Portable software abstractions built on top of Pigweed's
  extensive collection of :ref:`modules <docs-glossary-module>`.

.. _showcase-sense-complexity:

------------------------------------------------------
A note on complexity, or, why is Sense so complicated?
------------------------------------------------------
There is no getting around that the Sense codebase is complicated and large
relative to the complexity of the imaginary Sense product. A straightforward
implementation could have less code, potentially much less. So why then use
something like Pigweed, which may appear kafkaesque in complexity to some
engineers, compared to the simpler alternative?

.. Use raw HTML for the next paragraph because it's not possible to bold,
.. underline, and format a word in reStructuredText.

.. raw:: html

   <p>
     The short answer is <b><i><u>scale</u></i></b>. Scale in number of
     engineers, scale in number of subsystems, scale in risk to instability,
     scale in security concerns, scale in number of units manufactured, scale
     in brand risk to a bad product, scale of repeatability, and so on.
   </p>

Let’s explore some of the ways the Sense codebase is larger than a naive,
straightforward implementation of a comparable product.

* **Testing** - A key tenet of Pigweed is that automated testing is useful, and
  rapidly pays for itself by enabling product delivery in less time and with
  better reliability. While time-to-first-feature may be higher without a
  testing-enabled codebase, the time-to-no-bugs is reduced. However,
  structuring code for testing adds complexity.

* **Security & robustness** - Making host-portable code enables running with modern
  code analysis tooling like ASAN, TSAN, MSAN, fuzzers, and more that are unlikely
  to run in the embedded context. Fun fact: We caught real bugs in Sense with this
  tooling during development!

* **Portability** - Code written with Pigweed abstractions can run on multiple
  target platforms, including custom hardware as well as Mac and Linux. This
  portability retains product optionality around hardware selection, and makes
  reusing the same code across products easier.

.. _hermetic: https://bazel.build/basics/hermeticity

* **Fast and consistent developer setup** - Our Bazel-based development environment
  enables engineers to become productive quickly. Furthermore, the fact that the
  environment is repeatable and `hermetic`_ reduces the chance that local developer
  machine settings will leak into developed code or processes. The product team's
  infrastructure, e.g. continuous integration (CI) & commit queue (CQ), can
  leverage the same machinery, reducing the delta between developer machines and
  infra.

With that said, we are still working on Sense, and hope to reduce the
complexity and add more comments to make the code more approachable.

----------
Next steps
----------
.. _Sense source code: https://cs.opensource.google/pigweed/showcase/sense

Start the :ref:`Sense tutorial <showcase-sense-tutorial-intro>`
or browse the `Sense source code`_.

.. toctree::
   :maxdepth: 1
   :hidden:

   Overview <self>
   intro
   setup
   explore
   build
   code_intelligence
   host_tests
   analysis
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
   Source code <https://pigweed.googlesource.com/pigweed/showcase/sense/>
