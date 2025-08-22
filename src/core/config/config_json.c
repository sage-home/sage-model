#include "config_json.h"

#ifdef CONFIG_JSON_SUPPORT

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cjson/cJSON.h>

#include "config_legacy.h"
#include "../memory.h"
#include "../core_utils.h"

// JSON utility functions
double get_json_double(const void *json_obj, const char *key, double default_val) {
    const cJSON *json = (const cJSON *)json_obj;
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(json, key);
    
    if (cJSON_IsNumber(item)) {
        return cJSON_GetNumberValue(item);
    }
    
    return default_val;
}

int get_json_int(const void *json_obj, const char *key, int default_val) {
    const cJSON *json = (const cJSON *)json_obj;
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(json, key);
    
    if (cJSON_IsNumber(item)) {
        return (int)cJSON_GetNumberValue(item);
    }
    
    return default_val;
}

void get_json_string(const void *json_obj, const char *key, char *dest, size_t dest_size) {
    const cJSON *json = (const cJSON *)json_obj;
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(json, key);
    
    if (cJSON_IsString(item) && item->valuestring) {
        strncpy(dest, item->valuestring, dest_size - 1);
        dest[dest_size - 1] = '\0';
    } else {
        dest[0] = '\0';
    }
}

// JSON to params conversion using explicit mapping
int json_to_params(const void *json_root, struct params *params) {
    const cJSON *json = (const cJSON *)json_root;
    
    // Initialize params to defaults (zero/empty)
    memset(params, 0, sizeof(struct params));
    
    // Map simulation parameters
    const cJSON *simulation = cJSON_GetObjectItemCaseSensitive(json, "simulation");
    if (simulation) {
        params->BoxSize = get_json_double(simulation, "boxSize", 0.0);
        params->Omega = get_json_double(simulation, "omega", 0.25);
        params->OmegaLambda = get_json_double(simulation, "omegaLambda", 0.75);
        params->BaryonFrac = get_json_double(simulation, "baryonFrac", 0.17);
        params->Hubble_h = get_json_double(simulation, "hubble_h", 0.73);
        params->PartMass = get_json_double(simulation, "partMass", 0.0);
    }
    
    // Map I/O parameters
    const cJSON *io = cJSON_GetObjectItemCaseSensitive(json, "io");
    if (io) {
        get_json_string(io, "treeDir", params->SimulationDir, sizeof(params->SimulationDir));
        get_json_string(io, "treeName", params->TreeName, sizeof(params->TreeName));
        get_json_string(io, "outputDir", params->OutputDir, sizeof(params->OutputDir));
        get_json_string(io, "fileNameGalaxies", params->FileNameGalaxies, sizeof(params->FileNameGalaxies));
        
        params->FirstFile = get_json_int(io, "firstFile", 0);
        params->LastFile = get_json_int(io, "lastFile", 0);
        params->NumSimulationTreeFiles = get_json_int(io, "numSimulationTreeFiles", 1);
        
        // Convert string enums to enum values
        char tree_type_str[64];
        get_json_string(io, "treeType", tree_type_str, sizeof(tree_type_str));
        params->TreeType = string_to_tree_type(tree_type_str);
        
        char output_format_str[64];
        get_json_string(io, "outputFormat", output_format_str, sizeof(output_format_str));
        params->OutputFormat = string_to_output_format(output_format_str);
        
        char forest_dist_str[64];
        get_json_string(io, "forestDistributionScheme", forest_dist_str, sizeof(forest_dist_str));
        params->ForestDistributionScheme = string_to_forest_dist_scheme(forest_dist_str);
    }
    
    // Map physics parameters
    const cJSON *physics = cJSON_GetObjectItemCaseSensitive(json, "physics");
    if (physics) {
        params->SFprescription = get_json_int(physics, "sfPrescription", 0);
        params->AGNrecipeOn = get_json_int(physics, "agnRecipeOn", 2);
        params->SupernovaRecipeOn = get_json_int(physics, "supernovaRecipeOn", 1);
        params->ReionizationOn = get_json_int(physics, "reionizationOn", 1);
        params->DiskInstabilityOn = get_json_int(physics, "diskInstabilityOn", 1);
        
        // Physics efficiency parameters
        params->SfrEfficiency = get_json_double(physics, "sfrEfficiency", 0.01);
        params->FeedbackReheatingEpsilon = get_json_double(physics, "feedbackReheatingEpsilon", 3.0);
        params->FeedbackEjectionEfficiency = get_json_double(physics, "feedbackEjectionEfficiency", 0.3);
        params->RadioModeEfficiency = get_json_double(physics, "radioModeEfficiency", 0.08);
        params->QuasarModeEfficiency = get_json_double(physics, "quasarModeEfficiency", 0.001);
        params->BlackHoleGrowthRate = get_json_double(physics, "blackHoleGrowthRate", 0.015);
        
        // Additional physics parameters
        params->RecycleFraction = get_json_double(physics, "recycleFraction", 0.43);
        params->Yield = get_json_double(physics, "yield", 0.025);
        params->FracZleaveDisk = get_json_double(physics, "fracZleaveDisk", 0.25);
        params->ReIncorporationFactor = get_json_double(physics, "reIncorporationFactor", 1.5e10);
        params->ThreshMajorMerger = get_json_double(physics, "threshMajorMerger", 0.3);
        params->ThresholdSatDisruption = get_json_double(physics, "thresholdSatDisruption", 1.0);
        
        // Reionization parameters
        params->Reionization_z0 = get_json_double(physics, "reionization_z0", 8.0);
        params->Reionization_zr = get_json_double(physics, "reionization_zr", 7.0);
        
        // Supernova parameters
        params->EnergySN = get_json_double(physics, "energySN", 1.0e51);
        params->EtaSN = get_json_double(physics, "etaSN", 5.0e-3);
    }
    
    // Map units (if specified)
    const cJSON *units = cJSON_GetObjectItemCaseSensitive(json, "units");
    if (units) {
        params->UnitLength_in_cm = get_json_double(units, "length_in_cm", 3.085678e24);
        params->UnitVelocity_in_cm_per_s = get_json_double(units, "velocity_in_cm_per_s", 1.0e5);
        params->UnitMass_in_g = get_json_double(units, "mass_in_g", 1.989e43);
    } else {
        // Default unit values
        params->UnitLength_in_cm = 3.085678e24;     // 1 kpc in cm
        params->UnitVelocity_in_cm_per_s = 1.0e5;   // 1 km/s in cm/s  
        params->UnitMass_in_g = 1.989e43;           // 10^10 Msun in g
    }
    
    // Map snapshot parameters
    const cJSON *snapshots = cJSON_GetObjectItemCaseSensitive(json, "snapshots");
    if (snapshots) {
        params->LastSnapshotNr = get_json_int(snapshots, "lastSnapshotNr", 63);
        params->NumSnapOutputs = get_json_int(snapshots, "numOutputs", -1);
        
        get_json_string(snapshots, "fileWithSnapList", params->FileWithSnapList, sizeof(params->FileWithSnapList));
    } else {
        // Default snapshot values
        params->LastSnapshotNr = 63;
        params->NumSnapOutputs = -1;  // All snapshots
        params->FileWithSnapList[0] = '\0';
    }
    
    return CONFIG_SUCCESS;
}

// Main JSON configuration reading function
int config_read_json(config_t *config, const char *filename) {
    if (!config || !filename) {
        return CONFIG_ERROR_INVALID_STATE;
    }
    
    // Read file into string
    char *json_string = read_file_to_string(filename);
    if (!json_string) {
        snprintf(config->last_error, sizeof(config->last_error),
                "Could not read JSON file: %s", filename);
        return CONFIG_ERROR_FILE_READ;
    }
    
    // Parse JSON
    cJSON *json = cJSON_Parse(json_string);
    sage_free(json_string);
    
    if (!json) {
        const char *error_ptr = cJSON_GetErrorPtr();
        snprintf(config->last_error, sizeof(config->last_error),
                "JSON parse error at: %.50s", error_ptr ? error_ptr : "unknown location");
        return CONFIG_ERROR_PARSE;
    }
    
    // Allocate params structure
    config->params = sage_malloc(sizeof(struct params));
    if (!config->params) {
        snprintf(config->last_error, sizeof(config->last_error),
                "Failed to allocate memory for configuration parameters");
        cJSON_Delete(json);
        return CONFIG_ERROR_MEMORY;
    }
    config->owns_params = true;
    
    // Convert JSON to params structure using mapping convention
    int result = json_to_params(json, config->params);
    
    cJSON_Delete(json);
    
    if (result != CONFIG_SUCCESS) {
        sage_free(config->params);
        config->params = NULL;
        config->owns_params = false;
        return result;
    }
    
    config->format = CONFIG_FORMAT_JSON;
    strncpy(config->source_file, filename, sizeof(config->source_file) - 1);
    config->source_file[sizeof(config->source_file) - 1] = '\0';
    
    return CONFIG_SUCCESS;
}

#endif /* CONFIG_JSON_SUPPORT */