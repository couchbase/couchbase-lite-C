#! /bin/bash -e

ls */*.nim | entr -c -s "nim c -r $@"
