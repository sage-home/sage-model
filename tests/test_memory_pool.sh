#!/bin/bash
set -e  # Exit on any error

# Ensure stubs directory exists
mkdir -p stubs

# Compile the test program
make -f Makefile.memory_pool

# Run the test
./test_memory_pool

echo "Memory Pool tests completed successfully!"