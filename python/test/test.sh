#! /bin/bash -e

SCRIPT_DIR=`dirname $0`
cd "$SCRIPT_DIR"

export PYTHONPATH=../../build/CBL_C/Build/Products/Debug/python
python3 test.py
