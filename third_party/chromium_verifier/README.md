# Chrome Certificate Verifier Library

The folder provides targets for building the certificate verifier used by
chromium. The sources live in the chromium source repo. It is recommended
to download the repo via `pw package install chromium_verifier`, which
performs a sparse checkout instead of checking out the who repo. For gn build,
set `dir_pw_third_party_chromium_verifier` to point to the repo path. The
library requires `third_party/boringssl` and need to be setup first. See
`third_party/boringssl/README.md` for instruction. The library will primarily
be used by pw_tls_client when using boringssl backend.

The verifier we build for embedded target excludes the chromium metric feature.
Specifically, for the current port, we use a noop implementation for function
`UmaHistogramCounts10000()`. The function is originally used to generate
histograms that record iteration count. For the verifier, the iteration count
is only used in unittest. Compiling the feature requires to bring in a
significant amount of additional sources and also many system dependencies
including threading, file system, memory mapping management (sys/mman.h) etc.
It's too complicated to accomodate for embedded target.

However we do build a full version including the metric feature on Linux host
platform for running native unittest, as a criterion for rolling.
