# Pigweed Embedded Oriented Software Libraries

Pigweed is a collection of embedded-focused libraries, which we call "modules".
These modules are designed for small-footprint MMU-less microcontrollers like
the ST Micro STM32L452 or the Nordic NRF82832. The modules are designed to
facilitate easy integration into existing codebases.

Pigweed is in the early stages of development.

# Getting Started

```bash
$ git clone sso://pigweed.googlesource.com/pigweed/pigweed ~/pigweed
$ cd ~/pigweed
$ . env_setup/setup.sh
```

The environment setup script will pull down the versions of tools necessary
to build Pigweed and add them to your environment. You can then build with
either GN or Bazel. You can also confirm you're getting the right versions
of tools&mdash;they should be installed under `env_setup/`.

```bash
$ which gn
~/pigweed/env_setup/cipd/tools/gn
$ gn gen out/host
$ ninja -C out/host
```

```bash
$ which bazel
~/pigweed/env_setup/cipd/tools/bazel
$ bazel test //...
```

And do the following to test on hardware. (The bazel build does not yet
support building for hardware.)

```bash
$ gn gen --args='pw_target_config = "//targets/stm32f429i-disc1/target_config.gni"' out/disco
$ ninja -C out/disco
```

If any of this doesn't work please
[file a bug](https://bugs.chromium.org/p/pigweed/issues/entry).
