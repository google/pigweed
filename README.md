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
$ env_setup/cipd/cipd.py auth-login  # Once per machine.
$ . env_setup/bootstrap.sh
```

You can use `. env_setup/env_setup.sh` in place of `. env_setup/bootstrap.sh`.
Both should work every time, but `bootstrap.sh` tends to remove and reinstall
things at the expense of time whereas `env_setup.sh` tends to do basic checks
to see if time can be saved by skipping expensive operations.

If you're using Homebrew and you get an error saying
`module 'http.client' has no attribute 'HTTPSConnection'` then your
Homebrew Python was not set up to support SSL. Ensure it's installed with
`brew install openssl` and then run
`brew uninstall python && brew install python`. After that things should work.

The environment setup script will pull down the versions of tools necessary
to build Pigweed and add them to your environment. You can then build with
either GN or Bazel. You can also confirm you're getting the right versions
of tools&mdash;they should be installed under `env_setup/`.

```bash
$ which gn
~/pigweed/.cipd/pigweed.ensure/gn
$ gn gen out/host
$ ninja -C out/host
```

```bash
$ which cmake
~/pigweed/.cipd/pigweed.ensure/cmake
$ cmake -B out/cmake-host -S . -G Ninja
$ ninja -C out/cmake-host
```

```bash
$ which bazel
~/pigweed/.cipd/pigweed.ensure/bazel
$ bazel test //...
```

And do the following to test on the STM32F429 Discovery board. (The bazel build
does not yet support building for hardware.)

```bash
$ gn gen --args='pw_target_config = "//targets/stm32f429i-disc1/target_config.gni"' out/disco
$ ninja -C out/disco
$ pw test --root out/disco/ --runner stm32f429i_disc1_unit_test_runner -- --port /dev/ttyACM0
```

If any of this doesn't work please
[file a bug](https://bugs.chromium.org/p/pigweed/issues/entry).
