.. _module-pw_file:

=======
pw_file
=======

.. attention::

  ``pw_file`` is under construction, and may see significant breaking API
  changes.

The ``pw_file`` module defines a service for file system-like interactions
between a client and server. FileSystem services may be backed by true file
systems, or by virtual file systems that provide a file-system like interface
with no true underlying file system.

pw_file does not define a protocol for file transfers.
`pw_transfer <module-pw_transfer>`_ provides a generalized mechanism for
performing file transfers, and is recommended to be used in tandem with pw_file.

-----------
RPC service
-----------
The FileSystem RPC service is oriented to allow direct interaction, and has no
sequenced protocol. Unlike FTP, all interactions are stateless. This service
also does not yet define any authentication mechanism, meaning that all clients
that can access a FileSystem service are granted equal permissions.

.. literalinclude:: file.proto
  :language: protobuf
  :lines: 14-
