.. _module-pw_software_update:

-------------------
pw_software_update
-------------------

Building blocks for secure, end-to-end software delivery, over anything.

.. attention:: 🚧 This documentation is **under construction**. 🚧

Overview
========

**Software update** is any channel that enables a product *vendor* to deliver
some kind of *improvement* to a product *consumer*, in good faith.

To that definition, a decent software update solution should holistically
address the following needs.

1. The product receives feature, performance, stability, UX improvements with
   minimum intervention from both the vendor and consumer. The product just
   automatically gets increasingly useful.

2. The product to receive timely security patches in response to newly
   discovered vulnerabilities and expiring trust material etc. The product just
   automatically heals itself.

3. The product vendor and consumer to nourish and maintain **mutual trust** over
   the supported lifetime of a product. The product vendor being able to
   remotely influence a product's behavior is a fantastic but dangerous power.
   Insider attacks can happen by mistake, willingly, or when compelled. They
   are unpreventable and naturally draws distrust from both consumer and
   regulatory agencies.

4. To **liberate** product development workflows rather than impede them.
   Security-heavy systems tend to compete with consumer-facing features
   for attention and resources from a product development team and thus
   perceived as "necessary evil". That is a misconception. With careful design,
   security features can actually liberate many workflows by taking care of
   the security and privacy concerns.

The following sections dive deeper into the frameworks governing the design
of the `pw_software_update` module to further clarify how it systematically
addresses the aforementioned requirements.


Threats
-------

..
   TODO(alizhang): Explain the threats we mitigate and those we don't.

Security
--------

..
   TODO(alizhang): Explain how trust is created, delivered, and nurtured
   throughout the lifetime of a product. Discuss how software update fit into
   the "Verified Boot" security model.

Experience
----------

..
   TODO(alizhang): Explain how to design software update to liberate
   development, release engineering, hardware ops, factory, metrics, and
   product launching workflows.

Getting started
===============

..
   TODO(alizhang): Tutorials, codelabs, representative examples.

Reference
=========

Bundle format
-------------

Manifesting
-----------

Key management
--------------

Signing
-------

Building
--------

Testing
-------

Device provisioning
-------------------

Device verification flow
------------------------