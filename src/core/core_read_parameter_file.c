#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h> /* for isblank()*/

#include "core_allvars.h"
#include "core_mymalloc.h"
#include "config/config.h"

enum datatypes {
    DOUBLE = 1,
    STRING = 2,
    INT = 3
};

#define MAXTAGS          300  /* Max number of parameters */
#define MAXTAGLEN         50  /* Max number of characters in the string param tags */

int compare_ints_descending (const void* p1, const void* p2);

int compare_ints_descending (const void* p1, const void* p2)
{
    int i1 = *(int*) p1;
    int i2 = *(int*) p2;
    if (i1 < i2) {
        return 1;
    } else if (i1 == i2) {
        return 0;
    } else {
        return -1;
    }
 }

int read_parameter_file(const char *fname, struct params *run_params)
{
    // Use new configuration system internally while maintaining API compatibility
    config_t *config = config_create();
    if (!config) {
        fprintf(stderr, "Error: Failed to create configuration object\n");
        return FILE_READ_ERROR;
    }
    
    int result = config_read_file(config, fname);
    if (result != CONFIG_SUCCESS) {
        fprintf(stderr, "Error reading parameter file '%s': %s\n", 
                fname, config_get_last_error(config));
        config_destroy(config);
        return result == CONFIG_ERROR_FILE_READ ? FILE_NOT_FOUND : PARSE_ERROR;
    }
    
    // Validate configuration if validation is enabled
#ifdef SAGE_CONFIG_VALIDATION
    result = config_validate(config);
    if (result != CONFIG_SUCCESS) {
        fprintf(stderr, "Configuration validation failed for '%s':\n", fname);
        config_print_validation_errors(config);
        // Continue anyway for backward compatibility - just warn
    }
#endif
    
    // Copy parameters to provided structure (maintaining API compatibility)
    if (config->params) {
        // Preserve existing ThisTask and NTasks values before copying
        int preserved_ThisTask = run_params->ThisTask;
        int32_t preserved_NTasks = run_params->NTasks;
        
        memcpy(run_params, config->params, sizeof(struct params));
        
        // Restore the preserved values
        run_params->ThisTask = preserved_ThisTask;
        run_params->NTasks = preserved_NTasks;
    } else {
        fprintf(stderr, "Error: No configuration data loaded from '%s'\n", fname);
        config_destroy(config);
        return PARSE_ERROR;
    }
    
    config_destroy(config);
    return 0;  // Maintain legacy return code (0 = success)
}

#undef MAXTAGS
#undef MAXTAGLEN
