.. _module-pw_software_update:

-------------------
pw_software_update
-------------------

This module provides the building blocks for trusted software update systems.

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
   able to remotely modify a product's behavior is both fantastic and risky.
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

At its core, software update is a system that allows a consumer to download some
file provided by some vendor **by choice**. That choice might be "I want to be
left alone. I will not check for updates and I wish not to be solicited", or
"I want to sign my life away and never be bothered again. Please automatically
check and install all updates as soon as they are available", or anything in
between those two extremes.

How a consumer approaches such a choice depends on many factors.

1. The consumer's ability to authenticate the files, and by derivation, their
   vendor. Authentication enables the consumer to identify the update vendor (
   with cryptographic non-deniability) and assign some **baseline trust** to
   it based on individual judgment and the public credibility of the vendor.

   .. note:: From the vendor's point of view, authentication also protects the
     vendor's product from being tampered by attackers or consumers.
     Traditionally that has been the main purpose of security in software update
     systems.

2. The consumer's ability to independently audit the update files. e.g. by
   re-producing bitwise-identical binaries from available source code, hiring an
   accredited security researcher / investigator, looking up the files from a
   publicly available non-forgeable ledger etc.

3. The consumer's ability to assess the "bomb radius" of allowing an update,
   i.e. the worst possible fallout that may result from the vendor betraying the
   consumer's baseline trust. On platforms with well designed trust structure,
   it is easy for a generally informed consumer to reason that a misbehaving car
   infotainment app won't be able to take control of "system" and drive the car
   into a ditch, unless the application manages to escape its capabilities
   sandbox set up by the governing system software, either by exploiting a bug
   in the system or by colluding with the system vendor.

4. The consumer's situation. All else being equal, a developer evaluating a
   smart speaker with a test account, a US senator carrying a smartphone,
   and an airliner technician maintaining a fleet of passenger aircraft
   will approach software update choices with drastically different discretion.

It is in the best interests of both consumers and vendors to design and fit
software update systems in various platforms in a way that bolsters mutual
trust. So let's discuss this further in the "security model".

Security model
--------------

Authentication
^^^^^^^^^^^^^^

`pw_software_update` adopts `TUF <https://theupdateframework.io>`_ as the
authentication framework. This framework is well-formulated in the TUF
`specification <https://theupdateframework.github.io/specification/latest/>`_ (a
leisure read). To help with context continuity, the most relevant points are
captured below.

.. attention:: 🚧🏗 This documentation is under construction. The trucks were
  last seen here. 🏗🚧


Authorization
^^^^^^^^^^^^^

Transparency
^^^^^^^^^^^^

Deployment model
----------------

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