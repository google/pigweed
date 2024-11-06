.. _module-pw_channel-quickstart-guides:

===================
Quickstart & guides
===================
.. pigweed-module-subpage::
   :name: pw_channel

.. _module-pw_channel-guides-faqs:

---------------------------------
Frequently asked questions (FAQs)
---------------------------------

Can different tasks write into and read from the same channel?
==============================================================
No; it is not possible to read from the channel in one task while
writing to it from another task. A single task must own and operate
the channel. In the future, a wrapper will be offered which will
allow the channel to be split into a read half and a write half which
can be used from independent tasks.
