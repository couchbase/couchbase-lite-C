#!/usr/bin/env python3

import argparse
import glob
import os
import re
import sys

def get_symbols_from_exp(file):
    symbols = []

    with open(file) as f:
        isAppleExpFile = os.path.splitext(file)[1] == ".exp"

        for line in f.readlines():
            line = line.strip()
            
            # Ignore non-symbol lines:
            if not line or line.startswith("EXPORTS") or line.startswith(";"):
                continue
            if any(str in line for str in ["#", "*", "{", "}", "local:", "global:"]):
                continue
            
            # Collect symbols:
            if isAppleExpFile and line.startswith("_"):
                line = line[1:]
            if line.endswith(";"):
                line = line[:-1]
            
            symbols.append(line)

    return symbols

def get_symbols_from_header(file):
    symbols = []
    
    with open(file) as f:
        # Make multi-line functions single line
        content = re.sub(",\s*\n\s*", ", ", f.read())
        
        for line in content.split("\n"):
            if any(str in line for str in ["static inline"]):
                continue
            
            match = None
            if "FLEECE_PUBLIC" in line:                             # Fleece Constants
                match = re.search("\s+(k?FL\w+)\s*;", line)
            elif "CBL_PUBLIC" in line:                              # CBL Constants
                match = re.search("\s+(k?CBL\w+)\s*;", line)
            elif any(str in line for str in ["FLAPI", "CBLAPI"]):   # FLeece or CBL Functions
                match = re.search("\s+((_FL|FL|CBL)\w+)\s*\(", line)

            if match:
                symbols.append(match.group(1))

    return symbols

def check_exported_symbols(exp_paths, header_paths, exc_exp_symbols = None, 
                           exc_headers_symbols = None, exc_exp_paths = None, exc_header_paths = None):
    """Returns a tuple of missing and unknown exported symbol set"""

    # Excluded exported symbols (API defined in private headers for using in tests):
    if not exc_exp_symbols:
        exc_exp_symbols = []

    # Excluded symbols from headers:
    if not exc_headers_symbols:
        exc_headers_symbols = []

    # Excluded export files:
    exc_exp_files = []
    if exc_exp_paths:
        for path in exc_exp_paths:
            for file in glob.glob(path):
                exc_exp_files.append(file)

    # Excluded header files:
    exc_header_files = []
    if exc_header_paths:
        for path in exc_header_paths:
            for file in glob.glob(path):
                exc_header_files.append(file)
    
    # Get symbols from exported symbol files:
    exp_symbols = []
    for path in exp_paths:
        for file in glob.glob(path):
            if file not in exc_exp_files:
                exp_symbols.extend(get_symbols_from_exp(file))

    # Remove excluded exported symbols privately used in tests
    exp_symbols = [s for s in exp_symbols if s not in exc_exp_symbols]
    
    # Get symbols from header files:
    header_symbols = []
    for path in header_paths:
        for file in glob.glob(path):
            if file not in exc_header_files:
                header_symbols.extend(get_symbols_from_header(file))

    # Remove excluded symbols from headers
    header_symbols = [s for s in header_symbols if s not in exc_headers_symbols]
    
    # Find missing and unknown exported symbols:
    exp_symbols_set = set(exp_symbols)
    header_symbols_set = set(header_symbols)

    missing_symbols = header_symbols_set - exp_symbols_set
    unknown_symbols = exp_symbols_set - header_symbols_set

    return (missing_symbols, unknown_symbols)

if __name__ == "__main__":
    parser = argparse.ArgumentParser("Check missing exported symbols for CBL-C.")
    parser.add_argument("--exp", required=True, action='append', help="Exported symbol file paths.")
    parser.add_argument("--header", required=True, action='append', help="Header file paths.")
    parser.add_argument("--exclude_exp_symbols", required=False, help="List of excluded exported symbols (comma delimiter).")
    parser.add_argument("--exclude_header_symbols", required=False, help="List of excluded symbols from headers (comma delimiter).")
    parser.add_argument("--exclude_exp", required=False, action='append', help="Excluded exported symbol file paths.")
    parser.add_argument("--exclude_header", required=False, action='append', help="Excluded header file paths.")
    
    args = parser.parse_args()

    exclude_exp_symbols = []
    if args.exclude_exp_symbols:
        exclude_exp_symbols = [x.strip() for x in args.exclude_exp_symbols.split(",")]

    exclude_header_symbols = []
    if args.exclude_header_symbols:
        exclude_header_symbols = [x.strip() for x in args.exclude_header_symbols.split(",")]

    missings, unknowns = check_exported_symbols(
        args.exp, 
        args.header, 
        exclude_exp_symbols, 
        exclude_header_symbols, 
        args.exclude_exp, 
        args.exclude_header)

    if not missings and not unknowns:
        print(f"\nResult: OK")
    else:
        if missings:
            print(f"\nMISSING ({len(missings)}):")
            for s in sorted(missings):
                print(s)
        if unknowns:
            print(f"\nUNKNOWN ({len(unknowns)}):")
            for s in sorted(unknowns):
                print(s)
                
        sys.exit(1)
