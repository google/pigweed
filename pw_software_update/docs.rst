.. _module-pw_software_update:

-------------------
pw_software_update
-------------------

This module provides the building blocks for trusted software update systems.

.. attention:: 🚧 This documentation is **under construction**. 🚧

Goals
=====

**Software update** is any system that enables a product *vendor* to deliver
some kind of *improvement* to a product *consumer*, in good faith.

To that definition, a software update system should design toward the following
goals.

1. The product receives feature, performance, stability, UX improvements with
   minimum intervention from both the vendor and consumer. The product just
   automatically gets **increasingly more useful**.

2. The product receives timely security patches in response to newly discovered
   vulnerabilities and/or expiring trust material etc. The product is
   **self-healing**.

3. All software updates **require consumer approval**. The product vendor being
   able to remotely modify a product's behavior is both fantastic and dangerous.
   The system must mitigate insider attacks that may happen by mistake,
   willingly, or when compelled. The product vendor should strive to help the
   consumer make an informed decision over whether or not, when, and how to
   check / install what updates, to the extent feasible and as required by local
   regulations.

4. Software updates **liberate engineering workflows**. While security-heavy
   systems tend to compete with consumer-facing features for attention and
   resources and thus perceived as "necessary evil", it is often a
   misconception. With careful design, security features can preserve and
   enlarge flexibility in affected workflows. No law, no freedom.

System overview
===============

Getting started
===============

Developer guides
================

POUF(Protocol, Operations, Usage and Format)
--------------------------------------------

Update serving
--------------

Update consumption
------------------

Requesting a security review
----------------------------