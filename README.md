# Pigweed

Pigweed is a collection of embedded-focused libraries, called “modules”.
These modules are designed for small-footprint MMU-less microcontrollers like
the ST Micro STM32L452 or the Nordic NRF82832. The modules are designed to
facilitate easy integration into existing codebases.

Pigweed is in the early stages of development, and should be considered
experimental. We’re continuing to evolve the platform and add new modules. We
value developer feedback along the way.

Pigweed is an open source project with a [code of conduct](CODE_OF_CONDUCT.md)
that we expect everyone who interacts with the project to respect.

# Getting Started

```bash
$ git clone sso://pigweed.googlesource.com/pigweed/pigweed ~/pigweed
$ cd ~/pigweed
# Only need to run auth-login once per machine.
$ pw_env_setup/py/pw_env_setup/cipd_setup/wrapper.py auth-login
$ . pw_env_setup/bootstrap.sh
```

You can use `. pw_env_setup/setup.sh` in place of `. pw_env_setup/bootstrap.sh`.
Both should work every time, but `bootstrap.sh` tends to remove and reinstall
things at the expense of time whereas `setup.sh` assumes things have
already been installed and only sets environment variables.

If you're using Homebrew and you get an error saying
`module 'http.client' has no attribute 'HTTPSConnection'` then your
Homebrew Python was not set up to support SSL. Ensure it's installed with
`brew install openssl` and then run
`brew uninstall python && brew install python`. After that things should work.

The environment setup script will pull down the versions of tools necessary
to build Pigweed and add them to your environment. You can then build with
GN, CMake, or Bazel. You can also confirm you're getting the right versions
of tools—they should be installed under `.cipd/`.

**Build for the host with GN**
```bash
$ which gn
~/pigweed/.cipd/pigweed.ensure/gn
$ gn gen out/host
$ ninja -C out/host
```

**Build for the host with CMake**
```bash
$ which cmake
~/pigweed/.cipd/pigweed.ensure/bin/cmake
$ cmake -B out/cmake-host -S . -G Ninja
$ ninja -C out/cmake-host
```

**Build for the host with Bazel**
```bash
$ which bazel
~/pigweed/.cipd/pigweed.ensure/bazel
$ bazel test //...
```

**Build for the STM32F429 Discovery board**
```bash
$ gn gen --args='pw_target_config = "//targets/stm32f429i-disc1/target_config.gni"' out/disco
$ ninja -C out/disco
$ pw test --root out/disco/ --runner stm32f429i_disc1_unit_test_runner
```

The CMake and Bazel builds do not yet support building for hardware.

To flash firmware to an STM32 Discovery development board (and run `pw test`)
from macOS, you need to install OpenOCD. Install Homebrew using the latest
instructions at https://brew.sh/, then install OpenOCD with
`brew install openocd`.

If any of this doesn't work please
[file a bug](https://bugs.chromium.org/p/pigweed/issues/entry).
