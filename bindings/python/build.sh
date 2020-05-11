#! /bin/bash -e

SCRIPT_DIR=`dirname $0`
cd $SCRIPT_DIR/CouchbaseLite
python3 ../BuildPyCBL.py $@
