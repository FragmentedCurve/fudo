# FUDO README

*fudo* is a wrapper for translating `sudo` commands to `doas` commands. Typically, *fudo* is invoked via a symbolic link named `sudo`. It parses the arguments originally intended for `sudo`, translates those arguments to a `doas` command, and then invokes `doas` with the translated arguments.

## Features

Since the `sudo` feature set is larger than `doas`, not all `sudo` commands can be converted into exact `doas` commands. For instance, `sudo` has a `--preserve-env` flag with no direct `doas` equivalent. However, the `doas.conf` options `keepenv` and `setenv` can be set to produce a similar effect.

The goal of *fudo* is to facilitate the replacement of `sudo` with `doas` on operating systems built to integrate with `sudo` (e.g., Arch Linux, which deploys scripts using `sudo`-specific features). This is achieved by converting as many flags as possible and ignoring the rest. Flags that can't be translated are left to be configured in `doas.conf`. The system administrator must decide how `doas.conf` should be configured to accommodate the necessary environment.

## Building & Installation

To build *fudo*, navigate to its source directory and run:

```sh
make
```

To install *fudo* as the root user, execute:

```sh
install fudo /usr/bin/fudo
ln -s /usr/bin/fudo /usr/bin/sudo
```

## Using fudo

Once installed, any `sudo` command will be converted to a `doas` command. By default, `fudo` displays the command translation to `stderr`. For example:

```sh
$ sudo whoami
fudo <<< sudo whoami
fudo >>> /usr/bin/doas whoami
root
```

You can disable this by setting the environment variable `FUDO_HIDE`:

```sh
$ export FUDO_HIDE=1
$ sudo whoami
root
```

The environment variables `SUDO_USER`, `SUDO_UID`, and `SUDO_GID` are always passed to `doas`. If you're running scripts or commands that rely on these variables, ensure `doas.conf` is configured to use them. For example:

```sh
permit setenv { SUDO_USER SUDO_UID SUDO_GID } user as root
```

## Configuration

To maximize compatibility, adjust your `doas.conf` to handle specific environment variables and behaviors previously managed by `sudo`. Here’s an example configuration:

```sh
permit keepenv user as root
permit setenv { SUDO_USER SUDO_UID SUDO_GID } user as root
```

With the above configurations, `doas` can replicate some of the environmental management provided by `sudo`.

## Conclusion

*fudo* aims to bridge the gap between `sudo` and `doas` by providing a flexible and configurable solution for systems transitioning to `doas`. With proper configuration, *fudo* can help maintain system operations that were originally dependent on `sudo`.

For any issues or contributions, please refer to the project's repository.

---

By following these instructions and configurations, you should be able to seamlessly integrate *fudo* into your workflow, leveraging `doas` as a secure alternative to `sudo`.
