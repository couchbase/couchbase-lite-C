#! /bin/bash

nim --path:src --path:binding --outdir:bin -d:nimDebugDlOpen -r c test/smokeTest.nim

# Other flags:
# --debugger:native
# -d:release
