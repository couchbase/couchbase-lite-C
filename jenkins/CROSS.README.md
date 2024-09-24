# README regarding cross compilation

## Toolchain Rationale

Our cross compilation process in 4.0 and beyond uses clang and LLVM because it is, by design, a cross compiler.  Debian 11 ships with clang 11 which is new enough to be able to build Couchbase Lite C even with its C++20 functionality.  Accordingly the only thing left from the old GNU toolchain is the system libstdc++ and libgcc_s.  

## Cross Compilation Primer

Cross compilation is defined as building a program for a system differing from the one performing the build.  This can be in obvious ways, such as building an ARM program on an x86_64 machine, or in less obvious ways, like building an x86_64 program designed to run on an older variant of Linux than the one doing the build.  Cross compilation relies mainly on two things:

- Cross Compiler
- Sysroot

The former is merely a compiler that runs on the build system architecture, while being capable of emitting machine code for the target system architecture.  Historically with GNU, every cross compiler needed a separate build (i.e. x86_64-linux-gnu-gcc, arm-linux-gnueabihf-gcc, etc).  This is no longer the case with clang, and the same compiler can be used to target many different architectures via the `--target` flag.

The latter is essentially a directory that is set up identically to the target environment (i.e. /usr, /usr/lib, /lib, /lib64, /usr/include, etc) in terms of system libraries and headers.  In fact, the best way to create a sysroot is to copy these directories right out of some sort of official distribution.  This directory can be passed to the compiler via `--sysroot` and after that the compiler will behave as if your directory were `/` on the filesystem in terms of implicit library and header searching.

## Cross Compiler

As mentioned, the first piece is essentially handed to us on a silver platter.  clang is widely distributed in many distros now, and is built by default with all the targets we need and many more.  So all we need for this step is to install clang, lld, and llvm from the default package manager of the distro.

## Sysroot

This step is a little bit more involved, but it only need to be done once for each environment you want to target.  Most of the steps are documented in this [wiki page](https://github.com/couchbase/couchbase-lite-C/wiki/Raspberry-Pi-Setup#part-2-sysroot) but there is one additional thing specific to clang that needs to be done.  The file `ld-linux-<arch>.so.1` (sometimes 2 or 3) is found in the `lib/<triple>` folder, but clang will look for it directly in the `lib` folder so there needs to be a symlink (note that on x86_64 it is the `lib64` folder instead):

```bash
# example for arm64
cd sysroot/lib
ln -s aarch64-linux-gnu/ld-linux-aarch64.so.1 ld-linux-aarch64.so.1
```