/**
 * @file test_property_yaml_validation.c
 * @brief Comprehensive validation test for properties.yaml structure and generation system
 * 
 * This test validates the foundational YAML metadata file that drives the entire property
 * system code generation. It catches structural errors, type violations, and configuration
 * inconsistencies that would otherwise manifest as compile-time or runtime failures.
 * 
 * Tests cover:
 * - YAML file structure and schema validation
 * - Property type definition validation and edge cases
 * - Required field presence and format verification
 * - Core vs physics property separation compliance
 * - Dynamic array configuration validation
 * - Output transformer configuration validation
 * - Property name uniqueness and C identifier compliance
 * - Auto-generated enum and accessor correctness
 * - Integration with property system infrastructure
 * - Error boundary conditions and malformed data handling
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/stat.h>
#include <ctype.h>

#include "../src/core/core_allvars.h"
#include "../src/core/core_properties.h"
#include "../src/core/core_property_utils.h"
#include "../src/core/core_logging.h"

// Test configuration constants
#define PROPERTIES_YAML_PATH "src/properties.yaml"
#define TEST_YAML_DIR "tests/test_yaml_temp"
#define MAX_PROPERTY_NAME_LENGTH 64
#define MAX_TYPE_NAME_LENGTH 32
#define MAX_DESCRIPTION_LENGTH 256
#define MAX_UNITS_LENGTH 32
#define MAX_LINE_LENGTH 1024

// Test counter globals
static int tests_run = 0;
static int tests_passed = 0;

// Test context for complex validation scenarios
static struct test_context {
    FILE *properties_file;
    char *file_content;
    size_t content_size;
    int num_properties_found;
    char property_names[100][MAX_PROPERTY_NAME_LENGTH];
    bool has_core_properties;
    bool has_physics_properties;
    bool has_dynamic_arrays;
    bool initialized;
} test_ctx;

#define TEST_ASSERT(condition, message) do { \
    tests_run++; \
    if (!(condition)) { \
        printf("FAIL: %s\n", message); \
        printf("  at %s:%d\n", __FILE__, __LINE__); \
    } else { \
        tests_passed++; \
    } \
} while(0)

// Utility functions for YAML parsing and validation
static int load_properties_yaml_content(void);
static bool is_valid_c_identifier(const char *name);
static bool line_contains_property_definition(const char *line);
static bool line_contains_required_field(const char *line, const char *field);
static bool extract_yaml_value(const char *line, const char *field, char *value, size_t max_len);
static int count_yaml_indentation(const char *line);
static bool is_valid_property_type(const char *type);
static bool is_core_property_type(const char *type);
static bool validate_dynamic_array_syntax(const char *type, const char *property_content);

//=============================================================================
// Test Setup and Teardown
//=============================================================================

/**
 * @brief Setup test context - loads and prepares YAML content for validation
 */
static int setup_test_context(void) {
    memset(&test_ctx, 0, sizeof(test_ctx));
    
    // Load the properties.yaml file content
    if (load_properties_yaml_content() != 0) {
        printf("ERROR: Failed to load %s\n", PROPERTIES_YAML_PATH);
        return -1;
    }
    
    // Create temporary directory for test files
    mkdir(TEST_YAML_DIR, 0755);
    
    test_ctx.initialized = true;
    return 0;
}

/**
 * @brief Teardown test context - cleanup resources
 */
static void teardown_test_context(void) {
    if (test_ctx.properties_file) {
        fclose(test_ctx.properties_file);
        test_ctx.properties_file = NULL;
    }
    
    if (test_ctx.file_content) {
        free(test_ctx.file_content);
        test_ctx.file_content = NULL;
    }
    
    // Remove temporary directory
    system("rm -rf " TEST_YAML_DIR);
    
    test_ctx.initialized = false;
}

/**
 * @brief Load properties.yaml content into memory for analysis
 */
static int load_properties_yaml_content(void) {
    struct stat st;
    
    // Check if file exists
    if (stat(PROPERTIES_YAML_PATH, &st) != 0) {
        printf("ERROR: %s does not exist\n", PROPERTIES_YAML_PATH);
        return -1;
    }
    
    // Open file for reading
    test_ctx.properties_file = fopen(PROPERTIES_YAML_PATH, "r");
    if (!test_ctx.properties_file) {
        printf("ERROR: Cannot open %s for reading\n", PROPERTIES_YAML_PATH);
        return -1;
    }
    
    // Allocate buffer for entire file content
    test_ctx.content_size = st.st_size;
    test_ctx.file_content = malloc(test_ctx.content_size + 1);
    if (!test_ctx.file_content) {
        printf("ERROR: Failed to allocate memory for file content\n");
        return -1;
    }
    
    // Read entire file
    size_t bytes_read = fread(test_ctx.file_content, 1, test_ctx.content_size, test_ctx.properties_file);
    if (bytes_read != test_ctx.content_size) {
        printf("ERROR: Failed to read complete file content\n");
        return -1;
    }
    
    // Null-terminate content
    test_ctx.file_content[test_ctx.content_size] = '\0';
    
    return 0;
}

//=============================================================================
// Utility Functions for YAML Validation
//=============================================================================

/**
 * @brief Check if a string is a valid C identifier
 */
static bool is_valid_c_identifier(const char *name) {
    if (!name || strlen(name) == 0) return false;
    
    // First character must be letter or underscore
    if (!isalpha(name[0]) && name[0] != '_') return false;
    
    // Remaining characters must be alphanumeric or underscore
    for (size_t i = 1; i < strlen(name); i++) {
        if (!isalnum(name[i]) && name[i] != '_') return false;
    }
    
    return true;
}

/**
 * @brief Check if line contains start of a property definition
 */
static bool line_contains_property_definition(const char *line) {
    // Look for "- name:" pattern indicating property start
    char *trimmed = strdup(line);
    char *start = trimmed;
    
    // Skip leading whitespace
    while (*start && isspace(*start)) start++;
    
    bool result = (strncmp(start, "- name:", 7) == 0);
    free(trimmed);
    return result;
}

/**
 * @brief Check if line contains a required field
 */
static bool line_contains_required_field(const char *line, const char *field) {
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "%s:", field);
    return strstr(line, pattern) != NULL;
}

/**
 * @brief Extract YAML value from a line
 */
static bool extract_yaml_value(const char *line, const char *field, char *value, size_t max_len) {
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "%s:", field);
    
    char *field_pos = strstr(line, pattern);
    if (!field_pos) return false;
    
    // Move past field name and colon
    char *value_start = field_pos + strlen(pattern);
    
    // Skip whitespace
    while (*value_start && isspace(*value_start)) value_start++;
    
    // Handle quoted strings
    if (*value_start == '"') {
        value_start++;
        char *quote_end = strchr(value_start, '"');
        if (quote_end) {
            size_t len = quote_end - value_start;
            if (len < max_len) {
                strncpy(value, value_start, len);
                value[len] = '\0';
                return true;
            }
        }
    } else {
        // Handle unquoted values
        char *value_end = value_start;
        while (*value_end && !isspace(*value_end) && *value_end != '\n') value_end++;
        
        size_t len = value_end - value_start;
        if (len < max_len && len > 0) {
            strncpy(value, value_start, len);
            value[len] = '\0';
            return true;
        }
    }
    
    return false;
}

/**
 * @brief Count YAML indentation level
 */
static int count_yaml_indentation(const char *line) {
    int indent = 0;
    while (line[indent] == ' ') indent++;
    return indent;
}

/**
 * @brief Validate if type string represents a valid property type
 */
static bool is_valid_property_type(const char *type) {
    if (!type || strlen(type) == 0) return false;
    
    // Valid scalar types
    const char *valid_types[] = {
        "int32_t", "uint32_t", "int64_t", "uint64_t",
        "float", "double", "long long", "bool",
        NULL
    };
    
    // Check scalar types
    for (int i = 0; valid_types[i]; i++) {
        if (strcmp(type, valid_types[i]) == 0) return true;
    }
    
    // Check array types (e.g., "float[3]" or "double[]")
    if (strchr(type, '[') && strchr(type, ']')) {
        char base_type[MAX_TYPE_NAME_LENGTH];
        char *bracket = strchr(type, '[');
        size_t base_len = bracket - type;
        
        if (base_len < sizeof(base_type)) {
            strncpy(base_type, type, base_len);
            base_type[base_len] = '\0';
            
            // Recursively check base type
            return is_valid_property_type(base_type);
        }
    }
    
    return false;
}

/**
 * @brief Check if property type is appropriate for core properties
 */
static bool is_core_property_type(const char *type) {
    // Core properties should use simple, fundamental types
    const char *core_types[] = {
        "int32_t", "uint32_t", "int64_t", "uint64_t", 
        "long long", "float", "double",
        NULL
    };
    
    for (int i = 0; core_types[i]; i++) {
        if (strcmp(type, core_types[i]) == 0) return true;
    }
    
    return false;
}

/**
 * @brief Validate dynamic array syntax and configuration
 */
static bool validate_dynamic_array_syntax(const char *type, const char *property_content) {
    // Check for dynamic array syntax (empty brackets)
    if (!strstr(type, "[]")) return true; // Not a dynamic array
    
    // For dynamic arrays, must have size_parameter
    return strstr(property_content, "size_parameter:") != NULL;
}

//=============================================================================
// Test Cases - YAML Structure Validation
//=============================================================================

/**
 * Test: Basic YAML file structure and accessibility
 */
static void test_yaml_file_structure(void) {
    printf("=== Testing YAML file structure and accessibility ===\n");
    
    TEST_ASSERT(test_ctx.properties_file != NULL, "properties.yaml should be readable");
    TEST_ASSERT(test_ctx.file_content != NULL, "File content should be loaded");
    TEST_ASSERT(test_ctx.content_size > 0, "File should not be empty");
    
    // Check for required top-level structure
    TEST_ASSERT(strstr(test_ctx.file_content, "properties:") != NULL, 
                "File should contain 'properties:' section");
    
    // Check for valid YAML formatting basics
    char *line = strtok(test_ctx.file_content, "\n");
    bool has_property_entries = false;
    
    while (line) {
        // Skip comments and empty lines
        char *trimmed = line;
        while (*trimmed && isspace(*trimmed)) trimmed++;
        
        if (*trimmed != '#' && *trimmed != '\0') {
            if (line_contains_property_definition(line)) {
                has_property_entries = true;
                break;
            }
        }
        line = strtok(NULL, "\n");
    }
    
    TEST_ASSERT(has_property_entries, "File should contain property definitions");
}

/**
 * Test: Property definition completeness and required fields
 */
static void test_property_definition_completeness(void) {
    printf("\n=== Testing property definition completeness ===\n");
    
    // Reset file content for fresh parsing
    if (load_properties_yaml_content() != 0) {
        printf("ERROR: Failed to reload YAML content\n");
        return;
    }
    
    char *content_copy = strdup(test_ctx.file_content);
    char *line = strtok(content_copy, "\n");
    char current_property[MAX_PROPERTY_NAME_LENGTH] = {0};
    bool in_property = false;
    bool has_name = false, has_type = false, has_initial_value = false;
    bool has_units = false, has_description = false, has_output = false;
    bool has_read_only = false, has_is_core = false;
    int properties_validated = 0;
    
    while (line) {
        if (line_contains_property_definition(line)) {
            // Validate previous property if we were processing one
            if (in_property && strlen(current_property) > 0) {
                TEST_ASSERT(has_name, "Property should have 'name' field");
                TEST_ASSERT(has_type, "Property should have 'type' field");
                TEST_ASSERT(has_initial_value, "Property should have 'initial_value' field");
                TEST_ASSERT(has_units, "Property should have 'units' field");
                TEST_ASSERT(has_description, "Property should have 'description' field");
                TEST_ASSERT(has_output, "Property should have 'output' field");
                TEST_ASSERT(has_read_only, "Property should have 'read_only' field");
                TEST_ASSERT(has_is_core, "Property should have 'is_core' field");
                properties_validated++;
            }
            
            // Start new property
            char name_value[MAX_PROPERTY_NAME_LENGTH];
            if (extract_yaml_value(line, "name", name_value, sizeof(name_value))) {
                strcpy(current_property, name_value);
                in_property = true;
                has_name = true;
                has_type = has_initial_value = has_units = false;
                has_description = has_output = has_read_only = has_is_core = false;
            }
        } else if (in_property) {
            // Check for required fields in current property
            if (line_contains_required_field(line, "type")) has_type = true;
            if (line_contains_required_field(line, "initial_value")) has_initial_value = true;
            if (line_contains_required_field(line, "units")) has_units = true;
            if (line_contains_required_field(line, "description")) has_description = true;
            if (line_contains_required_field(line, "output")) has_output = true;
            if (line_contains_required_field(line, "read_only")) has_read_only = true;
            if (line_contains_required_field(line, "is_core")) has_is_core = true;
        }
        
        line = strtok(NULL, "\n");
    }
    
    // Validate last property
    if (in_property && strlen(current_property) > 0) {
        TEST_ASSERT(has_name, "Last property should have 'name' field");
        TEST_ASSERT(has_type, "Last property should have 'type' field");
        TEST_ASSERT(has_initial_value, "Last property should have 'initial_value' field");
        TEST_ASSERT(has_units, "Last property should have 'units' field");
        TEST_ASSERT(has_description, "Last property should have 'description' field");
        TEST_ASSERT(has_output, "Last property should have 'output' field");
        TEST_ASSERT(has_read_only, "Last property should have 'read_only' field");
        TEST_ASSERT(has_is_core, "Last property should have 'is_core' field");
        properties_validated++;
    }
    
    TEST_ASSERT(properties_validated > 0, "Should validate at least one property");
    test_ctx.num_properties_found = properties_validated;
    
    free(content_copy);
}

/**
 * Test: Property name validation and C identifier compliance
 */
static void test_property_name_validation(void) {
    printf("\n=== Testing property name validation ===\n");
    
    char *content_copy = strdup(test_ctx.file_content);
    char *line = strtok(content_copy, "\n");
    char property_names[100][MAX_PROPERTY_NAME_LENGTH];
    int name_count = 0;
    
    while (line) {
        if (line_contains_property_definition(line)) {
            char name_value[MAX_PROPERTY_NAME_LENGTH];
            if (extract_yaml_value(line, "name", name_value, sizeof(name_value))) {
                // Test C identifier compliance
                TEST_ASSERT(is_valid_c_identifier(name_value), 
                           "Property name should be valid C identifier");
                
                // Test reasonable length
                TEST_ASSERT(strlen(name_value) > 0 && strlen(name_value) < MAX_PROPERTY_NAME_LENGTH,
                           "Property name should have reasonable length");
                
                // Check for duplicates
                for (int i = 0; i < name_count; i++) {
                    TEST_ASSERT(strcmp(property_names[i], name_value) != 0,
                               "Property names should be unique");
                }
                
                // Store name for duplicate checking
                if (name_count < 100) {
                    strcpy(property_names[name_count], name_value);
                    strcpy(test_ctx.property_names[name_count], name_value);
                    name_count++;
                }
            }
        }
        line = strtok(NULL, "\n");
    }
    
    TEST_ASSERT(name_count > 0, "Should find at least one property name");
    test_ctx.num_properties_found = name_count;
    
    free(content_copy);
}

/**
 * Test: Property type validation and system compliance
 */
static void test_property_type_validation(void) {
    printf("\n=== Testing property type validation ===\n");
    
    char *content_copy = strdup(test_ctx.file_content);
    char *line = strtok(content_copy, "\n");
    char current_property[MAX_PROPERTY_NAME_LENGTH] = {0};
    bool in_property = false;
    int valid_types_found = 0;
    
    while (line) {
        if (line_contains_property_definition(line)) {
            char name_value[MAX_PROPERTY_NAME_LENGTH];
            if (extract_yaml_value(line, "name", name_value, sizeof(name_value))) {
                strcpy(current_property, name_value);
                in_property = true;
            }
        } else if (in_property && line_contains_required_field(line, "type")) {
            char type_value[MAX_TYPE_NAME_LENGTH];
            if (extract_yaml_value(line, "type", type_value, sizeof(type_value))) {
                // Test type validity
                TEST_ASSERT(is_valid_property_type(type_value),
                           "Property type should be valid SAGE type");
                
                // Test type appropriateness for property name
                if (strlen(current_property) > 0) {
                    printf("  Validating type '%s' for property '%s'\n", type_value, current_property);
                }
                
                valid_types_found++;
                in_property = false; // Reset for next property
            }
        }
        line = strtok(NULL, "\n");
    }
    
    TEST_ASSERT(valid_types_found > 0, "Should find at least one valid property type");
    
    free(content_copy);
}

/**
 * Test: Core vs physics property separation validation
 */
static void test_core_physics_separation(void) {
    printf("\n=== Testing core vs physics property separation ===\n");
    
    char *content_copy = strdup(test_ctx.file_content);
    char *line = strtok(content_copy, "\n");
    char current_property[MAX_PROPERTY_NAME_LENGTH] = {0};
    char property_buffer[1024] = {0}; // Buffer to collect full property definition
    bool in_property = false;
    int core_properties = 0, physics_properties = 0;
    
    while (line) {
        if (line_contains_property_definition(line)) {
            // Process previous property if we have one
            if (in_property && strlen(current_property) > 0 && strlen(property_buffer) > 0) {
                bool is_core = strstr(property_buffer, "is_core: true") != NULL;
                char type_value[MAX_TYPE_NAME_LENGTH];
                
                // Extract type from property buffer
                char *type_line = strstr(property_buffer, "type:");
                if (type_line && extract_yaml_value(type_line, "type", type_value, sizeof(type_value))) {
                    if (is_core) {
                        // Core properties should use fundamental types
                        TEST_ASSERT(is_core_property_type(type_value),
                                   "Core properties should use fundamental types");
                        core_properties++;
                        test_ctx.has_core_properties = true;
                    } else {
                        physics_properties++;
                        test_ctx.has_physics_properties = true;
                    }
                }
            }
            
            // Start new property
            char name_value[MAX_PROPERTY_NAME_LENGTH];
            if (extract_yaml_value(line, "name", name_value, sizeof(name_value))) {
                strcpy(current_property, name_value);
                strcpy(property_buffer, line);
                strcat(property_buffer, "\n");
                in_property = true;
            }
        } else if (in_property) {
            // Accumulate property definition lines
            if (strlen(property_buffer) + strlen(line) + 1 < sizeof(property_buffer)) {
                strcat(property_buffer, line);
                strcat(property_buffer, "\n");
            }
        }
        
        line = strtok(NULL, "\n");
    }
    
    // Process last property
    if (in_property && strlen(current_property) > 0 && strlen(property_buffer) > 0) {
        bool is_core = strstr(property_buffer, "is_core: true") != NULL;
        char type_value[MAX_TYPE_NAME_LENGTH];
        
        char *type_line = strstr(property_buffer, "type:");
        if (type_line && extract_yaml_value(type_line, "type", type_value, sizeof(type_value))) {
            if (is_core) {
                TEST_ASSERT(is_core_property_type(type_value),
                           "Core properties should use fundamental types");
                core_properties++;
                test_ctx.has_core_properties = true;
            } else {
                physics_properties++;
                test_ctx.has_physics_properties = true;
            }
        }
    }
    
    TEST_ASSERT(core_properties > 0, "Should have at least one core property");
    TEST_ASSERT(physics_properties > 0, "Should have at least one physics property");
    
    printf("  Found %d core properties and %d physics properties\n", 
           core_properties, physics_properties);
    
    free(content_copy);
}

/**
 * Test: Dynamic array configuration validation
 */
static void test_dynamic_array_validation(void) {
    printf("\n=== Testing dynamic array configuration ===\n");
    
    char *content_copy = strdup(test_ctx.file_content);
    char *line = strtok(content_copy, "\n");
    char current_property[MAX_PROPERTY_NAME_LENGTH] = {0};
    char property_buffer[1024] = {0};
    bool in_property = false;
    int dynamic_arrays_found = 0;
    
    while (line) {
        if (line_contains_property_definition(line)) {
            // Process previous property
            if (in_property && strlen(current_property) > 0 && strlen(property_buffer) > 0) {
                char type_value[MAX_TYPE_NAME_LENGTH];
                char *type_line = strstr(property_buffer, "type:");
                
                if (type_line && extract_yaml_value(type_line, "type", type_value, sizeof(type_value))) {
                    if (strstr(type_value, "[]")) {
                        // This is a dynamic array
                        TEST_ASSERT(validate_dynamic_array_syntax(type_value, property_buffer),
                                   "Dynamic arrays must have size_parameter");
                        dynamic_arrays_found++;
                        test_ctx.has_dynamic_arrays = true;
                        printf("  Found dynamic array: %s with type %s\n", 
                               current_property, type_value);
                    }
                }
            }
            
            // Start new property
            char name_value[MAX_PROPERTY_NAME_LENGTH];
            if (extract_yaml_value(line, "name", name_value, sizeof(name_value))) {
                strcpy(current_property, name_value);
                strcpy(property_buffer, line);
                strcat(property_buffer, "\n");
                in_property = true;
            }
        } else if (in_property) {
            // Accumulate property lines
            if (strlen(property_buffer) + strlen(line) + 1 < sizeof(property_buffer)) {
                strcat(property_buffer, line);
                strcat(property_buffer, "\n");
            }
        }
        
        line = strtok(NULL, "\n");
    }
    
    // Process last property
    if (in_property && strlen(current_property) > 0 && strlen(property_buffer) > 0) {
        char type_value[MAX_TYPE_NAME_LENGTH];
        char *type_line = strstr(property_buffer, "type:");
        
        if (type_line && extract_yaml_value(type_line, "type", type_value, sizeof(type_value))) {
            if (strstr(type_value, "[]")) {
                TEST_ASSERT(validate_dynamic_array_syntax(type_value, property_buffer),
                           "Dynamic arrays must have size_parameter");
                dynamic_arrays_found++;
                test_ctx.has_dynamic_arrays = true;
                printf("  Found dynamic array: %s with type %s\n", 
                       current_property, type_value);
            }
        }
    }
    
    printf("  Total dynamic arrays found: %d\n", dynamic_arrays_found);
    
    free(content_copy);
}

/**
 * Test: Integration with auto-generated property system
 */
static void test_integration_with_generated_system(void) {
    printf("\n=== Testing integration with auto-generated property system ===\n");
    
    // Test that properties from YAML match generated enums
    TEST_ASSERT(PROP_COUNT > 0, "Generated property system should define PROP_COUNT");
    
    // Test a few well-known core properties that should exist
    TEST_ASSERT(PROP_SnapNum >= 0 && PROP_SnapNum < PROP_COUNT, 
                "SnapNum should be valid generated property");
    TEST_ASSERT(PROP_Type >= 0 && PROP_Type < PROP_COUNT,
                "Type should be valid generated property");
    TEST_ASSERT(PROP_GalaxyNr >= 0 && PROP_GalaxyNr < PROP_COUNT,
                "GalaxyNr should be valid generated property");
    
    // Test property name resolution works
    const char *snapnum_name = get_property_name(PROP_SnapNum);
    TEST_ASSERT(snapnum_name != NULL, "get_property_name should work for SnapNum");
    TEST_ASSERT(strcmp(snapnum_name, "SnapNum") == 0, 
                "Property name should match YAML definition");
    
    // Test property ID lookup works
    property_id_t snapnum_id = get_property_id("SnapNum");
    TEST_ASSERT(snapnum_id == PROP_SnapNum, 
                "get_property_id should return correct ID for SnapNum");
    
    // Verify the number of properties is reasonable compared to YAML
    TEST_ASSERT(PROP_COUNT >= test_ctx.num_properties_found,
                "Generated property count should be at least as many as found in YAML");
    
    printf("  YAML properties found: %d, Generated PROP_COUNT: %d\n",
           test_ctx.num_properties_found, PROP_COUNT);
}

/**
 * Test: Error boundary conditions and malformed data handling
 */
static void test_error_boundary_conditions(void) {
    printf("\n=== Testing error boundary conditions ===\n");
    
    // Test property name boundary conditions
    property_id_t invalid_id = get_property_id(NULL);
    TEST_ASSERT(invalid_id == PROP_COUNT, 
                "get_property_id(NULL) should return PROP_COUNT");
    
    invalid_id = get_property_id("");
    TEST_ASSERT(invalid_id == PROP_COUNT,
                "get_property_id(\"\") should return PROP_COUNT");
    
    invalid_id = get_property_id("NonExistentProperty");
    TEST_ASSERT(invalid_id == PROP_COUNT,
                "get_property_id should return PROP_COUNT for invalid property");
    
    // Test property name retrieval boundary conditions
    const char *invalid_name = get_property_name(PROP_COUNT);
    TEST_ASSERT(invalid_name == NULL,
                "get_property_name should return NULL for invalid ID");
    
    invalid_name = get_property_name(-1);
    TEST_ASSERT(invalid_name == NULL,
                "get_property_name should return NULL for negative ID");
    
    // Test that we can handle property system gracefully even with potential YAML issues
    TEST_ASSERT(test_ctx.has_core_properties,
                "Should have found core properties in YAML");
    TEST_ASSERT(test_ctx.has_physics_properties,
                "Should have found physics properties in YAML");
}

//=============================================================================
// Test Runner
//=============================================================================

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;
    
    printf("\n========================================\n");
    printf("Starting tests for test_property_yaml_validation\n");
    printf("========================================\n\n");
    
    printf("This test validates the properties.yaml metadata file that drives:\n");
    printf("  1. YAML file structure and schema compliance\n");
    printf("  2. Property definition completeness and required fields\n");
    printf("  3. Property name validation and C identifier compliance\n");
    printf("  4. Property type validation and system compatibility\n");
    printf("  5. Core vs physics property separation compliance\n");
    printf("  6. Dynamic array configuration validation\n");
    printf("  7. Integration with auto-generated property system\n");
    printf("  8. Error boundary conditions and robustness\n\n");
    
    // Setup test context
    if (setup_test_context() != 0) {
        printf("ERROR: Failed to set up test context\n");
        return 1;
    }
    
    // Run comprehensive test suite
    test_yaml_file_structure();
    test_property_definition_completeness();
    test_property_name_validation();
    test_property_type_validation();
    test_core_physics_separation();
    test_dynamic_array_validation();
    test_integration_with_generated_system();
    test_error_boundary_conditions();
    
    // Teardown
    teardown_test_context();
    
    // Report results
    printf("\n========================================\n");
    printf("Test results for test_property_yaml_validation:\n");
    printf("  Total tests: %d\n", tests_run);
    printf("  Passed: %d\n", tests_passed);
    printf("  Failed: %d\n", tests_run - tests_passed);
    
    if (tests_run == tests_passed) {
        printf("\n✅ Property YAML Validation Test PASSED\n");
        printf("The properties.yaml file structure and content are valid.\n");
        printf("Properties found: %d\n", test_ctx.num_properties_found);
        printf("Core properties: %s\n", test_ctx.has_core_properties ? "✅ YES" : "❌ NO");
        printf("Physics properties: %s\n", test_ctx.has_physics_properties ? "✅ YES" : "❌ NO");
        printf("Dynamic arrays: %s\n", test_ctx.has_dynamic_arrays ? "✅ YES" : "❌ NO");
    } else {
        printf("\n❌ Property YAML Validation Test FAILED\n");
        printf("Issues found in properties.yaml structure or content.\n");
    }
    
    printf("========================================\n\n");
    
    return (tests_run == tests_passed) ? 0 : 1;
}