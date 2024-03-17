# Introduction
 
NetHackDS is what it sounds like:  a port of the famous game NetHack for the
Nintendo DS.

The goal of this project is to create a port of NetHack for the DS utilizing the unique features of the DS to create an easy-to-use NetHacking experience that's as good or better than playing on a PC.

# Installing

Each release of NetHackDS contains three major artifacts:

1. The 'NetHackDS.nds' ROM itself.
2. The 'NetHack' directory, which contains a number of necessary data and configuration files.
3. The game manual in PDF form.

To install, simply extract the ZIP file into the root of your flash cart *preserving the directory structure*.

If you want to install manually, do the following:

1. Copy 'NetHackDS.nds' to your flash cart in your preferred location (you can feel free to rename the ROM if you like).
2. Copy the contents of the NetHack folder to a corresponding folder on your flash cart.  For version 3.6.7-1 or later, any of the following folder names may be used[^1]:
    1. /data/NetHack-X.Y.Z
    2. /data/NetHack
    3. /NetHack-X.Y.Z
    4. /NetHack

Finally, depending on the model of flash cart, you may need to apply an appropriate [DLDI](https://www.chishm.com/DLDI/index.html) patch to the ROM.

# Configuring

NetHackDS is configured via the `defaults.nh` file located in the NetHack data directory.  For more information on specific configuration settings, see the generic NetHack documentation as well as the NetHackDS manual, which covers various port-specific configuration options.

# Building

## Setting up the environment

In order to build NetHackDS, a number of pre-requisites must be in place:

1. [devkitpro](https://devkitpro.org) and the associated nds-dev dependencies.
2. libpcre
 
   You can find my DS port [on GitHub
   here](https://github.com/fancypantalons/pcre-ds).  Build and install
   according to the included instructions.
3. Masscat's gdb stub

   I'm now hosting this code as a [separate github project](https://github.com/fancypantalons/NDS-Debug-Stub), and have updated it to build with the latest devkitarm.
4. A working build environment for the host machine capable of producing and
   running 32-bit executable binaries.[^2]

NHDS is set up assuming a Unix host build environment.  Getting the build
working on an alternative host environment is left as an exercise to the
reader.

## Compiling

With the pre-requisites in place, a build can be triggered by running:

```sh
sys/nds/setup.sh
make -f sys/nds/Makefile
```

The game and data files are then available in the `binary` directory.

## Running via Desmume

[Desmume](https://desmume.org/) is able to run homebrew like NetHackDS with the right setup and flags.

First, you must manually apply a DLDI patch for the ROM.  The [R4DS](https://www.chishm.com/DLDI/downloads/r4tf_v2.dldi) DLDI patch is tested and known to work with Desmume.

Once patched, you can run the game after a build as follows (assuming a fairly recent version of Desmume):

```sh
desmume-cli --slot1 R4 --slot1-fat-dir binary/ binary/NetHackDS.nds
```

Keep in mind, at least in my limited testing, it doesn't appear Desmume
supports writing back to the supplied directory, so saving and bonesfile
dropping will not work.

[^1]:
    For version 3.6.7 and earlier, only the '/NetHack' folder is supported.
    
[^2]:
    NetHack ships with a build tool that assembles the data library that
    accompanies the game.  Obviously we can't run that executable on the NDS, so we
    run it on the host OS.  Unfortunately, the library built with a 64-bit version
    of the tool is not readable on a 32-bit version of the game, and vice versa.
    And because the NDS is a 32-bit environment, we must therefore create a 32-bit
    version of the tool and run it.

    The above notably means we can't build on the Windows Subsystem for Linux, as
    that environment does not support 32-bit executables.

