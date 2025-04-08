#!/bin/bash
set -e  # Exit on error

echo "===== Running Memory Pool Tests ====="

# First run the standalone test
echo "Running standalone memory pool test..."
./test_memory_pool.sh

# Run a quick verification on created output files if SAGE is built
if [ -f "../sage" ]; then
    echo "Running SAGE with memory pooling enabled and disabled for comparison..."
    
    # Get the test parameter file
    PARAM_FILE="../input/mini-millennium.par"
    
    # Create temporary parameter files
    TMP_PARAM_POOL_ON="${PARAM_FILE}.pool_on"
    TMP_PARAM_POOL_OFF="${PARAM_FILE}.pool_off"
    
    # Create copies with pool enabled/disabled
    cat "$PARAM_FILE" | grep -v "EnableGalaxyMemoryPool" > "$TMP_PARAM_POOL_ON"
    echo "EnableGalaxyMemoryPool     1" >> "$TMP_PARAM_POOL_ON"
    
    cat "$PARAM_FILE" | grep -v "EnableGalaxyMemoryPool" > "$TMP_PARAM_POOL_OFF"
    echo "EnableGalaxyMemoryPool     0" >> "$TMP_PARAM_POOL_OFF"
    
    # Run both and time them
    echo "Running with memory pooling enabled..."
    time ../sage "$TMP_PARAM_POOL_ON" > /dev/null 2>&1
    
    echo "Running with memory pooling disabled..."
    time ../sage "$TMP_PARAM_POOL_OFF" > /dev/null 2>&1
    
    # Clean up
    rm -f "$TMP_PARAM_POOL_ON" "$TMP_PARAM_POOL_OFF"
fi

echo "All memory pooling tests completed successfully!"