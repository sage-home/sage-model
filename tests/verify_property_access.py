#!/usr/bin/env python3
# verify_property_access.py
import re
import sys
import os
import glob
import argparse
from collections import defaultdict

# Fields that should be accessed via property macros
GALAXY_FIELDS = [
    'HotGas', 'ColdGas', 'StellarMass', 'BulgeMass', 'MetalsHotGas', 
    'MetalsColdGas', 'Mvir', 'Rvir', 'Vvir', 'EjectedMass', 'BlackHoleMass',
    'MetalsEjectedMass', 'Cooling', 'Heating', 'r_heat', 'ICS', 'MetalsICS',
    'Type', 'Pos', 'Vel', 'MetalsStellarMass', 'MetalsBulgeMass', 'SfrDisk',
    'SfrBulge', 'SfrDiskColdGas', 'SfrDiskColdGasMetals', 'SfrBulgeColdGas',
    'SfrBulgeColdGasMetals', 'infallMvir', 'infallVvir', 'infallVmax', 'dT',
    'DiskScaleRadius', 'MergTime', 'QuasarModeBHaccretionMass',
    'TimeOfLastMajorMerger', 'TimeOfLastMinorMerger', 'OutflowRate',
    'TotalSatelliteBaryons'
]

# Array fields that need special handling for element access
ARRAY_FIELDS = [
    'Pos', 'Vel', 'SfrDisk', 'SfrBulge', 'SfrDiskColdGas', 
    'SfrDiskColdGasMetals', 'SfrBulgeColdGas', 'SfrBulgeColdGasMetals'
]

# Dynamic array fields that need special handling for size and element access
DYNAMIC_ARRAY_FIELDS = [
    'StarFormationHistory', 'TestNonZeroArray'
]

# Regex patterns to detect different types of access
DIRECT_FIELD_PATTERN = r'galaxies\[\w+\]\.([A-Za-z]+)|galaxy->([A-Za-z]+)'
ARRAY_ACCESS_PATTERN = r'galaxies\[\w+\]\.([A-Za-z]+)\[\w+\]|galaxy->([A-Za-z]+)\[\w+\]'
PROPER_MACRO_PATTERN = r'GALAXY_PROP_([A-Za-z]+)\(&galaxies\[\w+\]\)|GALAXY_PROP_([A-Za-z]+)\(galaxy\)'
PROPER_ARRAY_MACRO_PATTERN = r'GALAXY_PROP_([A-Za-z]+)_ELEM\(&galaxies\[\w+\], \w+\)|GALAXY_PROP_([A-Za-z]+)_ELEM\(galaxy, \w+\)'
SYNC_PATTERN = r'sync_properties_to_direct|sync_direct_to_properties'

def check_file(filename, verbose=False):
    """
    Check a file for direct field access vs property macro usage
    Returns the number of problems found
    """
    print(f"Checking {filename}...")
    try:
        with open(filename, 'r') as f:
            content = f.read()
    except Exception as e:
        print(f"Error opening {filename}: {e}")
        return 0
    
    # Find all direct field accesses
    direct_matches = re.findall(DIRECT_FIELD_PATTERN, content)
    array_matches = re.findall(ARRAY_ACCESS_PATTERN, content)
    
    # Find property macro usages
    macro_matches = re.findall(PROPER_MACRO_PATTERN, content)
    array_macro_matches = re.findall(PROPER_ARRAY_MACRO_PATTERN, content)
    
    # Find sync calls
    sync_calls = re.findall(SYNC_PATTERN, content)
    
    # Process results
    direct_accesses = []
    for match in direct_matches:
        field = match[0] if match[0] else match[1]
        if field in GALAXY_FIELDS:
            line_num = find_line_number(content, f".{field}")
            direct_accesses.append((field, line_num))
    
    array_accesses = []
    for match in array_matches:
        field = match[0] if match[0] else match[1]
        if field in ARRAY_FIELDS:
            line_num = find_line_number(content, f".{field}[")
            array_accesses.append((field, line_num))
    
    # Count macro usages
    macro_usages = defaultdict(int)
    for match in macro_matches:
        field = match[0] if match[0] else match[1]
        macro_usages[field] += 1
    
    array_macro_usages = defaultdict(int)
    for match in array_macro_matches:
        field = match[0] if match[0] else match[1]
        array_macro_usages[field] += 1
    
    # Report findings
    problems = 0
    
    if direct_accesses:
        problems += len(direct_accesses)
        print("  Direct galaxy field accesses found:")
        for field, line in direct_accesses:
            print(f"  - Line {line}: {field}")
    
    if array_accesses:
        problems += len(array_accesses)
        print("  Direct galaxy array accesses found:")
        for field, line in array_accesses:
            print(f"  - Line {line}: {field}[]")
    
    if verbose:
        print("\n  Property macro usage statistics:")
        for field, count in sorted(macro_usages.items(), key=lambda x: x[1], reverse=True):
            print(f"  - GALAXY_PROP_{field}(): {count} uses")
        
        print("\n  Array property macro usage statistics:")
        for field, count in sorted(array_macro_usages.items(), key=lambda x: x[1], reverse=True):
            print(f"  - GALAXY_PROP_{field}_ELEM(): {count} uses")
        
        print(f"\n  Sync function calls: {len(sync_calls)}")
    
    if problems == 0:
        print("  No direct galaxy field accesses found. ✓")
    else:
        print(f"  Found {problems} direct field access issues that need to be fixed.")
    
    # Basic validation to ensure GALAXY_PROP_* macros are being used consistently
    if len(macro_usages) + len(array_macro_usages) == 0 and len(GALAXY_FIELDS) > 0:
        print("  WARNING: No GALAXY_PROP_* macros found in this file!")
    
    return problems

def analyze_modules_directory(directory, verbose=False):
    """
    Analyze all .c files in a directory for property access
    """
    c_files = glob.glob(os.path.join(directory, "*.c"))
    h_files = glob.glob(os.path.join(directory, "*.h"))
    
    total_problems = 0
    
    print(f"\nAnalyzing {len(c_files)} .c files and {len(h_files)} .h files in {directory}")
    print("=" * 80)
    
    for filename in sorted(c_files + h_files):
        problems = check_file(filename, verbose)
        total_problems += problems
        print("-" * 80)
    
    return total_problems

def find_line_number(content, pattern):
    """
    Find the line number for a pattern in the content
    """
    lines = content.split('\n')
    for i, line in enumerate(lines, 1):
        if pattern in line:
            return i
    return 0

def main():
    parser = argparse.ArgumentParser(description='Verify property access patterns in SAGE codebase')
    parser.add_argument('path', help='File or directory to analyze')
    parser.add_argument('-v', '--verbose', action='store_true', help='Show detailed statistics')
    parser.add_argument('-r', '--recursive', action='store_true', help='Recursively search directories')
    
    args = parser.parse_args()
    
    if os.path.isdir(args.path):
        if args.recursive:
            # Walk through the directory tree
            total_problems = 0
            for root, dirs, files in os.walk(args.path):
                c_files = [os.path.join(root, f) for f in files if f.endswith('.c')]
                h_files = [os.path.join(root, f) for f in files if f.endswith('.h')]
                
                if c_files or h_files:
                    print(f"\nChecking directory: {root}")
                    print("=" * 80)
                    
                    for filename in sorted(c_files + h_files):
                        problems = check_file(filename, args.verbose)
                        total_problems += problems
                        print("-" * 80)
        else:
            # Just analyze the specified directory
            total_problems = analyze_modules_directory(args.path, args.verbose)
    else:
        # Analyze a single file
        total_problems = check_file(args.path, args.verbose)
    
    if total_problems == 0:
        print("\nVALIDATION PASSED: No direct field accesses found.")
        return 0
    else:
        print(f"\nVALIDATION FAILED: Found {total_problems} direct field access issues that need to be fixed.")
        return 1

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: verify_property_access.py <file_path> [--verbose] [--recursive]")
        sys.exit(1)
    
    result = main()
    sys.exit(result)
