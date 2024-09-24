#!/bin/bash -e

# From https://sourceware.org/gdb/current/onlinedocs/gdb.html/MiniDebugInfo.html

WORKING_DIR="${1}"
pushd $WORKING_DIR

resolved_file=$(basename $(readlink -f libcblite.so))

# Extract the dynamic symbols from the main binary, there is no need
# to also have these in the normal symbol table.
llvm-nm -D $resolved_file --format=posix --defined-only \
  | awk '{ print $1 }' | sort > dynsyms

# Extract all the text (i.e. function) symbols from the debuginfo.
# (Note that we actually also accept "D" symbols, for the benefit
# of platforms like PowerPC64 that use function descriptors.)
llvm-nm $resolved_file --format=posix --defined-only \
  | awk '{ if ($2 == "T" || $2 == "t" || $2 == "D") print $1 }' \
  | sort > funcsyms

# Keep all the function symbols not already in the dynamic symbol
# table.
comm -13 dynsyms funcsyms > keep_symbols
rm dynsyms funcsyms

# Separate full debug info into debug binary.
llvm-objcopy --only-keep-debug $resolved_file libcblite.so.sym

# Copy the full debuginfo, keeping only a minimal set of symbols and
# removing some unnecessary sections.
llvm-objcopy -S --remove-section .gdb_index --remove-section .comment \
  --keep-symbols=keep_symbols libcblite.so.sym libcblite.so.minisym

# Drop the full debug info from the original binary.
llvm-strip --strip-debug -R .comment $resolved_file

# Inject the compressed data into the .gnu_debugdata section of the
# original binary.
xz libcblite.so.minisym
llvm-objcopy --add-section .gnu_debugdata=libcblite.so.minisym.xz $resolved_file
llvm-objcopy --add-gnu-debuglink=libcblite.so.sym $resolved_file
rm libcblite.so.minisym.xz keep_symbols
