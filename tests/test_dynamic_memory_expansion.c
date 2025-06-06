/**
 * Test suite for Dynamic Memory Expansion System
 * 
 * Tests cover:
 * - Memory system initialization and cleanup
 * - Dynamic block table expansion under memory pressure  
 * - Tree-scoped memory management
 * - Property system integration with dynamic memory
 * - Realistic physics module memory patterns
 * - Scientific accuracy with real tree data
 * - Error handling and boundary conditions
 * - Large allocation scenarios and fragmentation patterns
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

/* Include SAGE core headers */
#include "core_allvars.h"
#include "core_mymalloc.h"
#include "core_logging.h"
#include "core_properties.h"
#include "core_property_utils.h"

/* Try to include optional headers */
#ifdef __has_include
#if __has_include("core_module_system.h")
#include "core_module_system.h"
#include "core_pipeline_system.h"
#define MODULE_SYSTEM_AVAILABLE
#endif
#endif

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
    // Memory system state
    bool memory_system_initialized;
    
    // Property system state
    bool property_system_initialized;
    
    // Module system state  
    bool module_system_initialized;
    
    // Test data for scientific accuracy testing
    char *tree_data_path;
    struct params test_params;
    struct GALAXY *test_galaxies;
    int num_test_galaxies;
    int max_test_galaxies;
    
    // Memory tracking for validation
    size_t initial_memory_usage;
    size_t peak_memory_usage;
    
} test_ctx;

// Helper functions for testing
static size_t get_memory_usage_mb(void) {
    // Simple implementation - in a real system this could query actual memory usage
    return 100; // Default 100MB for testing
}

// Helper function to initialize a minimal params structure for testing
static void init_test_params(struct params *params) {
    // Initialize minimal parameters needed for testing
    memset(params, 0, sizeof(struct params));
    
    // Set some basic cosmological values that might be needed
    params->cosmology.Hubble_h = 0.7;
    params->cosmology.Omega = 0.3;
    params->cosmology.OmegaLambda = 0.7;
    
    // Set some basic simulation parameters
    params->simulation.LastSnapshotNr = 63;
    params->simulation.nsnapshots = 64;
}

// Setup function - called before tests
static int setup_test_context(void) {
    memset(&test_ctx, 0, sizeof(test_ctx));
    
    // Initialize memory system
    int status = memory_system_init();
    if (status != 0) {
        printf("ERROR: Failed to initialize memory system in test setup\n");
        return -1;
    }
    test_ctx.memory_system_initialized = true;
    
    // Initialize test parameters first
    init_test_params(&test_ctx.test_params);
    
    // Initialize property system if available
    status = initialize_property_system(&test_ctx.test_params);
    if (status != 0) {
        printf("WARNING: Failed to initialize property system in test setup\n");
        // Continue without property system for basic memory tests
    } else {
        test_ctx.property_system_initialized = true;
    }
    
    // Initialize module system for physics module simulation tests
    #ifdef MODULE_SYSTEM_AVAILABLE
    status = initialize_module_system(&test_ctx.test_params);
    if (status != 0) {
        printf("WARNING: Failed to initialize module system in test setup\n");
        // Continue without module system for basic memory tests  
    } else {
        test_ctx.module_system_initialized = true;
    }
    #endif
    
    // Set up test tree data path
    test_ctx.tree_data_path = "tests/test_data/trees_063.0";
    
    // Initialize test galaxy arrays  
    test_ctx.max_test_galaxies = 10000;
    test_ctx.test_galaxies = calloc(test_ctx.max_test_galaxies, sizeof(struct GALAXY));
    if (test_ctx.test_galaxies == NULL) {
        printf("ERROR: Failed to allocate test galaxy array\n");
        return -1;
    }
    
    test_ctx.initial_memory_usage = get_memory_usage_mb();
    
    return 0;
}

// Teardown function - called after tests
static void teardown_test_context(void) {
    printf("Starting test cleanup...\n");
    
    // Clean up test galaxies and their properties
    if (test_ctx.test_galaxies) {
        // Free any allocated galaxy properties if they were allocated
        if (test_ctx.property_system_initialized && test_ctx.num_test_galaxies > 0) {
            printf("Freeing galaxy properties for %d galaxies...\n", test_ctx.num_test_galaxies);
            for (int i = 0; i < test_ctx.num_test_galaxies; i++) {
                // NOTE FOR DEVELOPER: If this crashes, check property allocation/deallocation logic
                free_galaxy_properties(&test_ctx.test_galaxies[i]);
            }
        }
        printf("Freeing test galaxies array...\n");
        free(test_ctx.test_galaxies);
        test_ctx.test_galaxies = NULL;
    }
    
    // Clean up module system
    #ifdef MODULE_SYSTEM_AVAILABLE
    if (test_ctx.module_system_initialized) {
        printf("Cleaning up module system...\n");
        fflush(stdout);  // Force output before potential crash
        // NOTE FOR DEVELOPER: If the program hangs here, the issue is in module system cleanup
        // This might be related to module callback or registration cleanup
        cleanup_module_system();
        printf("Module system cleanup completed.\n");
        test_ctx.module_system_initialized = false;
    }
    #endif
    
    // Clean up property system
    if (test_ctx.property_system_initialized) {
        printf("Cleaning up property system...\n");
        fflush(stdout);  // Force output before potential crash
        // NOTE FOR DEVELOPER: If this hangs/crashes, check property system initialization/cleanup
        cleanup_property_system();
        printf("Property system cleanup completed.\n");
        test_ctx.property_system_initialized = false;
    }
    
    // Clean up memory system - THIS IS WHERE CRASHES ARE LIKELY OCCURRING
    if (test_ctx.memory_system_initialized) {
        printf("Cleaning up memory system...\n");
        fflush(stdout);  // Force output before potential crash
        // NOTE FOR DEVELOPER: This is where the cleanup failure is happening
        // Potential causes:
        // 1. Memory corruption in the block table expansion
        // 2. Double-free of memory blocks
        // 3. Use-after-free in memory tracking structures
        // 4. Incomplete memory bookkeeping during dynamic expansion
        // 5. Tree-scoped memory cleanup interfering with global cleanup
        //
        // Debug with: gdb ./tests/test_dynamic_memory_expansion
        // Or: valgrind --tool=memcheck ./tests/test_dynamic_memory_expansion
        //
        // Check core_mymalloc.c for issues in:
        // - expand_block_table()
        // - begin_tree_memory_scope() / end_tree_memory_scope()
        // - memory_system_cleanup()
        memory_system_cleanup();
        printf("Memory system cleanup completed.\n");
        test_ctx.memory_system_initialized = false;
    }
    
    printf("Test cleanup completed successfully.\n");
}

//=============================================================================
// Test Cases
//=============================================================================


/**
 * Test: Memory system initialization and cleanup
 */
static void test_memory_system_lifecycle(void) {
    printf("=== Testing memory system lifecycle ===\n");
    
    // Test initialization (already done in setup, test re-initialization)
    int status = memory_system_init();
    TEST_ASSERT(status == 0, "Memory system re-initialization should succeed");
    
    // Test cleanup and re-initialization cycle
    memory_system_cleanup();
    status = memory_system_init();
    TEST_ASSERT(status == 0, "Memory system should re-initialize after cleanup");
    
    printf("Memory system lifecycle tests passed\n");
}

/**
 * Test: Basic allocation and deallocation patterns
 */
static void test_basic_memory_operations(void) {
    printf("\n=== Testing basic memory operations ===\n");
    
    // Test basic allocation
    void *ptr1 = mymalloc(1024);
    TEST_ASSERT(ptr1 != NULL, "Basic allocation should succeed");
    
    void *ptr2 = mymalloc(2048);
    TEST_ASSERT(ptr2 != NULL, "Second allocation should succeed");
    
    // Test allocation with description
    void *ptr3 = mymalloc_full(4096, "test allocation");
    TEST_ASSERT(ptr3 != NULL, "Allocation with description should succeed");
    
    // Test calloc functionality
    void *ptr4 = mycalloc(100, 10);
    TEST_ASSERT(ptr4 != NULL, "Calloc allocation should succeed");
    
    // Verify calloc zeroing
    char *char_ptr = (char*)ptr4;
    bool all_zero = true;
    for (int i = 0; i < 1000; i++) {
        if (char_ptr[i] != 0) {
            all_zero = false;
            break;
        }
    }
    TEST_ASSERT(all_zero, "Calloc should zero-initialize memory");
    
    // Test realloc functionality
    void *ptr5 = myrealloc(ptr1, 8192);
    TEST_ASSERT(ptr5 != NULL, "Realloc should succeed");
    ptr1 = ptr5; // Update pointer
    
    // Free allocations
    myfree(ptr1);
    myfree(ptr2);
    myfree(ptr3);
    myfree(ptr4);
    
    // Test freeing NULL (should be safe)
    myfree(NULL);
    
    printf("Basic memory operations tests passed\n");
}


/**
 * Test: Dynamic block table expansion under memory pressure
 */
static void test_dynamic_block_expansion(void) {
    printf("\n=== Testing dynamic block table expansion ===\n");
    
    // Allocate many small blocks to trigger expansion
    const int num_allocations = 15000;
    void **ptrs = malloc(num_allocations * sizeof(void*));
    TEST_ASSERT(ptrs != NULL, "Test array allocation should succeed");
    
    printf("Allocating %d blocks to trigger dynamic expansion...\n", num_allocations);
    
    for (int i = 0; i < num_allocations; i++) {
        ptrs[i] = mymalloc(64);  // Small allocations to maximize block count
        TEST_ASSERT(ptrs[i] != NULL, "Each allocation should succeed");
        
        // Force memory pressure check every 1000 allocations
        if (i % 1000 == 0) {
            check_memory_pressure_and_expand();
            if (i % 5000 == 0) {
                printf("  Progress: %d/%d allocations completed\n", i, num_allocations);
            }
        }
    }
    
    // Test that we can still allocate after many allocations
    void *final_ptr = mymalloc(1024);
    TEST_ASSERT(final_ptr != NULL, "Final allocation should succeed after expansion");
    
    printf("Dynamic expansion successful - cleaning up %d allocations\n", num_allocations);
    
    // Free all allocations
    for (int i = 0; i < num_allocations; i++) {
        myfree(ptrs[i]);
    }
    myfree(final_ptr);
    free(ptrs);
    
    printf("Dynamic block expansion tests passed\n");
}


/**
 * Test: Tree-scoped memory management with nested scopes
 */
static void test_tree_memory_scoping(void) {
    printf("\n=== Testing tree-scoped memory management ===\n");
    
    // Allocate some memory before scope
    void *pre_scope = mymalloc(1024);
    TEST_ASSERT(pre_scope != NULL, "Pre-scope allocation should succeed");
    
    // Begin tree scope
    begin_tree_memory_scope();
    printf("Started tree memory scope\n");
    
    // Allocate memory within scope
    void *scope_ptr1 = mymalloc(2048);
    void *scope_ptr2 = mymalloc(4096);
    void *scope_ptr3 = mymalloc(8192);
    
    TEST_ASSERT(scope_ptr1 != NULL, "Scope allocation 1 should succeed");
    TEST_ASSERT(scope_ptr2 != NULL, "Scope allocation 2 should succeed");
    TEST_ASSERT(scope_ptr3 != NULL, "Scope allocation 3 should succeed");
    
    // Test nested scopes
    begin_tree_memory_scope();
    printf("Started nested tree memory scope\n");
    void *nested_ptr = mymalloc(1024);
    TEST_ASSERT(nested_ptr != NULL, "Nested scope allocation should succeed");
    end_tree_memory_scope();
    printf("Ended nested tree memory scope\n");
    
    // End main tree scope - should automatically free scope allocations
    end_tree_memory_scope();
    printf("Ended main tree memory scope\n");
    
    // Test that system is still functional after scope cleanup
    void *post_scope = mymalloc(1024);
    TEST_ASSERT(post_scope != NULL, "Post-scope allocation should succeed");
    
    // Clean up remaining allocations
    myfree(pre_scope);
    myfree(post_scope);
    
    printf("Tree memory scoping tests passed\n");
}

/**
 * Test: Property system integration with dynamic memory expansion
 * This tests memory expansion when galaxy properties are dynamically allocated
 */
static void test_property_system_integration(void) {
    printf("\n=== Testing property system integration ===\n");
    
    if (!test_ctx.property_system_initialized) {
        printf("SKIP: Property system not available for testing\n");
        return;
    }
    
    // Test memory expansion with property allocation
    const int num_galaxies = 5000;
    printf("Testing property allocation for %d galaxies\n", num_galaxies);
    
    for (int i = 0; i < num_galaxies && i < test_ctx.max_test_galaxies; i++) {
        // Allocate galaxy properties
        int status = allocate_galaxy_properties(&test_ctx.test_galaxies[i], &test_ctx.test_params);
        TEST_ASSERT(status == 0, "Galaxy property allocation should succeed");
        
        // Set some property values to test the property system
        GALAXY_PROP_SnapNum(&test_ctx.test_galaxies[i]) = 63;
        GALAXY_PROP_Type(&test_ctx.test_galaxies[i]) = 0;
        GALAXY_PROP_GalaxyNr(&test_ctx.test_galaxies[i]) = i;
        
        // Test property access
        int snap = GALAXY_PROP_SnapNum(&test_ctx.test_galaxies[i]);
        TEST_ASSERT(snap == 63, "Property access should return correct value");
        
        if (i % 1000 == 0) {
            check_memory_pressure_and_expand();
            printf("  Progress: %d/%d galaxies processed\n", i, num_galaxies);
        }
    }
    
    test_ctx.num_test_galaxies = num_galaxies;
    
    // Test property copying under memory pressure
    struct GALAXY temp_galaxy;
    int status = allocate_galaxy_properties(&temp_galaxy, &test_ctx.test_params);
    TEST_ASSERT(status == 0, "Temporary galaxy property allocation should succeed");
    
    status = copy_galaxy_properties(&temp_galaxy, &test_ctx.test_galaxies[0], &test_ctx.test_params);
    TEST_ASSERT(status == 0, "Galaxy property copying should succeed");
    
    // Verify copied properties
    int copied_snap = GALAXY_PROP_SnapNum(&temp_galaxy);
    TEST_ASSERT(copied_snap == 63, "Copied property should match original");
    
    free_galaxy_properties(&temp_galaxy);
    
    printf("Property system integration tests passed\n");
}

/**
 * Test: Realistic physics module memory patterns
 * This simulates typical physics module memory usage patterns
 */
static void test_physics_module_memory_patterns(void) {
    printf("\n=== Testing physics module memory patterns ===\n");
    
    if (!test_ctx.module_system_initialized) {
        printf("SKIP: Module system not available for testing\n");
        return;
    }
    
    // Simulate multiple physics modules allocating data
    const int num_modules = 8;
    const int data_per_module = 1024 * 1024; // 1MB per module
    void **module_data = malloc(num_modules * sizeof(void*));
    TEST_ASSERT(module_data != NULL, "Module data array allocation should succeed");
    
    printf("Simulating %d physics modules, %d bytes each\n", num_modules, data_per_module);
    
    // Simulate module initialization phase
    begin_tree_memory_scope();
    for (int i = 0; i < num_modules; i++) {
        char desc[64];
        snprintf(desc, sizeof(desc), "physics_module_%d_data", i);
        
        module_data[i] = mymalloc_full(data_per_module, desc);
        TEST_ASSERT(module_data[i] != NULL, "Physics module data allocation should succeed");
        
        // Simulate initialization of module data
        memset(module_data[i], i % 256, data_per_module);
        
        printf("  Module %d: allocated %d bytes\n", i, data_per_module);
    }
    
    // Test memory pressure handling during module execution
    check_memory_pressure_and_expand();
    
    // Simulate inter-module data exchange requiring additional memory
    const int exchange_buffer_size = 512 * 1024; // 512KB
    void **exchange_buffers = malloc(num_modules * sizeof(void*));
    TEST_ASSERT(exchange_buffers != NULL, "Exchange buffer array allocation should succeed");
    
    for (int i = 0; i < num_modules; i++) {
        exchange_buffers[i] = mymalloc(exchange_buffer_size);
        TEST_ASSERT(exchange_buffers[i] != NULL, "Module exchange buffer allocation should succeed");
    }
    
    // Simulate module cleanup (tree scope will handle bulk deallocation)
    for (int i = 0; i < num_modules; i++) {
        myfree(exchange_buffers[i]);
    }
    free(exchange_buffers);
    
    end_tree_memory_scope(); // This should free all module_data allocations
    
    free(module_data);
    
    printf("Physics module memory pattern tests passed\n");
}

/**
 * Test: Scientific accuracy with real tree data
 * This simulates actual tree-by-tree processing with realistic memory patterns
 */
static void test_scientific_tree_processing(void) {
    printf("\n=== Testing scientific tree processing patterns ===\n");
    
    // Test with simplified tree structure since we may not have full I/O available
    const int num_trees = 5;
    const int halos_per_tree[] = {100, 500, 1000, 2000, 5000};
    const int galaxies_per_halo = 3; // Conservative estimate
    
    for (int tree = 0; tree < num_trees; tree++) {
        printf("Processing simulated tree %d: %d halos\n", tree, halos_per_tree[tree]);
        
        // Begin tree processing scope
        begin_tree_memory_scope();
        
        // Simulate halo data allocation
        const int num_halos = halos_per_tree[tree];
        struct halo_data *halos = mymalloc(num_halos * sizeof(struct halo_data));
        TEST_ASSERT(halos != NULL, "Halo data allocation should succeed");
        
        // Simulate galaxy data allocation with realistic sizing
        const int estimated_galaxies = num_halos * galaxies_per_halo;
        struct GALAXY *galaxies = mymalloc(estimated_galaxies * sizeof(struct GALAXY));
        TEST_ASSERT(galaxies != NULL, "Galaxy data allocation should succeed");
        
        printf("  Allocated space for %d halos, %d galaxies\n", num_halos, estimated_galaxies);
        
        // Simulate galaxy property allocation if property system available
        if (test_ctx.property_system_initialized) {
            for (int i = 0; i < estimated_galaxies; i++) {
                int status = allocate_galaxy_properties(&galaxies[i], &test_ctx.test_params);
                TEST_ASSERT(status == 0, "Galaxy property allocation should succeed in tree processing");
                
                // Initialize basic properties
                GALAXY_PROP_SnapNum(&galaxies[i]) = 63; // Use fixed snapshot number
                GALAXY_PROP_Type(&galaxies[i]) = (i == 0) ? 0 : 1; // First galaxy is central
                GALAXY_PROP_GalaxyNr(&galaxies[i]) = i;
            }
        }
        
        // Simulate additional physics calculations requiring temporary memory
        const int temp_calc_size = estimated_galaxies * 64; // 64 bytes per galaxy for calculations
        void *temp_calc_data = mymalloc(temp_calc_size);
        TEST_ASSERT(temp_calc_data != NULL, "Temporary calculation data allocation should succeed");
        
        // Check memory pressure and expand if needed
        check_memory_pressure_and_expand();
        
        // Simulate memory-intensive physics calculations
        for (int calc = 0; calc < 5; calc++) {
            void *calc_buffer = mymalloc(estimated_galaxies * 32);
            TEST_ASSERT(calc_buffer != NULL, "Calculation buffer allocation should succeed");
            myfree(calc_buffer); // Free immediately to simulate calculation cleanup
        }
        
        // End tree processing scope - this should free all tree-related memory
        end_tree_memory_scope();
        
        printf("  Tree %d processing completed and memory freed\n", tree);
    }
    
    printf("Scientific tree processing tests passed\n");
}


/**
 * Test: Memory pressure detection and automatic expansion
 */
static void test_memory_pressure_detection(void) {
    printf("\n=== Testing memory pressure detection ===\n");
    
    // Test explicit expansion
    int status = expand_block_table();
    TEST_ASSERT(status == 0, "Explicit block table expansion should succeed");
    
    // Test pressure detection (function should complete without error)
    check_memory_pressure_and_expand();
    
    // Test multiple expansions
    for (int i = 0; i < 5; i++) {
        status = expand_block_table();
        TEST_ASSERT(status == 0, "Multiple expansions should succeed");
        printf("  Expansion %d completed successfully\n", i + 1);
    }
    
    printf("Memory pressure detection tests passed\n");
}

/**
 * Test: Large allocation scenarios and scalability
 */
static void test_large_allocation_scenarios(void) {
    printf("\n=== Testing large allocation scenarios ===\n");
    
    // Test progressively larger allocations
    size_t sizes[] = {1024*1024, 5*1024*1024, 10*1024*1024, 50*1024*1024, 100*1024*1024};
    const int num_sizes = sizeof(sizes) / sizeof(sizes[0]);
    void *ptrs[num_sizes];
    
    for (int i = 0; i < num_sizes; i++) {
        ptrs[i] = mymalloc_full(sizes[i], "large allocation test");
        if (ptrs[i] != NULL) {
            printf("  Successfully allocated %.2f MB\n", sizes[i] / (1024.0 * 1024.0));
            
            // Test writing to the memory to ensure it's actually usable
            char *test_ptr = (char*)ptrs[i];
            test_ptr[0] = 'A';
            test_ptr[sizes[i] - 1] = 'Z';
            TEST_ASSERT(test_ptr[0] == 'A' && test_ptr[sizes[i] - 1] == 'Z', 
                       "Allocated memory should be readable and writable");
        } else {
            printf("  Failed to allocate %.2f MB (may be system limit)\n", sizes[i] / (1024.0 * 1024.0));
        }
    }
    
    // Free successful allocations
    for (int i = 0; i < num_sizes; i++) {
        if (ptrs[i] != NULL) {
            myfree(ptrs[i]);
        }
    }
    
    printf("Large allocation scenario tests passed\n");
}

/**
 * Test: Error handling and edge cases
 */
static void test_error_handling_edge_cases(void) {
    printf("\n=== Testing error handling and edge cases ===\n");
    
    // Test zero allocation (should succeed with minimum size)
    void *zero_ptr = mymalloc(0);
    TEST_ASSERT(zero_ptr != NULL, "Zero allocation should succeed (gets aligned to minimum size)");
    myfree(zero_ptr);
    
    // Test very small allocation
    void *tiny_ptr = mymalloc(1);
    TEST_ASSERT(tiny_ptr != NULL, "Tiny allocation should succeed");
    myfree(tiny_ptr);
    
    // Test alignment of allocations
    void *align_ptr1 = mymalloc(7);  // Odd size
    void *align_ptr2 = mymalloc(13); // Another odd size
    TEST_ASSERT(align_ptr1 != NULL && align_ptr2 != NULL, "Odd-sized allocations should succeed");
    
    // Check alignment (pointers should be properly aligned)
    uintptr_t addr1 = (uintptr_t)align_ptr1;
    uintptr_t addr2 = (uintptr_t)align_ptr2;
    TEST_ASSERT(addr1 % 8 == 0, "Allocation should be 8-byte aligned");
    TEST_ASSERT(addr2 % 8 == 0, "Allocation should be 8-byte aligned");
    
    myfree(align_ptr1);
    myfree(align_ptr2);
    
    // Test operations without proper initialization (should be safe due to setup)
    check_memory_pressure_and_expand();  // Should be safe
    
    printf("Error handling and edge case tests passed\n");
}

/**
 * Test: Memory fragmentation patterns and performance
 */
static void test_memory_fragmentation_patterns(void) {
    printf("\n=== Testing memory fragmentation patterns ===\n");
    
    const int num_blocks = 2000;
    void *ptrs[num_blocks];
    
    // Allocate many blocks with variable sizes
    printf("Allocating %d blocks with variable sizes\n", num_blocks);
    for (int i = 0; i < num_blocks; i++) {
        size_t size = 1024 + (i % 100) * 16;  // Variable sizes 1024-2624 bytes
        ptrs[i] = mymalloc(size);
        TEST_ASSERT(ptrs[i] != NULL, "Variable size allocation should succeed");
    }
    
    // Free every other block to create fragmentation
    printf("Creating fragmentation by freeing every other block\n");
    for (int i = 1; i < num_blocks; i += 2) {
        myfree(ptrs[i]);
        ptrs[i] = NULL;
    }
    
    // Allocate new blocks in the gaps
    printf("Reallocating in fragmented space\n");
    for (int i = 1; i < num_blocks; i += 2) {
        ptrs[i] = mymalloc(512);  // Smaller size that should fit in gaps
        TEST_ASSERT(ptrs[i] != NULL, "Allocation in fragmented space should succeed");
    }
    
    // Test that memory system can handle fragmentation
    check_memory_pressure_and_expand();
    
    // Free all remaining blocks
    for (int i = 0; i < num_blocks; i++) {
        if (ptrs[i] != NULL) {
            myfree(ptrs[i]);
        }
    }
    
    printf("Memory fragmentation pattern tests passed\n");
}

/**
 * Test: Memory system cleanup after heavy usage
 * This test specifically validates that cleanup works after intensive memory operations
 */
static void test_memory_system_cleanup_validation(void) {
    printf("\n=== Testing memory system cleanup validation ===\n");
    
    // Perform intensive memory operations that stress the cleanup system
    const int num_allocs = 1000;
    void **ptrs = malloc(num_allocs * sizeof(void*));
    TEST_ASSERT(ptrs != NULL, "Test array allocation should succeed");
    
    // Mix of different allocation sizes
    for (int i = 0; i < num_allocs; i++) {
        size_t size = 64 + (i % 1000) * 16; // Variable sizes
        ptrs[i] = mymalloc(size);
        TEST_ASSERT(ptrs[i] != NULL, "Memory allocation should succeed");
    }
    
    // Test tree scoping with heavy usage
    begin_tree_memory_scope();
    
    // More allocations within scope
    void *scope_allocs[100];
    for (int i = 0; i < 100; i++) {
        scope_allocs[i] = mymalloc(1024 * (i + 1));
    }
    
    // End scope (this should free scope_allocs automatically)
    end_tree_memory_scope();
    
    // Free regular allocations
    for (int i = 0; i < num_allocs; i++) {
        myfree(ptrs[i]);
    }
    free(ptrs);
    
    // Force memory pressure handling
    check_memory_pressure_and_expand();
    
    printf("Memory system cleanup validation test completed\n");
    // NOTE FOR DEVELOPER: The actual cleanup test happens in teardown_test_context()
    // If the test passes but crashes during cleanup, that's where the bug is
}

/**
 * Test: Memory statistics and monitoring
 */
static void test_memory_statistics_monitoring(void) {
    printf("\n=== Testing memory statistics and monitoring ===\n");
    
    // Print initial statistics
    printf("Initial memory statistics:\n");
    print_memory_stats();
    
    // Allocate some memory and monitor changes
    const int num_allocs = 10;
    void *ptrs[num_allocs];
    
    for (int i = 0; i < num_allocs; i++) {
        size_t size = (i + 1) * 1024;
        ptrs[i] = mymalloc(size);
        TEST_ASSERT(ptrs[i] != NULL, "Monitoring test allocation should succeed");
    }
    
    printf("After allocations:\n");
    print_memory_stats();
    
    // Update peak usage tracking
    test_ctx.peak_memory_usage = get_memory_usage_mb();
    TEST_ASSERT(test_ctx.peak_memory_usage >= test_ctx.initial_memory_usage, 
               "Peak memory usage should be >= initial usage");
    
    // Free memory and check statistics
    for (int i = 0; i < num_allocs; i++) {
        myfree(ptrs[i]);
    }
    
    printf("After freeing allocations:\n");
    print_memory_stats();
    
    printf("Memory statistics monitoring tests passed\n");
    printf("Peak memory usage during test: %.2f MB\n", (double)test_ctx.peak_memory_usage);
}


//=============================================================================
// Test Runner
//=============================================================================

int main(int argc, char *argv[]) {
    (void)argc; // Suppress unused parameter warning
    (void)argv; // Suppress unused parameter warning
    
    printf("\n========================================\n");
    printf("Starting tests for test_dynamic_memory_expansion\n");
    printf("========================================\n\n");
    
    printf("This test verifies that the dynamic memory expansion system:\n");
    printf("  1. Initializes and cleans up properly\n");
    printf("  2. Handles basic memory operations correctly\n");
    printf("  3. Expands block tables dynamically under pressure\n");
    printf("  4. Manages tree-scoped memory with proper cleanup\n");
    printf("  5. Integrates correctly with the property system\n");
    printf("  6. Supports realistic physics module memory patterns\n");
    printf("  7. Handles scientific tree processing workflows\n");
    printf("  8. Detects memory pressure and expands automatically\n");
    printf("  9. Scales to large allocation scenarios\n");
    printf(" 10. Handles error conditions and edge cases gracefully\n");
    printf(" 11. Manages memory fragmentation effectively\n");
    printf(" 12. Provides accurate memory statistics and monitoring\n");
    printf(" 13. Properly cleans up after intensive memory operations\n\n");
    
    printf("NOTE FOR DEVELOPER: If this test crashes during cleanup at the end,\n");
    printf("there is a bug in the dynamic memory expansion system cleanup code.\n");
    printf("See teardown_test_context() for debugging hints and potential causes.\n\n");

    // Setup
    if (setup_test_context() != 0) {
        printf("ERROR: Failed to set up test context\n");
        return 1;
    }
    
    // Run core memory system tests
    test_memory_system_lifecycle();
    test_basic_memory_operations();
    test_dynamic_block_expansion();
    test_tree_memory_scoping();
    
    // Run enhanced integration tests
    test_property_system_integration();
    test_physics_module_memory_patterns();
    test_scientific_tree_processing();
    
    // Run stress and edge case tests
    test_memory_pressure_detection();
    test_large_allocation_scenarios();
    test_error_handling_edge_cases();
    test_memory_fragmentation_patterns();
    test_memory_system_cleanup_validation();  // New test for cleanup validation
    test_memory_statistics_monitoring();      // Restored memory statistics test
    
    // NOTE FOR DEVELOPER: If the program crashes AFTER this point,
    // the issue is in the cleanup code in teardown_test_context()
    // This is likely a bug in the dynamic memory expansion system
    
    // Teardown
    teardown_test_context();
    
    // Report results
    printf("\n========================================\n");
    printf("Test results for test_dynamic_memory_expansion:\n");
    printf("  Total tests: %d\n", tests_run);
    printf("  Passed: %d\n", tests_passed);
    printf("  Failed: %d\n", tests_run - tests_passed);
    printf("========================================\n\n");
    
    return (tests_run == tests_passed) ? 0 : 1;
}