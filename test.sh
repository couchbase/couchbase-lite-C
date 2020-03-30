#!/bin/bash -e
#
# Runs Couchbase Lite for C unit tests on Unix-type systems.
#
# Prerequisites: Libraries & tests have been compiled using the `build.sh` script.
#
# Mac developers may prefer to use the Xcode project in the Xcode/ directory.
# Select the "CBL_Tests" scheme and choose Run.

SCRIPT_DIR=`dirname $0`
cd "$SCRIPT_DIR"

# Suppresses regular logs; comment this out if you want to see them
export LiteCoreLog=warning

# Some of the replicator tests connect to a Sync Gateway instance with a particular setup.
# The config files and Walrus database files can be found in the LiteCore repo, at
# vendor/couchbase-lite-core/Replicator/tests/data/
#
# From a shell in that directory, run `sync_gateway config.json` to start a non-TLS
# server on port 4984, and in another shell run `sync_gateway ssl_config.json` to start
# a TLS server on port 4994.
#
# To skip these tests, comment out the variables below:

export CBL_TEST_SERVER_URL=ws://localhost:4984
export CBL_TEST_SERVER_URL_TLS=wss://localhost:4994

# Now run the tests:

./build_cmake/test/CBL_C_Tests -r list "$@"
