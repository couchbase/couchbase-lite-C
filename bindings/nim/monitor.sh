#! /bin/bash -e

echo "$@" | entr -c -s "nim c -r $@"
