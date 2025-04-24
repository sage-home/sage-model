#!/bin/bash

# Script to run the evolve_galaxies loop integration tests
# with optional debugging capabilities

set -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$SCRIPT_DIR/.."

# Default options
DEBUG=0
VERBOSE=0
VALGRIND=0

# Process command line options
while [ $# -gt 0 ]; do
    case "$1" in
        --debug)
            DEBUG=1
            shift
            ;;
        --verbose)
            VERBOSE=1
            shift
            ;;
        --valgrind)
            VALGRIND=1
            shift
            ;;
        --help)
            echo "Usage: $0 [options]"
            echo "Options:"
            echo "  --debug    Run test with debug output"
            echo "  --verbose  Show verbose output"
            echo "  --valgrind Run with valgrind for memory checking"
            echo "  --help     Show this help"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            echo "Use --help for usage information."
            exit 1
            ;;
    esac
done

# Build the test
echo "Building evolve_integration test..."
make test_evolve_integration

# Run with appropriate options
if [ $VALGRIND -eq 1 ]; then
    echo "Running with valgrind..."
    valgrind --leak-check=full --track-origins=yes ./tests/test_evolve_integration
elif [ $DEBUG -eq 1 ]; then
    echo "Running with debug options..."
    # Set environment variable to increase logging level
    SAGE_LOG_LEVEL=DEBUG ./tests/test_evolve_integration
elif [ $VERBOSE -eq 1 ]; then
    echo "Running with verbose output..."
    SAGE_LOG_LEVEL=INFO ./tests/test_evolve_integration
else
    echo "Running standard test..."
    # Higher log level to reduce output
    SAGE_LOG_LEVEL=WARNING ./tests/test_evolve_integration
fi

echo "Test completed successfully!"
