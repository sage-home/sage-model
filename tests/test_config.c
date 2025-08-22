#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "config/config.h"

// Helper function to create a temporary file and return its name
// Returns file descriptor that should be closed by caller
static int create_temp_file(char *filename_buffer, size_t buffer_size) {
    snprintf(filename_buffer, buffer_size, "/tmp/sage_test_config_XXXXXX");
    int fd = mkstemp(filename_buffer);
    if (fd == -1) {
        fprintf(stderr, "Error: Could not create temporary file\n");
        assert(0);
    }
    return fd;
}

// Test configuration creation and destruction
static int test_config_creation() {
    printf("  Testing configuration creation...\n");
    
    config_t *config = config_create();
    assert(config != NULL);
    assert(config->format == CONFIG_FORMAT_UNKNOWN);
    assert(config->params == NULL);
    assert(config->is_validated == false);
    assert(config->owns_params == false);
    
    config_destroy(config);
    printf("    SUCCESS: Configuration creation works\n");
    return 0;
}

// Test format detection
static int test_format_detection() {
    printf("  Testing format detection...\n");
    
    assert(config_detect_format("millennium.par") == CONFIG_FORMAT_LEGACY_PAR);
    assert(config_detect_format("config.json") == CONFIG_FORMAT_JSON);
    assert(config_detect_format("unknown.txt") == CONFIG_FORMAT_LEGACY_PAR);  // Default
    assert(config_detect_format("test") == CONFIG_FORMAT_LEGACY_PAR);         // No extension
    assert(config_detect_format(NULL) == CONFIG_FORMAT_UNKNOWN);              // NULL input
    
    printf("    SUCCESS: Format detection works\n");
    return 0;
}

// Create a test .par file
void create_test_par_file(const char *filename) {
    FILE *fp = fopen(filename, "w");
    assert(fp != NULL);
    
    fprintf(fp, "%% Test parameter file for configuration system\n");
    fprintf(fp, "BoxSize                     62.5\n");
    fprintf(fp, "Omega                       0.25\n");
    fprintf(fp, "OmegaLambda                 0.75\n");
    fprintf(fp, "BaryonFrac                  0.17\n");
    fprintf(fp, "Hubble_h                    0.73\n");
    fprintf(fp, "PartMass                    0.0860657\n");
    fprintf(fp, "FirstFile                   0\n");
    fprintf(fp, "LastFile                    7\n");
    fprintf(fp, "NumSimulationTreeFiles      8\n");
    fprintf(fp, "OutputDir                   ./output/\n");
    fprintf(fp, "SimulationDir               ./input/data/millennium/\n");
    fprintf(fp, "TreeName                    trees_063\n");
    fprintf(fp, "TreeType                    lhalo_binary\n");
    fprintf(fp, "OutputFormat                sage_hdf5\n");
    fprintf(fp, "ForestDistributionScheme    uniform_in_forests\n");
    fprintf(fp, "SFprescription              0\n");
    fprintf(fp, "AGNrecipeOn                 2\n");
    fprintf(fp, "SupernovaRecipeOn           1\n");
    fprintf(fp, "ReionizationOn              1\n");
    fprintf(fp, "DiskInstabilityOn           1\n");
    fprintf(fp, "SfrEfficiency               0.01\n");
    fprintf(fp, "FeedbackReheatingEpsilon    3.0\n");
    fprintf(fp, "FeedbackEjectionEfficiency  0.3\n");
    fprintf(fp, "LastSnapshotNr              63\n");
    fprintf(fp, "NumOutputs                  -1\n");
    fprintf(fp, "FileNameGalaxies            model\n");
    fprintf(fp, "FileWithSnapList            input/desired_outputsnaps.txt\n");
    fprintf(fp, "RecycleFraction             0.43\n");
    fprintf(fp, "Yield                       0.025\n");
    fprintf(fp, "FracZleaveDisk              0.25\n");
    fprintf(fp, "ReIncorporationFactor       1.5e10\n");
    fprintf(fp, "ThreshMajorMerger           0.3\n");
    fprintf(fp, "ThresholdSatDisruption      1.0\n");
    fprintf(fp, "Reionization_z0             8.0\n");
    fprintf(fp, "Reionization_zr             7.0\n");
    fprintf(fp, "EnergySN                    1.0e51\n");
    fprintf(fp, "EtaSN                       5.0e-3\n");
    fprintf(fp, "RadioModeEfficiency         0.08\n");
    fprintf(fp, "QuasarModeEfficiency        0.001\n");
    fprintf(fp, "BlackHoleGrowthRate         0.015\n");
    fprintf(fp, "UnitLength_in_cm            3.085678e24\n");
    fprintf(fp, "UnitVelocity_in_cm_per_s    1.0e5\n");
    fprintf(fp, "UnitMass_in_g               1.989e43\n");
    fprintf(fp, "ExponentForestDistributionScheme  0.0\n");
    
    fclose(fp);
}

// Test legacy .par file reading
static int test_legacy_par_reading() {
    printf("  Testing legacy .par file reading...\n");
    
    char test_file[256];
    int fd = create_temp_file(test_file, sizeof(test_file));
    close(fd);  // Close fd so create_test_par_file can open it
    
    create_test_par_file(test_file);
    
    config_t *config = config_create();
    assert(config != NULL);
    
    int result = config_read_file(config, test_file);
    assert(result == CONFIG_SUCCESS);
    assert(config->format == CONFIG_FORMAT_LEGACY_PAR);
    assert(config->params != NULL);
    assert(strcmp(config->source_file, test_file) == 0);
    
    // Verify some key parameters
    assert(config->params->BoxSize == 62.5);
    assert(config->params->FirstFile == 0);
    assert(config->params->LastFile == 7);
    assert(config->params->Omega == 0.25);
    assert(config->params->OmegaLambda == 0.75);
    assert(config->params->Hubble_h == 0.73);
    assert(config->params->SFprescription == 0);
    assert(config->params->AGNrecipeOn == 2);
    assert(strcmp(config->params->OutputDir, "./output/") == 0);
    
    config_destroy(config);
    unlink(test_file);  // Clean up test file
    
    printf("    SUCCESS: Legacy .par file reading works\n");
    return 0;
}

// Create a test invalid .par file
void create_invalid_par_file(const char *filename) {
    FILE *fp = fopen(filename, "w");
    assert(fp != NULL);
    
    fprintf(fp, "%% Invalid parameter file for testing validation\n");
    fprintf(fp, "BoxSize                     -10.0\n");    // Invalid: negative
    fprintf(fp, "Omega                       2.0\n");      // Invalid: > 1.0
    fprintf(fp, "OmegaLambda                 0.75\n");
    fprintf(fp, "BaryonFrac                  0.17\n");
    fprintf(fp, "Hubble_h                    0.73\n");
    fprintf(fp, "PartMass                    0.0860657\n");
    fprintf(fp, "FirstFile                   5\n");        // Invalid: > LastFile
    fprintf(fp, "LastFile                    3\n");        // Invalid: < FirstFile
    fprintf(fp, "NumSimulationTreeFiles      8\n");
    fprintf(fp, "OutputDir                   ./output/\n");  // Valid, not empty
    fprintf(fp, "SimulationDir               ./input/data/millennium/\n");
    fprintf(fp, "TreeName                    trees_063\n");
    fprintf(fp, "TreeType                    lhalo_binary\n");
    fprintf(fp, "OutputFormat                sage_hdf5\n");
    fprintf(fp, "ForestDistributionScheme    uniform_in_forests\n");
    fprintf(fp, "SFprescription              99\n");       // Invalid: out of range
    fprintf(fp, "AGNrecipeOn                 2\n");
    fprintf(fp, "SupernovaRecipeOn           1\n");
    fprintf(fp, "ReionizationOn              1\n");
    fprintf(fp, "DiskInstabilityOn           1\n");
    fprintf(fp, "SfrEfficiency               0.01\n");
    fprintf(fp, "FeedbackReheatingEpsilon    3.0\n");
    fprintf(fp, "FeedbackEjectionEfficiency  0.3\n");
    fprintf(fp, "LastSnapshotNr              63\n");
    fprintf(fp, "NumOutputs                  -1\n");
    fprintf(fp, "FileNameGalaxies            model\n");
    fprintf(fp, "FileWithSnapList            input/desired_outputsnaps.txt\n");
    fprintf(fp, "RecycleFraction             0.43\n");
    fprintf(fp, "Yield                       0.025\n");
    fprintf(fp, "FracZleaveDisk              0.25\n");
    fprintf(fp, "ReIncorporationFactor       1.5e10\n");
    fprintf(fp, "ThreshMajorMerger           0.3\n");
    fprintf(fp, "ThresholdSatDisruption      1.0\n");
    fprintf(fp, "Reionization_z0             8.0\n");
    fprintf(fp, "Reionization_zr             7.0\n");
    fprintf(fp, "EnergySN                    1.0e51\n");
    fprintf(fp, "EtaSN                       5.0e-3\n");
    fprintf(fp, "RadioModeEfficiency         0.08\n");
    fprintf(fp, "QuasarModeEfficiency        0.001\n");
    fprintf(fp, "BlackHoleGrowthRate         0.015\n");
    fprintf(fp, "UnitLength_in_cm            3.085678e24\n");
    fprintf(fp, "UnitVelocity_in_cm_per_s    1.0e5\n");
    fprintf(fp, "UnitMass_in_g               1.989e43\n");
    fprintf(fp, "ExponentForestDistributionScheme  0.0\n");
    
    fclose(fp);
}

// Test configuration validation
static int test_configuration_validation() {
    printf("  Testing configuration validation...\n");
    
    // Test valid configuration
    char valid_file[256];
    int fd = create_temp_file(valid_file, sizeof(valid_file));
    close(fd);  // Close fd so create_test_par_file can open it
    
    create_test_par_file(valid_file);
    
    config_t *config = config_create();
    assert(config != NULL);
    
    int result = config_read_file(config, valid_file);
    assert(result == CONFIG_SUCCESS);
    
    result = config_validate(config);
    assert(result == CONFIG_SUCCESS);
    assert(config->is_validated == true);
    
    config_destroy(config);
    unlink(valid_file);
    
    // Test invalid configuration
    char invalid_file[256];
    fd = create_temp_file(invalid_file, sizeof(invalid_file));
    close(fd);  // Close fd so create_invalid_par_file can open it
    
    create_invalid_par_file(invalid_file);
    
    config = config_create();
    assert(config != NULL);
    
    result = config_read_file(config, invalid_file);
    assert(result == CONFIG_SUCCESS);  // Reading should succeed
    
    result = config_validate(config);
    assert(result == CONFIG_ERROR_VALIDATION);  // Validation should fail
    assert(config->is_validated == false);
    assert(strlen(config->last_error) > 0);  // Should have error messages
    
    config_destroy(config);
    unlink(invalid_file);
    
    printf("    SUCCESS: Configuration validation works\n");
    return 0;
}

#ifdef CONFIG_JSON_SUPPORT
// Create a test JSON file
void create_test_json_file(const char *filename) {
    FILE *fp = fopen(filename, "w");
    assert(fp != NULL);
    
    fprintf(fp, "{\n");
    fprintf(fp, "  \"simulation\": {\n");
    fprintf(fp, "    \"boxSize\": 62.5,\n");
    fprintf(fp, "    \"omega\": 0.25,\n");
    fprintf(fp, "    \"omegaLambda\": 0.75,\n");
    fprintf(fp, "    \"baryonFrac\": 0.17,\n");
    fprintf(fp, "    \"hubble_h\": 0.73,\n");
    fprintf(fp, "    \"partMass\": 0.0860657\n");
    fprintf(fp, "  },\n");
    fprintf(fp, "  \"io\": {\n");
    fprintf(fp, "    \"treeDir\": \"./input/data/millennium/\",\n");
    fprintf(fp, "    \"treeName\": \"trees_063\",\n");
    fprintf(fp, "    \"treeType\": \"lhalo_binary\",\n");
    fprintf(fp, "    \"outputDir\": \"./output/\",\n");
    fprintf(fp, "    \"outputFormat\": \"sage_hdf5\",\n");
    fprintf(fp, "    \"firstFile\": 0,\n");
    fprintf(fp, "    \"lastFile\": 7,\n");
    fprintf(fp, "    \"numSimulationTreeFiles\": 8,\n");
    fprintf(fp, "    \"forestDistributionScheme\": \"uniform_in_forests\",\n");
    fprintf(fp, "    \"fileNameGalaxies\": \"model\"\n");
    fprintf(fp, "  },\n");
    fprintf(fp, "  \"physics\": {\n");
    fprintf(fp, "    \"sfPrescription\": 0,\n");
    fprintf(fp, "    \"agnRecipeOn\": 2,\n");
    fprintf(fp, "    \"supernovaRecipeOn\": 1,\n");
    fprintf(fp, "    \"reionizationOn\": 1,\n");
    fprintf(fp, "    \"diskInstabilityOn\": 1,\n");
    fprintf(fp, "    \"sfrEfficiency\": 0.01,\n");
    fprintf(fp, "    \"feedbackReheatingEpsilon\": 3.0,\n");
    fprintf(fp, "    \"feedbackEjectionEfficiency\": 0.3\n");
    fprintf(fp, "  },\n");
    fprintf(fp, "  \"snapshots\": {\n");
    fprintf(fp, "    \"lastSnapshotNr\": 63,\n");
    fprintf(fp, "    \"numOutputs\": -1\n");
    fprintf(fp, "  }\n");
    fprintf(fp, "}\n");
    
    fclose(fp);
}

// Test JSON configuration reading
static int test_json_configuration() {
    printf("  Testing JSON configuration reading...\n");
    
    char test_file[256];
    int fd = create_temp_file(test_file, sizeof(test_file));
    close(fd);  // Close fd so create_test_json_file can open it
    
    create_test_json_file(test_file);
    
    config_t *config = config_create();
    assert(config != NULL);
    
    int result = config_read_file(config, test_file);
    assert(result == CONFIG_SUCCESS);
    assert(config->format == CONFIG_FORMAT_JSON);
    assert(config->params != NULL);
    assert(strcmp(config->source_file, test_file) == 0);
    
    // Verify same parameters from JSON as .par
    assert(config->params->BoxSize == 62.5);
    assert(config->params->FirstFile == 0);
    assert(config->params->LastFile == 7);
    assert(config->params->Omega == 0.25);
    assert(config->params->OmegaLambda == 0.75);
    assert(config->params->Hubble_h == 0.73);
    assert(config->params->SFprescription == 0);
    assert(config->params->AGNrecipeOn == 2);
    assert(strcmp(config->params->OutputDir, "./output/") == 0);
    
    config_destroy(config);
    unlink(test_file);
    
    printf("    SUCCESS: JSON configuration reading works\n");
    return 0;
}
#endif

// Test error handling
static int test_error_handling() {
    printf("  Testing error handling...\n");
    
    config_t *config = config_create();
    assert(config != NULL);
    
    // Test reading non-existent file
    int result = config_read_file(config, "non_existent_file.par");
    assert(result != CONFIG_SUCCESS);
    assert(strlen(config->last_error) > 0);
    
    // Test validation without reading
    result = config_validate(config);
    assert(result == CONFIG_ERROR_INVALID_STATE);
    
    const char *error = config_get_last_error(config);
    assert(error != NULL);
    assert(strlen(error) > 0);
    
    config_destroy(config);
    
    printf("    SUCCESS: Error handling works\n");
    return 0;
}

// Test utility functions
static int test_utility_functions() {
    printf("  Testing utility functions...\n");
    
    // Test format to string conversion
    assert(strcmp(config_format_to_string(CONFIG_FORMAT_UNKNOWN), "unknown") == 0);
    assert(strcmp(config_format_to_string(CONFIG_FORMAT_JSON), "json") == 0);
    assert(strcmp(config_format_to_string(CONFIG_FORMAT_LEGACY_PAR), "legacy_par") == 0);
    
    // Test error to string conversion
    assert(strcmp(config_error_to_string(CONFIG_SUCCESS), "success") == 0);
    assert(strcmp(config_error_to_string(CONFIG_ERROR_MEMORY), "memory_allocation_failed") == 0);
    assert(strcmp(config_error_to_string(CONFIG_ERROR_PARSE), "parse_error") == 0);
    
    printf("    SUCCESS: Utility functions work\n");
    return 0;
}

//=============================================================================
// Test Runner
//=============================================================================

int main(int argc, char *argv[]) {
    (void)argc; (void)argv; // Suppress unused parameter warnings
    printf("\nRunning %s...\n", __FILE__);
    printf("\n=== Testing Configuration System ===\n");
    
    printf("This test verifies:\n");
    printf("  1. Configuration creation and cleanup\n");
    printf("  2. Format detection operates correctly\n");
    printf("  3. Legacy .par file reading works\n");
    printf("  4. Configuration validation functions properly\n");
    printf("  5. Error conditions are handled correctly\n");
    printf("  6. Utility functions work as expected\n");
#ifdef CONFIG_JSON_SUPPORT
    printf("  7. JSON configuration reading works\n");
#endif
    printf("\n");

    int result = 0;
    int test_count = 0;
    int passed_count = 0;
    
    // Run tests
    test_count++;
    if (test_config_creation() == 0) {
        passed_count++;
    } else {
        result = 1;
    }
    
    test_count++;
    if (test_format_detection() == 0) {
        passed_count++;
    } else {
        result = 1;
    }
    
    test_count++;
    if (test_legacy_par_reading() == 0) {
        passed_count++;
    } else {
        result = 1;
    }
    
    test_count++;
    if (test_configuration_validation() == 0) {
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
    if (test_utility_functions() == 0) {
        passed_count++;
    } else {
        result = 1;
    }
    
#ifdef CONFIG_JSON_SUPPORT
    test_count++;
    if (test_json_configuration() == 0) {
        passed_count++;
    } else {
        result = 1;
    }
    printf("\n✓ JSON configuration support is enabled\n");
#else
    printf("\n⚠ JSON configuration support is disabled (cJSON not found)\n");
#endif
    
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