#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>  // for off_t

/* Create a minimal GALAXY struct for testing without including core_allvars.h */
struct GALAXY {
    int Type;
    int64_t GalaxyIndex;
    /* Add only the fields we actually use in tests */
};

#include "../src/core/core_mymalloc.h"
#include "../src/core/core_array_utils.h"

/**
 * @file test_array_utils.c
 * @brief Tests for the array utility functions
 */

// Test array expansion with different growth factors
static int test_array_expansion() {
    printf("Testing array expansion with different growth factors...\n");
    
    // Allocate initial array directly with mymalloc
    int capacity = 10;
    int *array = mymalloc(capacity * sizeof(int));
    if (!array) {
        fprintf(stderr, "Failed initial allocation\n");
        return -1;
    }
    
    // Fill array with values
    for (int i = 0; i < capacity; i++) {
        array[i] = i;
    }
    
    printf("Initial capacity: %d\n", capacity);
    assert(capacity >= 10);
    
    // Expand to larger size
    int old_capacity = capacity;
    if (array_expand((void **)&array, sizeof(int), &capacity, capacity * 2, 1.5f) != 0) {
        fprintf(stderr, "Failed expansion\n");
        myfree(array);
        return -1;
    }
    
    printf("Expanded capacity: %d (from %d)\n", capacity, old_capacity);
    assert(capacity >= old_capacity * 2);
    
    // Verify values are preserved
    for (int i = 0; i < old_capacity; i++) {
        assert(array[i] == i);
    }
    
    myfree(array);
    printf("Test passed.\n");
    return 0;
}

// Test default expansion
static int test_default_expansion() {
    printf("Testing default array expansion...\n");
    
    // Allocate initial array directly with mymalloc
    int capacity = 20;
    float *array = mymalloc(capacity * sizeof(float));
    if (!array) {
        fprintf(stderr, "Failed initial allocation\n");
        return -1;
    }
    
    // Fill array with values
    for (int i = 0; i < capacity; i++) {
        array[i] = (float)i * 1.5f;
    }
    
    printf("Initial capacity: %d\n", capacity);
    assert(capacity >= 20);
    
    // Expand to larger size
    int old_capacity = capacity;
    if (array_expand_default((void **)&array, sizeof(float), &capacity, capacity * 3) != 0) {
        fprintf(stderr, "Failed expansion\n");
        myfree(array);
        return -1;
    }
    
    printf("Expanded capacity: %d (from %d)\n", capacity, old_capacity);
    assert(capacity >= old_capacity * 3);
    
    // Verify values are preserved
    for (int i = 0; i < old_capacity; i++) {
        assert(array[i] == (float)i * 1.5f);
    }
    
    myfree(array);
    printf("Test passed.\n");
    return 0;
}

// Test galaxy array expansion
static int test_galaxy_array_expansion() {
    printf("Testing galaxy array expansion...\n");
    
    // Allocate initial array directly with mymalloc
    int capacity = 30;
    struct GALAXY *galaxies = mymalloc(capacity * sizeof(struct GALAXY));
    if (!galaxies) {
        fprintf(stderr, "Failed initial allocation\n");
        return -1;
    }
    
    // Initialize galaxies with test values
    for (int i = 0; i < capacity; i++) {
        galaxies[i].Type = i % 3;
        galaxies[i].GalaxyIndex = (int64_t)(i + 1000);
    }
    
    printf("Initial capacity: %d\n", capacity);
    assert(capacity >= 30);
    
    // Expand to larger size
    int old_capacity = capacity;
    if (galaxy_array_expand(&galaxies, &capacity, capacity * 2) != 0) {
        fprintf(stderr, "Failed expansion\n");
        myfree(galaxies);
        return -1;
    }
    
    printf("Expanded capacity: %d (from %d)\n", capacity, old_capacity);
    assert(capacity >= old_capacity * 2);
    
    // Verify values are preserved
    for (int i = 0; i < old_capacity; i++) {
        assert(galaxies[i].Type == i % 3);
        assert(galaxies[i].GalaxyIndex == (int64_t)(i + 1000));
    }
    
    myfree(galaxies);
    printf("Test passed.\n");
    return 0;
}

// Test multiple small expansions
static int test_multiple_expansions() {
    printf("Testing multiple small expansions...\n");
    
    int capacity = 10;
    int *array = mymalloc(capacity * sizeof(int));
    if (!array) {
        fprintf(stderr, "Failed initial allocation\n");
        return -1;
    }
    
    int num_expansions = 0;
    
    // Repeatedly expand by small increments
    for (int target_size = 10; target_size <= 10000; target_size *= 2) {
        if (array_expand_default((void **)&array, sizeof(int), &capacity, target_size) != 0) {
            fprintf(stderr, "Failed expansion to size %d\n", target_size);
            myfree(array);
            return -1;
        }
        
        num_expansions++;
        assert(capacity >= target_size);
    }
    
    printf("Performed %d expansions to reach capacity %d\n", num_expansions, capacity);
    // With geometric growth, we should have far fewer than linear expansions
    assert(num_expansions < 20);
    
    myfree(array);
    printf("Test passed.\n");
    return 0;
}

int main() {
    printf("=== Array Utilities Tests ===\n\n");
    
    int status = 0;
    
    if (test_array_expansion() != 0) {
        status = 1;
    }
    
    if (test_default_expansion() != 0) {
        status = 1;
    }
    
    if (test_galaxy_array_expansion() != 0) {
        status = 1;
    }
    
    if (test_multiple_expansions() != 0) {
        status = 1;
    }
    
    if (status == 0) {
        printf("\nAll tests passed!\n");
    } else {
        printf("\nSome tests failed.\n");
    }
    
    return status;
}
