#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/core/core_allvars.h"
#include "../src/core/core_module_system.h"
#include "../src/core/core_galaxy_extensions.h"
#include "../src/physics/example_galaxy_extension.h"

/* Test module ID */
#define TEST_MODULE_ID 99

/* Function prototypes */
void test_extension_registration(void);
void test_extension_memory_management(void);
void test_extension_access(void);
void test_extension_copy(void);

int main(void) {
    printf("Testing Galaxy Extension Mechanism\n");
    
    /* Initialize module and extension systems */
    module_system_initialize();
    galaxy_extension_system_initialize();
    
    /* Run tests */
    test_extension_registration();
    test_extension_memory_management();
    test_extension_access();
    test_extension_copy();
    
    /* Clean up */
    galaxy_extension_system_cleanup();
    module_system_cleanup();
    
    printf("All tests passed!\n");
    return 0;
}

void test_extension_registration(void) {
    printf("Testing extension registration...\n");
    
    /* Register example extension */
    int extension_id = initialize_example_extension(TEST_MODULE_ID);
    if (extension_id < 0) {
        fprintf(stderr, "Failed to register example extension\n");
        exit(1);
    }
    
    /* Verify property can be found */
    const galaxy_property_t *property = galaxy_extension_find_property("ExampleExtension");
    if (property == NULL) {
        fprintf(stderr, "Failed to find registered extension property\n");
        exit(1);
    }
    
    /* Verify property attributes */
    if (strcmp(property->name, "ExampleExtension") != 0) {
        fprintf(stderr, "Property name mismatch\n");
        exit(1);
    }
    
    if (property->module_id != TEST_MODULE_ID) {
        fprintf(stderr, "Property module ID mismatch\n");
        exit(1);
    }
    
    if (property->size != sizeof(example_extension_data_t)) {
        fprintf(stderr, "Property size mismatch\n");
        exit(1);
    }
    
    printf("Extension registration test passed!\n");
}

void test_extension_memory_management(void) {
    printf("Testing extension memory management...\n");
    
    /* Find extension ID */
    int extension_id = initialize_example_extension(TEST_MODULE_ID);
    if (extension_id < 0) {
        fprintf(stderr, "Failed to get example extension ID\n");
        exit(1);
    }
    
    /* Create a test galaxy */
    struct GALAXY galaxy;
    memset(&galaxy, 0, sizeof(galaxy));
    
    /* Initialize extension data */
    int status = galaxy_extension_initialize(&galaxy);
    if (status != 0) {
        fprintf(stderr, "Failed to initialize galaxy extension data\n");
        exit(1);
    }
    
    /* Verify extension data is initialized */
    if (galaxy.extension_data == NULL) {
        fprintf(stderr, "Galaxy extension data pointer is NULL\n");
        exit(1);
    }
    
    if (galaxy.num_extensions <= 0) {
        fprintf(stderr, "Galaxy extension count is invalid\n");
        exit(1);
    }
    
    /* Clean up extension data */
    status = galaxy_extension_cleanup(&galaxy);
    if (status != 0) {
        fprintf(stderr, "Failed to clean up galaxy extension data\n");
        exit(1);
    }
    
    /* Verify extension data is cleaned up */
    if (galaxy.extension_data != NULL) {
        fprintf(stderr, "Galaxy extension data pointer is not NULL after cleanup\n");
        exit(1);
    }
    
    printf("Extension memory management test passed!\n");
}

void test_extension_access(void) {
    printf("Testing extension access...\n");
    
    /* Find extension ID */
    int extension_id = initialize_example_extension(TEST_MODULE_ID);
    if (extension_id < 0) {
        fprintf(stderr, "Failed to get example extension ID\n");
        exit(1);
    }
    
    /* Create a test galaxy */
    struct GALAXY galaxy;
    memset(&galaxy, 0, sizeof(galaxy));
    galaxy.ColdGas = 1000.0;
    galaxy.MetalsColdGas = 100.0;
    galaxy.DiskScaleRadius = 10.0;
    galaxy.Vvir = 200.0;
    galaxy.GalaxyNr = 123;
    
    /* Initialize extension data */
    int status = galaxy_extension_initialize(&galaxy);
    if (status != 0) {
        fprintf(stderr, "Failed to initialize galaxy extension data\n");
        exit(1);
    }
    
    /* Access extension data - this should allocate memory */
    example_extension_data_t *ext_data = get_example_extension_data(&galaxy, extension_id);
    if (ext_data == NULL) {
        fprintf(stderr, "Failed to get example extension data\n");
        exit(1);
    }
    
    /* Verify extension data is accessible */
    if ((void *)ext_data != galaxy_extension_get_data(&galaxy, extension_id)) {
        fprintf(stderr, "Extension data pointers don't match\n");
        exit(1);
    }
    
    /* Test extension usage */
    demonstrate_extension_usage(&galaxy, extension_id);
    
    /* Verify extension data values */
    if (ext_data->h2_fraction <= 0.0 || ext_data->h2_fraction > 1.0) {
        fprintf(stderr, "Invalid h2_fraction value: %f\n", ext_data->h2_fraction);
        exit(1);
    }
    
    if (ext_data->pressure <= 0.0) {
        fprintf(stderr, "Invalid pressure value: %f\n", ext_data->pressure);
        exit(1);
    }
    
    if (ext_data->num_regions != 3) {
        fprintf(stderr, "Invalid number of regions: %d\n", ext_data->num_regions);
        exit(1);
    }
    
    /* Clean up extension data */
    status = galaxy_extension_cleanup(&galaxy);
    if (status != 0) {
        fprintf(stderr, "Failed to clean up galaxy extension data\n");
        exit(1);
    }
    
    printf("Extension access test passed!\n");
}

void test_extension_copy(void) {
    printf("Testing extension copy...\n");
    
    /* Find extension ID */
    int extension_id = initialize_example_extension(TEST_MODULE_ID);
    if (extension_id < 0) {
        fprintf(stderr, "Failed to get example extension ID\n");
        exit(1);
    }
    
    /* Create source galaxy */
    struct GALAXY src_galaxy;
    memset(&src_galaxy, 0, sizeof(src_galaxy));
    src_galaxy.ColdGas = 1000.0;
    src_galaxy.MetalsColdGas = 100.0;
    src_galaxy.DiskScaleRadius = 10.0;
    src_galaxy.Vvir = 200.0;
    src_galaxy.GalaxyNr = 123;
    
    /* Initialize extension data */
    int status = galaxy_extension_initialize(&src_galaxy);
    if (status != 0) {
        fprintf(stderr, "Failed to initialize source galaxy extension data\n");
        exit(1);
    }
    
    /* Access and populate extension data */
    example_extension_data_t *src_ext_data = get_example_extension_data(&src_galaxy, extension_id);
    if (src_ext_data == NULL) {
        fprintf(stderr, "Failed to get source example extension data\n");
        exit(1);
    }
    
    demonstrate_extension_usage(&src_galaxy, extension_id);
    
    /* Create destination galaxy */
    struct GALAXY dest_galaxy;
    memset(&dest_galaxy, 0, sizeof(dest_galaxy));
    
    /* Copy extension data */
    status = galaxy_extension_copy(&dest_galaxy, &src_galaxy);
    if (status != 0) {
        fprintf(stderr, "Failed to copy galaxy extension data\n");
        exit(1);
    }
    
    /* Access copied extension data */
    example_extension_data_t *dest_ext_data = get_example_extension_data(&dest_galaxy, extension_id);
    if (dest_ext_data == NULL) {
        fprintf(stderr, "Failed to get destination example extension data\n");
        exit(1);
    }
    
    /* Verify copied data */
    if (dest_ext_data->h2_fraction != src_ext_data->h2_fraction) {
        fprintf(stderr, "h2_fraction not copied correctly: src=%f, dest=%f\n", 
                src_ext_data->h2_fraction, dest_ext_data->h2_fraction);
        exit(1);
    }
    
    if (dest_ext_data->pressure != src_ext_data->pressure) {
        fprintf(stderr, "pressure not copied correctly: src=%f, dest=%f\n", 
                src_ext_data->pressure, dest_ext_data->pressure);
        exit(1);
    }
    
    if (dest_ext_data->num_regions != src_ext_data->num_regions) {
        fprintf(stderr, "num_regions not copied correctly: src=%d, dest=%d\n", 
                src_ext_data->num_regions, dest_ext_data->num_regions);
        exit(1);
    }
    
    for (int i = 0; i < src_ext_data->num_regions; i++) {
        if (dest_ext_data->regions[i].radius != src_ext_data->regions[i].radius) {
            fprintf(stderr, "region[%d].radius not copied correctly: src=%f, dest=%f\n", 
                    i, src_ext_data->regions[i].radius, dest_ext_data->regions[i].radius);
            exit(1);
        }
        
        if (dest_ext_data->regions[i].sfr != src_ext_data->regions[i].sfr) {
            fprintf(stderr, "region[%d].sfr not copied correctly: src=%f, dest=%f\n", 
                    i, src_ext_data->regions[i].sfr, dest_ext_data->regions[i].sfr);
            exit(1);
        }
    }
    
    /* Clean up extension data */
    status = galaxy_extension_cleanup(&src_galaxy);
    if (status != 0) {
        fprintf(stderr, "Failed to clean up source galaxy extension data\n");
        exit(1);
    }
    
    status = galaxy_extension_cleanup(&dest_galaxy);
    if (status != 0) {
        fprintf(stderr, "Failed to clean up destination galaxy extension data\n");
        exit(1);
    }
    
    printf("Extension copy test passed!\n");
}
