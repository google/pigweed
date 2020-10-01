.. _module-pw_blob_store:

-------------
pw_blob_store
-------------
``pw_blob_store`` is a storage container library for storing a single blob of
data. Blob_store is a flash-backed persistent storage system with integrated
data integrity checking that serves as a lightweight alternative to a file
system.

Write and read are only done using the BlobWriter and BlobReader classes.

Once a blob write is closed, reopening followed by a Discard(), Write(), or
Erase() will discard the previous blob.

Write blob:
  0) Create BlobWriter instance
  1) BlobWriter::Open().
  2) Add data using BlobWriter::Write().
  3) BlobWriter::Close().

Read blob:
  0) Create BlobReader instance
  1) BlobReader::Open().
  2) Read data using BlobReader::Read() or
     BlobReader::GetMemoryMappedBlob().
  3) BlobReader::Close().

.. note::
  The documentation for this module is currently incomplete.
