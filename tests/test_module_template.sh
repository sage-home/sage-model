#!/bin/bash
# Test script for module template validation

# Change to the tests directory
cd "$(dirname "$0")"

# Clean any previous test output
rm -rf test_module_output

# Compile and run the tests
make -f Makefile.module_framework
./test_module_framework --no-cleanup

# Preserve test output for inspection
if [ $? -eq 0 ]; then
    echo -e "\nPreserving test output in test_module_output for inspection."
    mkdir -p test_module_output_preserved
    cp -r test_module_output/* test_module_output_preserved/
    echo "Test output preserved in tests/test_module_output_preserved/"
fi

# Return the exit code of the test program
exit $?
