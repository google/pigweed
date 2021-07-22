# BoringSSL Library

The folder provides build scripts for building the BoringSSL library. The
source code needs to be downloaded by the user. It is recommended to download
via "pw package install boringssl". This ensures that necessary build files
are generated. It als downloads the chromium verifier library, which will be
used as the default certificate verifier for boringssl in pw_tls_client.
For gn build, set `dir_pw_third_party_boringssl` to point to the
path of the source code. For applications using BoringSSL, add
`$dir_pw_third_party/boringssl` to the dependency list.
