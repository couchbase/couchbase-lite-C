#! /bin/bash -e

find . -name '*.nim'  |  entr -c -s "nimble test"
