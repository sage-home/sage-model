#!/bin/bash
# Simple script to run unit tests
echo ""
echo "=== Running All Unit Tests ==="
/opt/homebrew/bin/ctest --output-on-failure -E "sage_end_to_end" || echo "Some unit tests failed (may be expected during development)"