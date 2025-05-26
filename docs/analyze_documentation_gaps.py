#!/usr/bin/env python3
"""
Analyze SAGE codebase to identify undocumented public functions.
This helps prioritize documentation efforts.
"""

import os
import re
from pathlib import Path

def find_public_functions(filepath):
    """Extract public (non-static) function declarations from header files."""
    functions = []
    
    try:
        with open(filepath, 'r') as f:
            content = f.read()
            
        # Remove comments
        content = re.sub(r'/\*.*?\*/', '', content, flags=re.DOTALL)
        content = re.sub(r'//.*$', '', content, flags=re.MULTILINE)
        
        # Find function declarations (not static, not typedef)
        # Pattern: return_type function_name(params);
        pattern = r'^(?!static|typedef)[\w\s\*]+\s+(\w+)\s*\([^)]*\)\s*;'
        
        for match in re.finditer(pattern, content, re.MULTILINE):
            func_name = match.group(1)
            if func_name not in ['if', 'while', 'for', 'switch']:  # Avoid keywords
                functions.append(func_name)
                
    except Exception as e:
        print(f"Error reading {filepath}: {e}")
        
    return functions

def check_documentation(component_name, doc_dir):
    """Check if component has documentation."""
    doc_files = []
    
    # Check for exact match
    for ext in ['.md', '.txt', '.rst']:
        doc_path = doc_dir / f"{component_name}{ext}"
        if doc_path.exists():
            doc_files.append(doc_path.name)
            
    # Check for partial matches
    for doc_file in doc_dir.glob('*.md'):
        if component_name.lower() in doc_file.stem.lower():
            doc_files.append(doc_file.name)
            
    return doc_files

def analyze_component(header_path, src_dir, doc_dir):
    """Analyze a component's public interface and documentation status."""
    component_name = header_path.stem
    
    # Find public functions
    public_functions = find_public_functions(header_path)
    
    # Check for source file
    src_file = src_dir / f"{component_name}.c"
    has_implementation = src_file.exists()
    
    # Check for documentation
    doc_files = check_documentation(component_name, doc_dir)
    
    # Check for tests
    test_patterns = [
        f"test_{component_name}.c",
        f"test_{component_name.replace('core_', '')}.c",
        f"test_{component_name.replace('_', '')}.c"
    ]
    
    test_files = []
    test_dir = src_dir.parent / 'tests'
    for pattern in test_patterns:
        test_file = test_dir / pattern
        if test_file.exists():
            test_files.append(test_file.name)
    
    return {
        'name': component_name,
        'public_functions': public_functions,
        'has_implementation': has_implementation,
        'documentation': doc_files,
        'tests': test_files,
        'needs_attention': len(public_functions) > 0 and (not doc_files or not test_files)
    }

def main():
    # Set up paths
    sage_root = Path('/Users/dcroton/Documents/Science/projects/sage-repos/sage-model')
    core_dir = sage_root / 'src' / 'core'
    io_dir = sage_root / 'src' / 'io'
    docs_dir = sage_root / 'docs'
    
    print("# SAGE Component Analysis Report\n")
    print("Analyzing public interfaces to identify documentation/test gaps...\n")
    
    # Analyze core components
    print("## Core Components\n")
    core_headers = sorted(core_dir.glob('core_*.h'))
    
    needs_docs = []
    needs_tests = []
    
    for header in core_headers:
        if header.stem in ['core_allvars', 'core_simulation', 'macros']:
            continue  # Skip non-component headers
            
        result = analyze_component(header, core_dir, docs_dir)
        
        if result['public_functions']:
            print(f"### {result['name']}")
            print(f"- Public functions: {len(result['public_functions'])}")
            print(f"- Documentation: {result['documentation'] or 'NONE'}")
            print(f"- Tests: {result['tests'] or 'NONE'}")
            
            if not result['documentation']:
                needs_docs.append((result['name'], len(result['public_functions'])))
            if not result['tests']:
                needs_tests.append((result['name'], len(result['public_functions'])))
                
            if result['needs_attention']:
                print(f"- **STATUS: NEEDS ATTENTION**")
            print()
    
    # Analyze I/O components
    print("\n## I/O Components\n")
    io_headers = sorted(io_dir.glob('*.h'))
    
    for header in io_headers:
        if header.stem.startswith('io_'):
            result = analyze_component(header, io_dir, docs_dir)
            
            if result['public_functions']:
                print(f"### {result['name']}")
                print(f"- Public functions: {len(result['public_functions'])}")
                print(f"- Documentation: {result['documentation'] or 'NONE'}")
                print(f"- Tests: {result['tests'] or 'NONE'}")
                
                if not result['documentation']:
                    needs_docs.append((result['name'], len(result['public_functions'])))
                if not result['tests']:
                    needs_tests.append((result['name'], len(result['public_functions'])))
                    
                if result['needs_attention']:
                    print(f"- **STATUS: NEEDS ATTENTION**")
                print()
    
    # Summary
    print("\n## Summary\n")
    print(f"### Components needing documentation ({len(needs_docs)}):")
    for name, count in sorted(needs_docs, key=lambda x: x[1], reverse=True):
        print(f"- {name} ({count} public functions)")
        
    print(f"\n### Components needing tests ({len(needs_tests)}):")
    for name, count in sorted(needs_tests, key=lambda x: x[1], reverse=True):
        print(f"- {name} ({count} public functions)")

if __name__ == "__main__":
    main()
