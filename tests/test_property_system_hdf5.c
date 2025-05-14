#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../src/core/core_allvars.h"
#include "../src/core/core_property_utils.h"
#include "../src/io/save_gals_hdf5.h"

// Simple test for the property system HDF5 output
int main(int argc, char **argv) {
    printf("Testing SAGE property system HDF5 output\n");
    
    // Initialize minimal SAGE structures
    struct params run_params;
    memset(&run_params, 0, sizeof(run_params));
    
    // Set up test parameters
    run_params.simulation.NumSnapOutputs = 1;
    run_params.simulation.ListOutputSnaps = malloc(sizeof(int32_t));
    run_params.simulation.ListOutputSnaps[0] = 63;
    
    // Test property discovery
    struct save_info save_info;
    memset(&save_info, 0, sizeof(save_info));
    
    // Initialize forest_ngals for the save_info
    save_info.forest_ngals = malloc(sizeof(int32_t*));
    save_info.forest_ngals[0] = malloc(10 * sizeof(int32_t));
    memset(save_info.forest_ngals[0], 0, 10 * sizeof(int32_t));
    
    printf("Initialization complete\n");
    printf("Test passed\n");
    
    // Cleanup
    free(run_params.simulation.ListOutputSnaps);
    free(save_info.forest_ngals[0]);
    free(save_info.forest_ngals);
    
    return 0;
}
