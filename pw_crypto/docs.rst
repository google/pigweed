.. _module-pw_crypto:

---------
pw_crypto
---------
A set of safe (read: easy to use, hard to misuse) crypto APIs.

.. attention::

  This module is under construction.

The following services are coming soon:

1. Digesting a message with SHA256.
2. Verifying a digital signature signed with ECDSA over the NIST P256 curve.

=====
Usage
=====

Here are some examples for a taste of what is coming up.

1. SHA256: Obtaining a oneshot digest.
--------------------------------------

.. code-block:: cpp

  #include "pw_crypto/sha256.h"

  std::byte digest[32];
  Status status = pw::crypto::sha256::Digest(message, digest);

2. SHA256: Digesting a long, potentially non-contiguous message.
----------------------------------------------------------------

.. code-block:: cpp

  #include "pw_crypto/sha256.h"

  std::byte digest[32];
  auto h = pw::crypto::sha256::Sha256();

  while (/* chunk ← Get next chunk of message */) {
    h.Update(chunk);
  }

  Status status = h.Final(digest);

3. ECDSA P256: Verifying a digital signature.
---------------------------------------------

.. code-block:: cpp

  #include "pw_crypto/sha256.h"

  std::byte digest[32];
  auto status = pw::crypto::sha256::Digest(message, digest);

  if (!status.ok()) {
    // handle errors.
  }

  bool valid = pw::crypto::ecdsa::VerifyP256Signature(public_key, digest, signature);

4. ECDSA: Verifying a digital signature signed with ECDSA over the NIST P256 curve, with a long and/or non-contiguous message.
------------------------------------------------------------------------------------------------------------------------------

.. code-block:: cpp

  #include "pw_crypto/sha256.h"

  std::byte digest[32];
  auto h = pw::crypto::sha256::Sha256();

  while (/* chunk ← Get the next chunk of message */) {
    h.Update(chunk);
  }

  auto status = h.Final(digest);
  bool valid = status.ok() && pw::crypto::ecdsa::VerifyP256Signature(public_key, digest, signature);