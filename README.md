FUDO README
===========

*fudo* is a wrapper for translating `sudo` commands to `doas`
commands. Typically, *fudo* is invoked by a symbolic link named
`sudo`. *fudo* will then parse the arguments originally intended for
`sudo`, translate those arguments to a `doas` command, and then invoke
`doas` with the translated arguments.

Because the sudo feature set is larger than doas, most sudo commands
can't be converted into an exact doas command. For example, sudo has a
flag *--preserve-env* that has no doas equivalent. However, the
`doas.conf` options *keepenv* and *setenv* can be alternatively set,
producing the same effect.

The goal of *fudo* is to help make possible, replacing `sudo` with
`doas` on an operating system that is built to be integrated with
sudo. (For example, Arch Linux deploys scripts using sudo specific
features.)  This goal is accomplished by converting as many flags as
possible and ignoring flags that can't. Flags that can't be translated
are left to be configured in `doas.conf`. In order to make replacing
`sudo` possible, the system administrator must decide how `doas.conf`
should be configured.


Building & Installation
=======================

While in fudo's source directory, run:

    $ make

Afterwards, you may install it by doing as root:

    # install fudo /usr/bin/fudo
    # ln -s /usr/bin/fudo /usr/bin/sudo


Using fudo
==========

After installing as stated above, any `sudo` command will be converted
to a `doas`. By default, `fudo` shows the command translation to
stderr. For example,

    $ sudo whoami
    fudo <<< sudo whoami
    fudo >>> /usr/bin/doas whoami
    root

You can disable this by setting the environment variable FUDO_HIDE.

    $ export FUDO_HIDE=
    $ sudo whoami
    root

The environment variables `SUDO_USER`, `SUDO_UID`, and `SUDO_GID` are
always passed to `doas`. If you're running scripts or commands that
use these environment variables, `doas.conf` must be configured to use
them. For example,

    permit setenv { SUDO_USER SUDO_UID SUDO_GID } user as root
