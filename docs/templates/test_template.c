# Test Template

Use this template when creating new tests for SAGE components.

---

```c
/**
 * Test suite for [Component Name]
 * 
 * Tests cover:
 * - Basic functionality
 * - Error handling
 * - Edge cases
 * - Integration points
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "[component_header].h"

// Test counter for reporting
static int tests_run = 0;
static int tests_passed = 0;

// Helper macro for test assertions
#define TEST_ASSERT(condition, message) do { \
    tests_run++; \
    if (!(condition)) { \
        printf("FAIL: %s\n", message); \
        printf("  at %s:%d\n", __FILE__, __LINE__); \
    } else { \
        tests_passed++; \
    } \
} while(0)

// Test fixtures
static struct test_context {
    // Add test data here
    void *component;
    int initialized;
} test_ctx;

// Setup function - called before tests
static int setup_test_context(void) {
    memset(&test_ctx, 0, sizeof(test_ctx));
    
    // Initialize test data
    // ...
    
    test_ctx.initialized = 1;
    return 0;
}

// Teardown function - called after tests
static void teardown_test_context(void) {
    if (test_ctx.component) {
        // Clean up
        test_ctx.component = NULL;
    }
    test_ctx.initialized = 0;
}

//=============================================================================
// Test Cases
//=============================================================================

/**
 * Test: Component initialization
 */
static void test_component_init(void) {
    printf("\n=== Testing component initialization ===\n");
    
    struct component *comp = NULL;
    int result = component_init(&comp);
    
    TEST_ASSERT(result == SUCCESS, "component_init should return SUCCESS");
    TEST_ASSERT(comp != NULL, "component should be allocated");
    
    // Clean up
    if (comp) {
        component_cleanup(comp);
    }
}

/**
 * Test: Basic functionality
 */
static void test_basic_functionality(void) {
    printf("\n=== Testing basic functionality ===\n");
    
    // Test implementation
    // ...
    
    TEST_ASSERT(1 == 1, "Basic test should pass");
}

/**
 * Test: Error handling
 */
static void test_error_handling(void) {
    printf("\n=== Testing error handling ===\n");
    
    // Test NULL parameters
    int result = component_init(NULL);
    TEST_ASSERT(result == ERROR_INVALID_PARAM, 
                "component_init(NULL) should return ERROR_INVALID_PARAM");
    
    // Test other error conditions
    // ...
}

/**
 * Test: Edge cases
 */
static void test_edge_cases(void) {
    printf("\n=== Testing edge cases ===\n");
    
    // Test boundary conditions
    // Test empty inputs
    // Test maximum values
    // ...
    
    TEST_ASSERT(1 == 1, "Edge case test placeholder");
}

/**
 * Test: Integration with other components
 */
static void test_integration(void) {
    printf("\n=== Testing integration ===\n");
    
    // Test interaction with other SAGE components
    // ...
    
    TEST_ASSERT(1 == 1, "Integration test placeholder");
}

//=============================================================================
// Test Runner
//=============================================================================

int main(int argc, char *argv[]) {
    printf("========================================\n");
    printf("Starting tests for [test_component_name]\n");
    printf("========================================\n");
    
    // Setup
    if (setup_test_context() != 0) {
        printf("ERROR: Failed to set up test context\n");
        return 1;
    }
    
    // Run tests
    test_component_init();
    test_basic_functionality();
    test_error_handling();
    test_edge_cases();
    test_integration();
    
    // Teardown
    teardown_test_context();
    
    // Report results
    printf("\n========================================\n");
    printf("Test results for [test_component_name]:\n");
    printf("  Total tests: %d\n", tests_run);
    printf("  Passed: %d\n", tests_passed);
    printf("  Failed: %d\n", tests_run - tests_passed);
    printf("========================================\n");
    
    return (tests_run == tests_passed) ? 0 : 1;
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
4. **Assertions**: Use clear assertion messages that explain what failed
5. **Setup/Teardown**: Use these to ensure consistent test environment
6. **Mocking**: Create mock objects when testing components in isolation

## Makefile Integration

Add to Makefile:
```makefile
test_component_name: tests/test_component_name.c $(SAGELIB)
	$(CC) $(OPTS) $(OPTIMIZE) $(CCFLAGS) -o tests/test_component_name tests/test_component_name.c -L. -l$(LIBNAME) $(LIBFLAGS)
```

Add to appropriate test category (CORE_TESTS, IO_TESTS, etc.)
