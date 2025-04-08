#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Don't redefine TESTING_STANDALONE if already defined by compiler */
#ifndef TESTING_STANDALONE
#define TESTING_STANDALONE
#endif

#include "../src/core/core_allvars.h"

/**
 * Test program for memory mapping parameter in core_allvars.h
 */

int main(int argc, char **argv) {
    (void)argc; // Suppress unused parameter warning
    (void)argv; // Suppress unused parameter warning
    printf("Testing memory mapping parameter in core_allvars.h\n");
    
    // Create a params structure
    struct params params;
    memset(&params, 0, sizeof(struct params));
    
    // Manually set and test the EnableMemoryMapping parameter
    params.runtime.EnableMemoryMapping = 1;
    
    if (params.runtime.EnableMemoryMapping != 1) {
        fprintf(stderr, "Failed: EnableMemoryMapping parameter not properly set\n");
        return 1;
    }
    
    printf("Success: EnableMemoryMapping parameter is present and can be set\n");
    return 0;
}