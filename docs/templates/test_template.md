# Unit Test Template

Use this template when creating new unit tests for SAGE components.

---

## Template Structure

```c
/**
 * Unit test for [Component Name]
 * 
 * Tests cover:
 * - Basic functionality
 * - Error handling  
 * - Edge cases
 * - Integration points
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "[component_header].h"

// Test counter for reporting
static int tests_run = 0;
static int tests_passed = 0;

/**
 * Test: Component initialization
 */
static int test_component_init(void) {
    printf("  Testing component initialization...\n");
    
    struct component *comp = NULL;
    int result = component_init(&comp);
    
    assert(result == SUCCESS);
    assert(comp != NULL);
    
    // Clean up
    if (comp) {
        component_cleanup(comp);
    }
    
    printf("    SUCCESS: Component initialization works\n");
    return 0;  // Test success
}

/**
 * Test: Basic functionality
 */
static int test_basic_functionality(void) {
    printf("  Testing basic functionality...\n");
    
    // Test implementation
    // ...
    
    printf("    SUCCESS: Basic functionality works\n");
    return 0;
}

/**
 * Test: Error handling
 */
static int test_error_handling(void) {
    printf("  Testing error handling...\n");
    
    // Test NULL parameters
    int result = component_init(NULL);
    assert(result == ERROR_INVALID_PARAM);
    
    // Test other error conditions
    // ...
    
    printf("    SUCCESS: Error handling works\n");
    return 0;
}

/**
 * Test: Edge cases
 */
static int test_edge_cases(void) {
    printf("  Testing edge cases...\n");
    
    // Test boundary conditions
    // Test empty inputs
    // Test maximum values
    // ...
    
    printf("    SUCCESS: Edge cases handled correctly\n");
    return 0;
}

/**
 * Test: Integration with other components
 */
static int test_integration(void) {
    printf("  Testing integration...\n");
    
    // Test interaction with other SAGE components
    // ...
    
    printf("    SUCCESS: Integration works\n");
    return 0;
}

//=============================================================================
// Test Runner
//=============================================================================

int main(int argc, char *argv[]) {
    printf("\nRunning %s...\n", __FILE__);
    printf("\n=== Testing [Component Name] ===\n");
    
    printf("This test verifies:\n");
    printf("  1. Component initialization and cleanup\n");
    printf("  2. Basic functionality operates correctly\n");
    printf("  3. Error conditions are handled properly\n");
    printf("  4. Edge cases work as expected\n");
    printf("  5. Integration with other components\n\n");

    int result = 0;
    int test_count = 0;
    int passed_count = 0;
    
    // Run tests
    test_count++;
    if (test_component_init() == 0) {
        passed_count++;
    } else {
        result = 1;
    }
    
    test_count++;
    if (test_basic_functionality() == 0) {
        passed_count++;
    } else {
        result = 1;
    }
    
    test_count++;
    if (test_error_handling() == 0) {
        passed_count++;
    } else {
        result = 1;
    }
    
    test_count++;
    if (test_edge_cases() == 0) {
        passed_count++;
    } else {
        result = 1;
    }
    
    test_count++;
    if (test_integration() == 0) {
        passed_count++;
    } else {
        result = 1;
    }
    
    // Report results
    printf("\n=== Test Results ===\n");
    printf("Passed: %d/%d tests\n", passed_count, test_count);
    
    if (result == 0) {
        printf("%s PASSED\n", __FILE__);
    } else {
        printf("%s FAILED\n", __FILE__);
    }
    
    return result;
}
```

## Test Writing Guidelines

1. **Test Naming**: Use descriptive names that explain what is being tested
2. **Test Independence**: Each test should be independent and not rely on others
3. **Test Coverage**: Aim to test:
   - Normal operation (happy path)
   - Error conditions
   - Boundary conditions
   - Integration points
4. **Assertions**: Use `assert()` for critical failures that should stop the test
5. **Output**: Use `printf()` statements for clear, descriptive, well-formatted output
6. **Return Values**: Return 0 for success, non-zero for failure
7. **Cleanup**: Always clean up resources in each test function

## CMake Integration

Tests are automatically detected and integrated when you:

1. **Create test file**: `tests/test_your_name.c`
2. **Reconfigure CMake**: `cmake ..` (from build directory)
3. **Build and run**: `cmake --build . --target test_your_name && ctest -R test_your_name`

### CMake Features

The CMake system automatically:
- Detects new test files in the `tests/` directory
- Creates executable targets linking with the SAGE library
- Registers tests with CTest for individual execution
- Assigns tests to appropriate categories based on naming patterns
- Provides all necessary include directories and library links

### Test Categories

Tests are automatically categorized based on naming:
- `test_core_*` → core_tests
- `test_property_*` → property_tests  
- `test_io_*` → io_tests
- `test_module_*` → module_tests
- `test_tree_*` → tree_tests

## Quick Reference

```bash
# Essential commands
cmake --build . --target unit_tests     # Fast unit tests only
ctest -R test_name -V                    # Run specific test with output
ctest --output-on-failure               # Show output only on failure

# Add new test
# 1. Create tests/test_name.c using this template
# 2. Run: cmake .. && cmake --build . --target test_name
# 3. Test: ctest -R test_name -V
```