.. _module-pw_hdlc-router:

======
Router
======
.. pigweed-module-subpage::
   :name: pw_hdlc

``pw_hdlc::Router`` is an experimental asynchronous HDLC router using
``pw_channel``.

It sends and receives HDLC packets using an external byte-oriented channel
and routes the decoded packets to local datagram-oriented channels.

---
API
---
.. doxygenclass:: pw::hdlc::Router

-----------------
More pw_hdlc docs
-----------------
.. include:: docs.rst
   :start-after: .. pw_hdlc-nav-start
   :end-before: .. pw_hdlc-nav-end
