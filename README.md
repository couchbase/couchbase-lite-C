# couchbase-lite-C

This is a generic cross-platform version of Couchbase Lite.  
Or, it will be when it's done.  
**It's in the very early stages of development.**

## Goals

(Unofficial; we have no PRD or design docs yet)

* C API
  - C++ is a stretch goal but easily achieved
  - Can be bound to other languages like JavaScript or Python
* API similar to other Couchbase Lite platforms
* Same feature set as other Couchbase Lite platforms
* Support common Linux distros, esp. Ubuntu and Raspbian
  - Will probably support macOS too,just for ease of development
  - Windows?
* Run on Raspberry-Pi-level hardware
  - 32-bit and 64-bit
  - x86 and ARM
  - Hundreds of MB RAM, hundreds of MHz CPU, hundreds of MB storage

## Building It

(Currently only supports macOS, building with Xcode 10.)

1. Clone the repo
2. Check out submodules (recursively)
3. Open the Xcode project
4. Select scheme `CBL_Tests`
5. Run
