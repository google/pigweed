.. _module-pw_crypto:

pw_crypto
=========
A set of safe (read: easy to use, hard to misuse) crypto APIs.

.. attention::

  This module is under construction.

The following crypto services are provided by this module.

1. Digesting a message with `SHA256`_.
2. Verifying a digital signature signed with `ECDSA`_ over the NIST P256 curve.
3. Many more to come ...

SHA256
------

1. Obtaining a oneshot digest.

.. code-block:: cpp

  #include "pw_crypto/sha256.h"

  std::byte digest[32];
  Status status = pw::crypto::sha256::Digest(message, digest);

2. Digesting a long, potentially non-contiguous message.

.. code-block:: cpp

  #include "pw_crypto/sha256.h"

  std::byte digest[32];
  auto h = pw::crypto::sha256::Sha256();

  while (/* chunk ← Get next chunk of message */) {
    h.Update(chunk);
  }

  Status status = h.Final(digest);

ECDSA
-----

1. Verifying a digital signature signed with ECDSA over the NIST P256 curve.

.. code-block:: cpp

  #include "pw_crypto/sha256.h"

  std::byte digest[32];
  auto status = pw::crypto::sha256::Digest(message, digest);

  if (!status.ok()) {
    // handle errors.
  }

  bool valid = pw::crypto::ecdsa::VerifyP256Signature(public_key, digest, signature).ok();

2. Verifying a digital signature signed with ECDSA over the NIST P256 curve, with a long and/or non-contiguous message.

.. code-block:: cpp

  #include "pw_crypto/sha256.h"

  std::byte digest[32];
  auto h = pw::crypto::sha256::Sha256();

  while (/* chunk ← Get the next chunk of message */) {
    h.Update(chunk);
  }

  auto status = h.Final(digest);
  bool valid = status.ok() && pw::crypto::ecdsa::VerifyP256Signature(public_key, digest, signature).ok();

Configuration
-------------

The crypto services offered by pw_crypto can be backed by different backend crypto libraries. For now only Mbed TLS is supported, others are under construction.

Mbed TLS

.. code-block:: sh

  # Install and configure MbedTLS
  pw package install mbedtls
  gn gen out --args='dir_pw_third_party_mbedtls="//.environment/packages/mbedtls" pw_crypto_SHA256_BACKEND="//pw_crypto:sha256_mbedtls" pw_crypto_ECDSA_BACKEND="//pw_crypto:ecdsa_mbedtls"'

  ninja -C out


Size Reports
------------

Below are size reports for each crypto service. These may vary from across different configurations.

.. include:: size_report
