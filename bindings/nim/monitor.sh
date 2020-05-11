#! /bin/bash -e
#
# This uses the `entr` tool to re-run `nimble test` whenever any source file is saved.
# Very handy to leave running in a terminal window while you work.
# You can get `entr` from <http://entrproject.org> or your friendly local package manager.

find . -name '*.nim'  |  entr -c -s "nimble test"
