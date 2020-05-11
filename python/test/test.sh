#! /bin/bash -e

SCRIPT_DIR=`dirname $0`
cd "$SCRIPT_DIR"

export PYTHONPATH=..
python3 test.py
