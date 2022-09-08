.. _module-pw_software_update:

-------------------
pw_software_update
-------------------

This module provides the following building blocks of a high assurance software
update system:

1. A `TUF <https://theupdateframework.io>`_-based security framework.
2. A `protocol buffer <https://developers.google.com/protocol-buffers>`_ based
   software update "bundle" format.
3. An update bundle decoder and verification stack.
4. An extensible software update RPC service.

High assurance software update
==============================

On a high-level, a high-assurance software update system should make users feel
safe to use and be technologically worthy of user's trust over time. In
particular it should demonstrate the following security and privacy properties.

1. The update packages are generic, sufficiently qualified, and officially
   signed with strong insider attack guardrails.
2. The update packages are delivered over secure channels.
3. Update checking, changelist, and installation are done with strong user
   authorization and awareness.
4. Incoming update packages are strongly authenticated by the client.
5. Software update requires and builds on top of verified boot.

Life of a software update
=========================

The following describes a typical software update sequence of events. The focus
is not to prescribe implementation details but to raise awareness in subtle
security and privacy considerations.

Stage 0: Product makers create and publish updates
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
A (system) software update is essentially a new version of the on-device
software stack. Product makers create, qualify and publish new software updates
to deliver new experiences or bug fixes to users.

While not visible to end users, the product maker team is expected to follow
widely agreed security and release engineering best practices before signing and
publishing a new software update. A new software update should be generic for
all devices, rather than targeting specific devices.

Stage 1: Users check for updates
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
For most consumer products, software updates are "opt-in", which means users
either manually check for new updates or opt-in for the device itself to check
(and/or install) updates automatically on the user's behalf. The opt-in may be
blanket or conditioned on the nature of the updates.

If users have authorized automatic updates, update checking also happens on a
regular schedule and at every reboot.

.. important::
   As a critical security recovery mechanism, checking and installing software
   updates ideally should happen early in boot, where the software stack has
   been freshly verified by verified boot and minimum mutable input is taken
   into account in update checking and installation.

   In other words, the longer the system has been running (up), the greater
   the chance that system has been taken control by an attacker. So it is
   a good idea to remind users to reboot when the system has been running for
   "too long".

Stage 2: Users install updates
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Once a new update has been determined to be available for the device, users will
be prompted to authorize downloading and installing the update. Users can also
opt-in to automatic downloading and installing.

.. important::
   If feasible, rechecking, downloading and installing an update should be
   carried out early in a reboot -- to recover from potential temporary attacker
   control.

To improve reliability and reduce disruption, modern system updates typically
employ an A/B update mechanism, where the incoming update is installed into
a backup boot slot, and only enacted and locked down (anti-rollback) after
the new slot has passed boot verification and fully booted into a good state.

.. important::
   While a system update is usually carried out by a user space update client,
   an incoming update may contain more than just the user space. It could
   contain firmware for the bootloader, trusted execution environment, DSP,
   sensor cores etc. which could be important components of a device's TCB (
   trusted compute base, where critical device security policies are enforced).
   When updating these components across different domains, it is best to let
   each component carry out the actual updating, some of which may require
   stronger user authorization (e.g. a test of physical presence, explicit
   authorization with an admin passcode etc.)

Lastly, updates should be checked again in case there are newer updates
available.

Getting started with bundles (coming soon)
==========================================