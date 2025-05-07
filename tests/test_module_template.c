/**
 * @file test_module_template.c
 * @brief Test script for module template generator
 * @author SAGE refactoring team
 * @date 2025-05-07
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/core/core_module_template.h"
#include "../src/core/core_logging.h"
#include "../src/core/core_parameter_views.h"

int main(int argc, char **argv) {
    (void)argc; // Unused
    (void)argv; // Unused
    
    // Define minimal runtime params structure for logging
    struct params runtime_params;
    memset(&runtime_params, 0, sizeof(struct params));
    
    // Initialize logging with runtime params
    printf("Initializing logging...\n");
    initialize_logging(&runtime_params);
    
    // Create template parameters
    struct module_template_params template_params;
    module_template_params_init(&template_params);
    
    // Set parameters for testing
    strcpy(template_params.module_name, "test_cooling_module");
    strcpy(template_params.module_prefix, "test_cooling");
    template_params.type = MODULE_TYPE_COOLING;
    strcpy(template_params.author, "SAGE Testing Team");
    strcpy(template_params.email, "sage-test@example.com");
    strcpy(template_params.description, "Test cooling module using GALAXY_PROP_* macros");
    strcpy(template_params.version, "1.0.0");
    
    // Include all template features
    template_params.include_galaxy_extension = true;
    template_params.include_event_handler = true;
    template_params.include_callback_registration = true;
    template_params.include_manifest = true;
    template_params.include_makefile = true;
    template_params.include_test_file = true;
    template_params.include_readme = true;
    
    // Set output directory
    strcpy(template_params.output_dir, "/tmp/test_module_template");
    
    // Generate the template
    int result = module_generate_template(&template_params);
    if (result != 0) {
        LOG_ERROR("Failed to generate module template");
        return 1;
    }
    
    LOG_INFO("Successfully generated module template in %s", template_params.output_dir);
    LOG_INFO("Please verify that the generated files include correct GALAXY_PROP_* usage.");
    
    return 0;
}