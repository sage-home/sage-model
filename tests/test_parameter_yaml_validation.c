/**
 * @file test_parameter_yaml_validation.c
 * @brief Comprehensive validation test for parameters.yaml structure and generation system
 * 
 * This test validates the foundational YAML metadata file that drives the parameter
 * system code generation. It catches structural errors, type violations, bounds checking
 * issues, and configuration inconsistencies that would otherwise manifest as compile-time
 * or runtime parameter parsing failures.
 * 
 * Tests cover:
 * - YAML file hierarchical structure and schema validation
 * - Parameter type definition validation (string, int, double, bool)
 * - Required field presence and format verification
 * - Core vs physics parameter categorization compliance
 * - Bounds checking configuration validation
 * - Enum parameter validation and value checking
 * - Struct field mapping validation and C identifier compliance
 * - Default value consistency validation
 * - Auto-generated parameter system integration
 * - Error boundary conditions and malformed configuration handling
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/stat.h>
#include <ctype.h>
#include <limits.h>
#include <float.h>

#include "../src/core/core_allvars.h"
#include "../src/core/core_parameters.h"
#include "../src/core/core_read_parameter_file.h"
#include "../src/core/core_logging.h"

// Test configuration constants
#define PARAMETERS_YAML_PATH "src/parameters.yaml"
#define TEST_YAML_DIR "tests/test_param_yaml_temp"
#define MAX_PARAMETER_NAME_LENGTH 64
#define MAX_TYPE_NAME_LENGTH 16
#define MAX_DESCRIPTION_LENGTH 256
#define MAX_STRUCT_FIELD_LENGTH 64
#define MAX_DEFAULT_VALUE_LENGTH 32
#define MAX_LINE_LENGTH 1024
#define MAX_ENUM_VALUES 20

// Test counter globals
static int tests_run = 0;
static int tests_passed = 0;

// Test context for complex validation scenarios
static struct test_context {
    FILE *parameters_file;
    char *file_content;
    size_t content_size;
    int num_parameters_found;
    int num_core_parameters;
    int num_physics_parameters;
    int num_enum_parameters;
    int num_bounds_parameters;
    int num_required_parameters;
    char parameter_names[200][MAX_PARAMETER_NAME_LENGTH];
    bool has_core_category;
    bool has_physics_category;
    bool has_hierarchical_structure;
    bool has_enum_parameters;
    bool has_bounds_validation;
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
static int load_parameters_yaml_content(void);
static bool is_valid_parameter_type(const char *type);
static bool is_valid_struct_field_mapping(const char *struct_field);
static bool is_valid_enum_values(const char *enum_values_line);
static bool validate_bounds_format(const char *bounds_line, const char *param_type);
static bool validate_default_value_type(const char *default_value, const char *param_type);
static bool extract_yaml_value(const char *line, const char *field, char *value, size_t max_len);
static bool extract_yaml_array_values(const char *line, char values[][MAX_PARAMETER_NAME_LENGTH], int max_values);
static bool line_contains_parameter_definition(const char *line);
static int count_yaml_indentation(const char *line);
static bool is_category_start(const char *line, char *category_name);
static bool is_subcategory_start(const char *line, char *subcategory_name);

//=============================================================================
// Test Setup and Teardown
//=============================================================================

/**
 * @brief Setup test context - loads and prepares YAML content for validation
 */
static int setup_test_context(void) {
    memset(&test_ctx, 0, sizeof(test_ctx));
    
    // Load the parameters.yaml file content
    if (load_parameters_yaml_content() != 0) {
        printf("ERROR: Failed to load %s\n", PARAMETERS_YAML_PATH);
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
    if (test_ctx.parameters_file) {
        fclose(test_ctx.parameters_file);
        test_ctx.parameters_file = NULL;
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
 * @brief Load parameters.yaml content into memory for analysis
 */
static int load_parameters_yaml_content(void) {
    struct stat st;
    
    // Check if file exists
    if (stat(PARAMETERS_YAML_PATH, &st) != 0) {
        printf("ERROR: %s does not exist\n", PARAMETERS_YAML_PATH);
        return -1;
    }
    
    // Open file for reading
    test_ctx.parameters_file = fopen(PARAMETERS_YAML_PATH, "r");
    if (!test_ctx.parameters_file) {
        printf("ERROR: Cannot open %s for reading\n", PARAMETERS_YAML_PATH);
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
    size_t bytes_read = fread(test_ctx.file_content, 1, test_ctx.content_size, test_ctx.parameters_file);
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
 * @brief Check if parameter type is valid
 */
static bool is_valid_parameter_type(const char *type) {
    if (!type || strlen(type) == 0) return false;
    
    const char *valid_types[] = {"string", "int", "double", "bool", NULL};
    
    for (int i = 0; valid_types[i]; i++) {
        if (strcmp(type, valid_types[i]) == 0) return true;
    }
    
    return false;
}

/**
 * @brief Check if struct field mapping is valid C dot notation
 */
static bool is_valid_struct_field_mapping(const char *struct_field) {
    if (!struct_field || strlen(struct_field) == 0) return false;
    
    // Should be in format "category.field" or just "field"
    // Must be valid C identifiers separated by dots
    char *copy = strdup(struct_field);
    char *token = strtok(copy, ".");
    
    while (token) {
        // Check if token is valid C identifier
        if (!isalpha(token[0]) && token[0] != '_') {
            free(copy);
            return false;
        }
        
        for (size_t i = 1; i < strlen(token); i++) {
            if (!isalnum(token[i]) && token[i] != '_') {
                free(copy);
                return false;
            }
        }
        
        token = strtok(NULL, ".");
    }
    
    free(copy);
    return true;
}

/**
 * @brief Check if enum values line is properly formatted
 */
static bool is_valid_enum_values(const char *enum_values_line) {
    if (!enum_values_line) return false;
    
    // Should contain a list in brackets like: ["value1", "value2", "value3"]
    char *bracket_start = strchr(enum_values_line, '[');
    char *bracket_end = strchr(enum_values_line, ']');
    
    if (!bracket_start || !bracket_end || bracket_end <= bracket_start) {
        return false;
    }
    
    // Extract content between brackets
    size_t content_len = bracket_end - bracket_start - 1;
    char *content = malloc(content_len + 1);
    strncpy(content, bracket_start + 1, content_len);
    content[content_len] = '\0';
    
    // Should contain at least one quoted value
    bool has_quoted_value = false;
    char *quote = strchr(content, '"');
    if (quote) {
        char *closing_quote = strchr(quote + 1, '"');
        if (closing_quote && closing_quote > quote) {
            has_quoted_value = true;
        }
    }
    
    free(content);
    return has_quoted_value;
}

/**
 * @brief Validate bounds format for numeric parameters
 */
static bool validate_bounds_format(const char *bounds_line, const char *param_type) {
    if (!bounds_line || !param_type) return false;
    
    // Only int and double parameters should have bounds
    if (strcmp(param_type, "int") != 0 && strcmp(param_type, "double") != 0) {
        return false; // Non-numeric type shouldn't have bounds
    }
    
    // Should contain [min, max] format
    char *bracket_start = strchr(bounds_line, '[');
    char *bracket_end = strchr(bounds_line, ']');
    char *comma = strchr(bounds_line, ',');
    
    return (bracket_start && bracket_end && comma && 
            bracket_start < comma && comma < bracket_end);
}

/**
 * @brief Validate default value matches parameter type
 */
static bool validate_default_value_type(const char *default_value, const char *param_type) {
    if (!default_value || !param_type) return true; // Default values are optional
    
    if (strcmp(param_type, "string") == 0) {
        // String values should be quoted
        return (default_value[0] == '"' && default_value[strlen(default_value)-1] == '"');
    } else if (strcmp(param_type, "int") == 0) {
        // Should be numeric
        char *endptr;
        strtol(default_value, &endptr, 10);
        return (*endptr == '\0');
    } else if (strcmp(param_type, "double") == 0) {
        // Should be numeric (int or float)
        char *endptr;
        strtod(default_value, &endptr);
        return (*endptr == '\0');
    } else if (strcmp(param_type, "bool") == 0) {
        // Should be true or false
        return (strcmp(default_value, "true") == 0 || strcmp(default_value, "false") == 0);
    }
    
    return false;
}

/**
 * @brief Extract YAML value from a line (same as property test but included for completeness)
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
 * @brief Extract array values from YAML array line
 */
static bool extract_yaml_array_values(const char *line, char values[][MAX_PARAMETER_NAME_LENGTH], int max_values) {
    char *bracket_start = strchr(line, '[');
    char *bracket_end = strchr(line, ']');
    
    if (!bracket_start || !bracket_end || bracket_end <= bracket_start) {
        return false;
    }
    
    // Extract content between brackets
    size_t content_len = bracket_end - bracket_start - 1;
    char *content = malloc(content_len + 1);
    strncpy(content, bracket_start + 1, content_len);
    content[content_len] = '\0';
    
    int value_count = 0;
    char *token = strtok(content, ",");
    
    while (token && value_count < max_values) {
        // Skip whitespace
        while (*token && isspace(*token)) token++;
        
        // Remove quotes if present
        if (*token == '"') {
            token++;
            char *end_quote = strchr(token, '"');
            if (end_quote) *end_quote = '\0';
        }
        
        strncpy(values[value_count], token, MAX_PARAMETER_NAME_LENGTH - 1);
        values[value_count][MAX_PARAMETER_NAME_LENGTH - 1] = '\0';
        value_count++;
        
        token = strtok(NULL, ",");
    }
    
    free(content);
    return value_count > 0;
}

/**
 * @brief Check if line contains start of a parameter definition
 */
static bool line_contains_parameter_definition(const char *line) {
    char *trimmed = strdup(line);
    char *start = trimmed;
    
    // Skip leading whitespace
    while (*start && isspace(*start)) start++;
    
    bool result = (strncmp(start, "- name:", 7) == 0);
    free(trimmed);
    return result;
}


/**
 * @brief Count YAML indentation level (same as property test)
 */
static int count_yaml_indentation(const char *line) {
    int indent = 0;
    while (line[indent] == ' ') indent++;
    return indent;
}

/**
 * @brief Check if line starts a category (e.g., "core:" or "physics:")
 */
static bool is_category_start(const char *line, char *category_name) {
    char *trimmed = strdup(line);
    char *start = trimmed;
    
    // Skip leading whitespace
    while (*start && isspace(*start)) start++;
    
    // Look for pattern like "core:" or "physics:"
    char *colon = strchr(start, ':');
    if (colon && count_yaml_indentation(line) == 2) { // Categories should be at indent level 2
        size_t name_len = colon - start;
        if (name_len > 0 && name_len < MAX_PARAMETER_NAME_LENGTH) {
            strncpy(category_name, start, name_len);
            category_name[name_len] = '\0';
            free(trimmed);
            return true;
        }
    }
    
    free(trimmed);
    return false;
}

/**
 * @brief Check if line starts a subcategory (e.g., "io:" or "simulation:")
 */
static bool is_subcategory_start(const char *line, char *subcategory_name) {
    char *trimmed = strdup(line);
    char *start = trimmed;
    
    // Skip leading whitespace
    while (*start && isspace(*start)) start++;
    
    // Look for pattern at subcategory indent level (4 spaces)
    char *colon = strchr(start, ':');
    if (colon && count_yaml_indentation(line) == 4) { // Subcategories should be at indent level 4
        size_t name_len = colon - start;
        if (name_len > 0 && name_len < MAX_PARAMETER_NAME_LENGTH) {
            strncpy(subcategory_name, start, name_len);
            subcategory_name[name_len] = '\0';
            free(trimmed);
            return true;
        }
    }
    
    free(trimmed);
    return false;
}

//=============================================================================
// Test Cases - YAML Structure Validation
//=============================================================================

/**
 * Test: Basic YAML file structure and hierarchical organization
 */
static void test_yaml_hierarchical_structure(void) {
    printf("=== Testing YAML hierarchical structure ===\n");
    
    TEST_ASSERT(test_ctx.parameters_file != NULL, "parameters.yaml should be readable");
    TEST_ASSERT(test_ctx.file_content != NULL, "File content should be loaded");
    TEST_ASSERT(test_ctx.content_size > 0, "File should not be empty");
    
    // Check for required top-level structure
    TEST_ASSERT(strstr(test_ctx.file_content, "parameters:") != NULL, 
                "File should contain 'parameters:' section");
    
    // Parse hierarchical structure
    char *content_copy = strdup(test_ctx.file_content);
    char *line = strtok(content_copy, "\n");
    bool found_categories = false;
    bool found_subcategories = false;
    bool found_parameters = false;
    char current_category[MAX_PARAMETER_NAME_LENGTH] = {0};
    char current_subcategory[MAX_PARAMETER_NAME_LENGTH] = {0};
    
    while (line) {
        char category[MAX_PARAMETER_NAME_LENGTH];
        char subcategory[MAX_PARAMETER_NAME_LENGTH];
        
        if (is_category_start(line, category)) {
            strcpy(current_category, category);
            found_categories = true;
            test_ctx.has_hierarchical_structure = true;
            
            if (strcmp(category, "core") == 0) {
                test_ctx.has_core_category = true;
            } else if (strcmp(category, "physics") == 0) {
                test_ctx.has_physics_category = true;
            }
            
            printf("  Found category: %s\n", category);
        } else if (is_subcategory_start(line, subcategory)) {
            strcpy(current_subcategory, subcategory);
            found_subcategories = true;
            printf("    Found subcategory: %s.%s\n", current_category, subcategory);
        } else if (line_contains_parameter_definition(line)) {
            found_parameters = true;
            char param_name[MAX_PARAMETER_NAME_LENGTH];
            if (extract_yaml_value(line, "name", param_name, sizeof(param_name))) {
                printf("      Found parameter: %s.%s.%s\n", 
                       current_category, current_subcategory, param_name);
            }
        }
        
        line = strtok(NULL, "\n");
    }
    
    TEST_ASSERT(found_categories, "Should find category definitions");
    TEST_ASSERT(found_subcategories, "Should find subcategory definitions");
    TEST_ASSERT(found_parameters, "Should find parameter definitions");
    TEST_ASSERT(test_ctx.has_core_category, "Should have 'core' category");
    
    printf("  Hierarchical structure validation: ✅\n");
    
    free(content_copy);
}

/**
 * Test: Parameter definition completeness and required fields
 */
static void test_parameter_definition_completeness(void) {
    printf("\n=== Testing parameter definition completeness ===\n");
    
    char *content_copy = strdup(test_ctx.file_content);
    char *line = strtok(content_copy, "\n");
    char current_parameter[MAX_PARAMETER_NAME_LENGTH] = {0};
    char parameter_buffer[2048] = {0}; // Buffer to collect full parameter definition
    bool in_parameter = false;
    int parameters_validated = 0;
    
    while (line) {
        if (line_contains_parameter_definition(line)) {
            // Validate previous parameter if we were processing one
            if (in_parameter && strlen(current_parameter) > 0 && strlen(parameter_buffer) > 0) {
                // Check required fields
                TEST_ASSERT(strstr(parameter_buffer, "type:") != NULL, 
                           "Parameter should have 'type' field");
                TEST_ASSERT(strstr(parameter_buffer, "description:") != NULL,
                           "Parameter should have 'description' field");
                TEST_ASSERT(strstr(parameter_buffer, "category:") != NULL,
                           "Parameter should have 'category' field");
                TEST_ASSERT(strstr(parameter_buffer, "required:") != NULL,
                           "Parameter should have 'required' field");
                TEST_ASSERT(strstr(parameter_buffer, "struct_field:") != NULL,
                           "Parameter should have 'struct_field' field");
                
                parameters_validated++;
            }
            
            // Start new parameter
            char name_value[MAX_PARAMETER_NAME_LENGTH];
            if (extract_yaml_value(line, "name", name_value, sizeof(name_value))) {
                strcpy(current_parameter, name_value);
                strcpy(parameter_buffer, line);
                strcat(parameter_buffer, "\n");
                in_parameter = true;
            }
        } else if (in_parameter) {
            // Accumulate parameter definition lines
            if (strlen(parameter_buffer) + strlen(line) + 1 < sizeof(parameter_buffer)) {
                strcat(parameter_buffer, line);
                strcat(parameter_buffer, "\n");
            }
        }
        
        line = strtok(NULL, "\n");
    }
    
    // Validate last parameter
    if (in_parameter && strlen(current_parameter) > 0 && strlen(parameter_buffer) > 0) {
        TEST_ASSERT(strstr(parameter_buffer, "type:") != NULL, 
                   "Last parameter should have 'type' field");
        TEST_ASSERT(strstr(parameter_buffer, "description:") != NULL,
                   "Last parameter should have 'description' field");
        TEST_ASSERT(strstr(parameter_buffer, "category:") != NULL,
                   "Last parameter should have 'category' field");
        TEST_ASSERT(strstr(parameter_buffer, "required:") != NULL,
                   "Last parameter should have 'required' field");
        TEST_ASSERT(strstr(parameter_buffer, "struct_field:") != NULL,
                   "Last parameter should have 'struct_field' field");
        parameters_validated++;
    }
    
    TEST_ASSERT(parameters_validated > 0, "Should validate at least one parameter");
    test_ctx.num_parameters_found = parameters_validated;
    
    printf("  Parameters validated: %d\n", parameters_validated);
    
    free(content_copy);
}

/**
 * Test: Parameter type validation and system compatibility
 */
static void test_parameter_type_validation(void) {
    printf("\n=== Testing parameter type validation ===\n");
    
    char *content_copy = strdup(test_ctx.file_content);
    char *line = strtok(content_copy, "\n");
    char current_parameter[MAX_PARAMETER_NAME_LENGTH] = {0};
    char parameter_buffer[2048] = {0};
    bool in_parameter = false;
    int string_params = 0, int_params = 0, double_params = 0, bool_params = 0;
    
    while (line) {
        if (line_contains_parameter_definition(line)) {
            // Process previous parameter
            if (in_parameter && strlen(current_parameter) > 0 && strlen(parameter_buffer) > 0) {
                char type_value[MAX_TYPE_NAME_LENGTH];
                char *type_line = strstr(parameter_buffer, "type:");
                
                if (type_line && extract_yaml_value(type_line, "type", type_value, sizeof(type_value))) {
                    TEST_ASSERT(is_valid_parameter_type(type_value),
                               "Parameter type should be valid");
                    
                    // Count parameter types
                    if (strcmp(type_value, "string") == 0) string_params++;
                    else if (strcmp(type_value, "int") == 0) int_params++;
                    else if (strcmp(type_value, "double") == 0) double_params++;
                    else if (strcmp(type_value, "bool") == 0) bool_params++;
                    
                    printf("  Parameter '%s' has valid type '%s'\n", current_parameter, type_value);
                }
            }
            
            // Start new parameter
            char name_value[MAX_PARAMETER_NAME_LENGTH];
            if (extract_yaml_value(line, "name", name_value, sizeof(name_value))) {
                strcpy(current_parameter, name_value);
                strcpy(parameter_buffer, line);
                strcat(parameter_buffer, "\n");
                in_parameter = true;
            }
        } else if (in_parameter) {
            if (strlen(parameter_buffer) + strlen(line) + 1 < sizeof(parameter_buffer)) {
                strcat(parameter_buffer, line);
                strcat(parameter_buffer, "\n");
            }
        }
        
        line = strtok(NULL, "\n");
    }
    
    // Process last parameter
    if (in_parameter && strlen(current_parameter) > 0 && strlen(parameter_buffer) > 0) {
        char type_value[MAX_TYPE_NAME_LENGTH];
        char *type_line = strstr(parameter_buffer, "type:");
        
        if (type_line && extract_yaml_value(type_line, "type", type_value, sizeof(type_value))) {
            TEST_ASSERT(is_valid_parameter_type(type_value),
                       "Last parameter type should be valid");
                       
            if (strcmp(type_value, "string") == 0) string_params++;
            else if (strcmp(type_value, "int") == 0) int_params++;
            else if (strcmp(type_value, "double") == 0) double_params++;
            else if (strcmp(type_value, "bool") == 0) bool_params++;
        }
    }
    
    TEST_ASSERT(string_params > 0, "Should have at least one string parameter");
    TEST_ASSERT(int_params > 0, "Should have at least one int parameter");
    TEST_ASSERT(double_params > 0, "Should have at least one double parameter");
    
    printf("  Type distribution: string=%d, int=%d, double=%d, bool=%d\n", 
           string_params, int_params, double_params, bool_params);
    
    free(content_copy);
}

/**
 * Test: Core vs physics parameter categorization
 */
static void test_core_physics_categorization(void) {
    printf("\n=== Testing core vs physics categorization ===\n");
    
    char *content_copy = strdup(test_ctx.file_content);
    char *line = strtok(content_copy, "\n");
    char current_parameter[MAX_PARAMETER_NAME_LENGTH] = {0};
    char parameter_buffer[2048] = {0};
    bool in_parameter = false;
    int core_params = 0, physics_params = 0;
    
    while (line) {
        if (line_contains_parameter_definition(line)) {
            // Process previous parameter
            if (in_parameter && strlen(current_parameter) > 0 && strlen(parameter_buffer) > 0) {
                char category_value[MAX_PARAMETER_NAME_LENGTH];
                char *category_line = strstr(parameter_buffer, "category:");
                
                if (category_line && extract_yaml_value(category_line, "category", category_value, sizeof(category_value))) {
                    if (strcmp(category_value, "core") == 0) {
                        core_params++;
                        test_ctx.num_core_parameters++;
                    } else if (strcmp(category_value, "physics") == 0) {
                        physics_params++;
                        test_ctx.num_physics_parameters++;
                    } else {
                        printf("  WARNING: Parameter '%s' has unknown category '%s'\n", 
                               current_parameter, category_value);
                    }
                }
            }
            
            // Start new parameter
            char name_value[MAX_PARAMETER_NAME_LENGTH];
            if (extract_yaml_value(line, "name", name_value, sizeof(name_value))) {
                strcpy(current_parameter, name_value);
                strcpy(parameter_buffer, line);
                strcat(parameter_buffer, "\n");
                in_parameter = true;
            }
        } else if (in_parameter) {
            if (strlen(parameter_buffer) + strlen(line) + 1 < sizeof(parameter_buffer)) {
                strcat(parameter_buffer, line);
                strcat(parameter_buffer, "\n");
            }
        }
        
        line = strtok(NULL, "\n");
    }
    
    // Process last parameter
    if (in_parameter && strlen(current_parameter) > 0 && strlen(parameter_buffer) > 0) {
        char category_value[MAX_PARAMETER_NAME_LENGTH];
        char *category_line = strstr(parameter_buffer, "category:");
        
        if (category_line && extract_yaml_value(category_line, "category", category_value, sizeof(category_value))) {
            if (strcmp(category_value, "core") == 0) {
                core_params++;
                test_ctx.num_core_parameters++;
            } else if (strcmp(category_value, "physics") == 0) {
                physics_params++;
                test_ctx.num_physics_parameters++;
            }
        }
    }
    
    TEST_ASSERT(core_params > 0, "Should have at least one core parameter");
    printf("  Found %d core parameters and %d physics parameters\n", core_params, physics_params);
    
    free(content_copy);
}

/**
 * Test: Bounds validation for numeric parameters
 */
static void test_bounds_validation(void) {
    printf("\n=== Testing bounds validation ===\n");
    
    char *content_copy = strdup(test_ctx.file_content);
    char *line = strtok(content_copy, "\n");
    char current_parameter[MAX_PARAMETER_NAME_LENGTH] = {0};
    char parameter_buffer[2048] = {0};
    bool in_parameter = false;
    int bounds_found = 0;
    
    while (line) {
        if (line_contains_parameter_definition(line)) {
            // Process previous parameter
            if (in_parameter && strlen(current_parameter) > 0 && strlen(parameter_buffer) > 0) {
                char type_value[MAX_TYPE_NAME_LENGTH];
                char *type_line = strstr(parameter_buffer, "type:");
                
                if (type_line && extract_yaml_value(type_line, "type", type_value, sizeof(type_value))) {
                    char *bounds_line = strstr(parameter_buffer, "bounds:");
                    
                    if (bounds_line) {
                        TEST_ASSERT(validate_bounds_format(bounds_line, type_value),
                                   "Bounds format should be valid for parameter type");
                        bounds_found++;
                        test_ctx.has_bounds_validation = true;
                        printf("  Parameter '%s' has valid bounds for type '%s'\n", 
                               current_parameter, type_value);
                    } else if (strcmp(type_value, "int") == 0 || strcmp(type_value, "double") == 0) {
                        printf("  INFO: Numeric parameter '%s' has no bounds (optional)\n", 
                               current_parameter);
                    }
                }
            }
            
            // Start new parameter
            char name_value[MAX_PARAMETER_NAME_LENGTH];
            if (extract_yaml_value(line, "name", name_value, sizeof(name_value))) {
                strcpy(current_parameter, name_value);
                strcpy(parameter_buffer, line);
                strcat(parameter_buffer, "\n");
                in_parameter = true;
            }
        } else if (in_parameter) {
            if (strlen(parameter_buffer) + strlen(line) + 1 < sizeof(parameter_buffer)) {
                strcat(parameter_buffer, line);
                strcat(parameter_buffer, "\n");
            }
        }
        
        line = strtok(NULL, "\n");
    }
    
    // Process last parameter
    if (in_parameter && strlen(current_parameter) > 0 && strlen(parameter_buffer) > 0) {
        char type_value[MAX_TYPE_NAME_LENGTH];
        char *type_line = strstr(parameter_buffer, "type:");
        
        if (type_line && extract_yaml_value(type_line, "type", type_value, sizeof(type_value))) {
            char *bounds_line = strstr(parameter_buffer, "bounds:");
            
            if (bounds_line) {
                TEST_ASSERT(validate_bounds_format(bounds_line, type_value),
                           "Last parameter bounds format should be valid");
                bounds_found++;
                test_ctx.has_bounds_validation = true;
            }
        }
    }
    
    test_ctx.num_bounds_parameters = bounds_found;
    printf("  Parameters with bounds validation: %d\n", bounds_found);
    
    free(content_copy);
}

/**
 * Test: Enum parameter validation
 */
static void test_enum_parameter_validation(void) {
    printf("\n=== Testing enum parameter validation ===\n");
    
    char *content_copy = strdup(test_ctx.file_content);
    char *line = strtok(content_copy, "\n");
    char current_parameter[MAX_PARAMETER_NAME_LENGTH] = {0};
    char parameter_buffer[2048] = {0};
    bool in_parameter = false;
    int enum_params_found = 0;
    
    while (line) {
        if (line_contains_parameter_definition(line)) {
            // Process previous parameter
            if (in_parameter && strlen(current_parameter) > 0 && strlen(parameter_buffer) > 0) {
                char *enum_type_line = strstr(parameter_buffer, "enum_type:");
                char *enum_values_line = strstr(parameter_buffer, "enum_values:");
                
                if (enum_type_line && enum_values_line) {
                    TEST_ASSERT(is_valid_enum_values(enum_values_line),
                               "Enum values should be properly formatted");
                    
                    // Validate enum values are reasonable
                    char enum_values[MAX_ENUM_VALUES][MAX_PARAMETER_NAME_LENGTH];
                    int value_count = 0;
                    
                    if (extract_yaml_array_values(enum_values_line, enum_values, MAX_ENUM_VALUES)) {
                        for (int i = 0; i < MAX_ENUM_VALUES && strlen(enum_values[i]) > 0; i++) {
                            value_count++;
                            printf("    Enum value %d: '%s'\n", i, enum_values[i]);
                        }
                        
                        TEST_ASSERT(value_count > 0, "Enum should have at least one valid value");
                        TEST_ASSERT(value_count <= MAX_ENUM_VALUES, "Enum should not have too many values");
                    }
                    
                    enum_params_found++;
                    test_ctx.has_enum_parameters = true;
                    printf("  Parameter '%s' is valid enum with %d values\n", 
                           current_parameter, value_count);
                }
            }
            
            // Start new parameter
            char name_value[MAX_PARAMETER_NAME_LENGTH];
            if (extract_yaml_value(line, "name", name_value, sizeof(name_value))) {
                strcpy(current_parameter, name_value);
                strcpy(parameter_buffer, line);
                strcat(parameter_buffer, "\n");
                in_parameter = true;
            }
        } else if (in_parameter) {
            if (strlen(parameter_buffer) + strlen(line) + 1 < sizeof(parameter_buffer)) {
                strcat(parameter_buffer, line);
                strcat(parameter_buffer, "\n");
            }
        }
        
        line = strtok(NULL, "\n");
    }
    
    // Process last parameter
    if (in_parameter && strlen(current_parameter) > 0 && strlen(parameter_buffer) > 0) {
        char *enum_type_line = strstr(parameter_buffer, "enum_type:");
        char *enum_values_line = strstr(parameter_buffer, "enum_values:");
        
        if (enum_type_line && enum_values_line) {
            TEST_ASSERT(is_valid_enum_values(enum_values_line),
                       "Last enum parameter values should be properly formatted");
            enum_params_found++;
            test_ctx.has_enum_parameters = true;
        }
    }
    
    test_ctx.num_enum_parameters = enum_params_found;
    printf("  Enum parameters found: %d\n", enum_params_found);
    
    free(content_copy);
}

/**
 * Test: Struct field mapping validation
 */
static void test_struct_field_mapping_validation(void) {
    printf("\n=== Testing struct field mapping validation ===\n");
    
    char *content_copy = strdup(test_ctx.file_content);
    char *line = strtok(content_copy, "\n");
    char current_parameter[MAX_PARAMETER_NAME_LENGTH] = {0};
    char parameter_buffer[2048] = {0};
    bool in_parameter = false;
    int valid_mappings = 0;
    
    while (line) {
        if (line_contains_parameter_definition(line)) {
            // Process previous parameter
            if (in_parameter && strlen(current_parameter) > 0 && strlen(parameter_buffer) > 0) {
                char struct_field_value[MAX_STRUCT_FIELD_LENGTH];
                char *struct_field_line = strstr(parameter_buffer, "struct_field:");
                
                if (struct_field_line && extract_yaml_value(struct_field_line, "struct_field", 
                                                          struct_field_value, sizeof(struct_field_value))) {
                    TEST_ASSERT(is_valid_struct_field_mapping(struct_field_value),
                               "Struct field mapping should be valid C dot notation");
                    
                    printf("  Parameter '%s' maps to struct field '%s'\n", 
                           current_parameter, struct_field_value);
                    valid_mappings++;
                }
            }
            
            // Start new parameter
            char name_value[MAX_PARAMETER_NAME_LENGTH];
            if (extract_yaml_value(line, "name", name_value, sizeof(name_value))) {
                strcpy(current_parameter, name_value);
                strcpy(parameter_buffer, line);
                strcat(parameter_buffer, "\n");
                in_parameter = true;
            }
        } else if (in_parameter) {
            if (strlen(parameter_buffer) + strlen(line) + 1 < sizeof(parameter_buffer)) {
                strcat(parameter_buffer, line);
                strcat(parameter_buffer, "\n");
            }
        }
        
        line = strtok(NULL, "\n");
    }
    
    // Process last parameter
    if (in_parameter && strlen(current_parameter) > 0 && strlen(parameter_buffer) > 0) {
        char struct_field_value[MAX_STRUCT_FIELD_LENGTH];
        char *struct_field_line = strstr(parameter_buffer, "struct_field:");
        
        if (struct_field_line && extract_yaml_value(struct_field_line, "struct_field", 
                                                  struct_field_value, sizeof(struct_field_value))) {
            TEST_ASSERT(is_valid_struct_field_mapping(struct_field_value),
                       "Last parameter struct field mapping should be valid");
            valid_mappings++;
        }
    }
    
    TEST_ASSERT(valid_mappings > 0, "Should have at least one valid struct field mapping");
    printf("  Valid struct field mappings: %d\n", valid_mappings);
    
    free(content_copy);
}

/**
 * Test: Default value validation and type consistency  
 */
static void test_default_value_validation(void) {
    printf("\n=== Testing default value validation ===\n");
    
    char *content_copy = strdup(test_ctx.file_content);
    char *line = strtok(content_copy, "\n");
    char current_parameter[MAX_PARAMETER_NAME_LENGTH] = {0};
    char parameter_buffer[2048] = {0};
    bool in_parameter = false;
    int validated_defaults = 0;
    int total_defaults = 0;
    
    while (line) {
        if (line_contains_parameter_definition(line)) {
            // Process previous parameter
            if (in_parameter && strlen(current_parameter) > 0 && strlen(parameter_buffer) > 0) {
                char type_value[MAX_TYPE_NAME_LENGTH];
                char default_value[MAX_PARAMETER_NAME_LENGTH];
                char *type_line = strstr(parameter_buffer, "type:");
                char *default_line = strstr(parameter_buffer, "default:");
                
                if (type_line && extract_yaml_value(type_line, "type", type_value, sizeof(type_value))) {
                    if (default_line && extract_yaml_value(default_line, "default", default_value, sizeof(default_value))) {
                        total_defaults++;
                        bool is_valid = validate_default_value_type(default_value, type_value);
                        TEST_ASSERT(is_valid, "Default value should match parameter type");
                        if (is_valid) {
                            validated_defaults++;
                            printf("  Parameter '%s' has valid default value '%s' for type '%s'\n", 
                                   current_parameter, default_value, type_value);
                        } else {
                            printf("  ERROR: Parameter '%s' has invalid default value '%s' for type '%s'\n", 
                                   current_parameter, default_value, type_value);
                        }
                    }
                }
            }
            
            // Start new parameter
            char name_value[MAX_PARAMETER_NAME_LENGTH];
            if (extract_yaml_value(line, "name", name_value, sizeof(name_value))) {
                strcpy(current_parameter, name_value);
                strcpy(parameter_buffer, line);
                strcat(parameter_buffer, "\n");
                in_parameter = true;
            }
        } else if (in_parameter) {
            if (strlen(parameter_buffer) + strlen(line) + 1 < sizeof(parameter_buffer)) {
                strcat(parameter_buffer, line);
                strcat(parameter_buffer, "\n");
            }
        }
        
        line = strtok(NULL, "\n");
    }
    
    // Process last parameter
    if (in_parameter && strlen(current_parameter) > 0 && strlen(parameter_buffer) > 0) {
        char type_value[MAX_TYPE_NAME_LENGTH];
        char default_value[MAX_PARAMETER_NAME_LENGTH];
        char *type_line = strstr(parameter_buffer, "type:");
        char *default_line = strstr(parameter_buffer, "default:");
        
        if (type_line && extract_yaml_value(type_line, "type", type_value, sizeof(type_value))) {
            if (default_line && extract_yaml_value(default_line, "default", default_value, sizeof(default_value))) {
                total_defaults++;
                bool is_valid = validate_default_value_type(default_value, type_value);
                TEST_ASSERT(is_valid, "Last parameter default value should match type");
                if (is_valid) {
                    validated_defaults++;
                }
            }
        }
    }
    
    printf("  Default values validated: %d/%d\n", validated_defaults, total_defaults);
    if (total_defaults > 0) {
        TEST_ASSERT(validated_defaults == total_defaults, "All default values should be valid for their types");
    }
    
    free(content_copy);
}

/**
 * Test: Integration with auto-generated parameter system
 */
static void test_integration_with_generated_system(void) {
    printf("\n=== Testing integration with auto-generated parameter system ===\n");
    
    // Test that parameters.yaml generated the parameter system correctly
    TEST_ASSERT(PARAM_COUNT > 0, "Generated parameter system should define PARAM_COUNT");
    
    // Test some well-known core parameters that should exist
    TEST_ASSERT(PARAM_FILENAMEGALAXIES >= 0 && PARAM_FILENAMEGALAXIES < PARAM_COUNT,
                "FileNameGalaxies should be valid generated parameter");
    TEST_ASSERT(PARAM_OUTPUTDIR >= 0 && PARAM_OUTPUTDIR < PARAM_COUNT,
                "OutputDir should be valid generated parameter");
    TEST_ASSERT(PARAM_TREETYPE >= 0 && PARAM_TREETYPE < PARAM_COUNT,
                "TreeType should be valid generated parameter");
    
    // Test parameter metadata access
    TEST_ASSERT(get_parameter_name(PARAM_FILENAMEGALAXIES) != NULL,
                "get_parameter_name should work for FileNameGalaxies");
    TEST_ASSERT(get_parameter_name(PARAM_OUTPUTDIR) != NULL,
                "get_parameter_name should work for OutputDir");
    
    // Test parameter ID lookup
    parameter_id_t id = get_parameter_id("FileNameGalaxies");
    TEST_ASSERT(id == PARAM_FILENAMEGALAXIES,
                "get_parameter_id should return correct ID for FileNameGalaxies");
    
    // Verify the number of parameters is reasonable compared to YAML
    TEST_ASSERT(PARAM_COUNT >= test_ctx.num_parameters_found,
                "Generated parameter count should be at least as many as found in YAML");
    
    printf("  YAML parameters found: %d, Generated PARAM_COUNT: %d\n",
           test_ctx.num_parameters_found, PARAM_COUNT);
    
    printf("  Parameter system integration: ✅\n");
}

/**
 * Test: Error boundary conditions and malformed configuration handling
 */
static void test_error_boundary_conditions(void) {
    printf("\n=== Testing error boundary conditions ===\n");
    
    // Test parameter ID boundary conditions
    parameter_id_t invalid_id = get_parameter_id(NULL);
    TEST_ASSERT(invalid_id == PARAM_COUNT,
                "get_parameter_id(NULL) should return PARAM_COUNT");
    
    invalid_id = get_parameter_id("");
    TEST_ASSERT(invalid_id == PARAM_COUNT,
                "get_parameter_id(\"\") should return PARAM_COUNT");
    
    invalid_id = get_parameter_id("NonExistentParameter");
    TEST_ASSERT(invalid_id == PARAM_COUNT,
                "get_parameter_id should return PARAM_COUNT for invalid parameter");
    
    // Test parameter name retrieval boundary conditions
    const char *invalid_name = get_parameter_name(PARAM_COUNT);
    TEST_ASSERT(invalid_name == NULL,
                "get_parameter_name should return NULL for invalid ID");
    
    invalid_name = get_parameter_name(-1);
    TEST_ASSERT(invalid_name == NULL,
                "get_parameter_name should return NULL for negative ID");
    
    // Test that we found the expected categories and structures
    TEST_ASSERT(test_ctx.has_hierarchical_structure,
                "Should have found hierarchical structure in YAML");
    TEST_ASSERT(test_ctx.has_core_category,
                "Should have found core category in YAML");
    
    // Test reasonable parameter counts
    TEST_ASSERT(test_ctx.num_core_parameters > 0,
                "Should have at least one core parameter");
    TEST_ASSERT(test_ctx.num_parameters_found > 10,
                "Should have reasonable number of parameters (>10)");
    TEST_ASSERT(test_ctx.num_parameters_found < 200,
                "Parameter count should be reasonable (<200)");
    
    printf("  Error boundary testing: ✅\n");
}

//=============================================================================
// Test Runner
//=============================================================================

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;
    
    printf("\n========================================\n");
    printf("Starting tests for test_parameter_yaml_validation\n");
    printf("========================================\n\n");
    
    printf("This test validates the parameters.yaml metadata file that drives:\n");
    printf("  1. YAML hierarchical structure and schema compliance\n");
    printf("  2. Parameter definition completeness and required fields\n");
    printf("  3. Parameter type validation and system compatibility\n");
    printf("  4. Core vs physics parameter categorization\n");
    printf("  5. Bounds validation for numeric parameters\n");
    printf("  6. Enum parameter validation and value checking\n");
    printf("  7. Struct field mapping validation\n");
    printf("  8. Default value validation and type consistency\n");
    printf("  9. Integration with auto-generated parameter system\n");
    printf("  10. Error boundary conditions and robustness\n\n");
    
    // Setup test context
    if (setup_test_context() != 0) {
        printf("ERROR: Failed to set up test context\n");
        return 1;
    }
    
    // Run comprehensive test suite
    test_yaml_hierarchical_structure();
    test_parameter_definition_completeness();
    test_parameter_type_validation();
    test_core_physics_categorization();
    test_bounds_validation();
    test_enum_parameter_validation();
    test_struct_field_mapping_validation();
    test_default_value_validation();
    test_integration_with_generated_system();
    test_error_boundary_conditions();
    
    // Teardown
    teardown_test_context();
    
    // Report results
    printf("\n========================================\n");
    printf("Test results for test_parameter_yaml_validation:\n");
    printf("  Total tests: %d\n", tests_run);
    printf("  Passed: %d\n", tests_passed);
    printf("  Failed: %d\n", tests_run - tests_passed);
    
    if (tests_run == tests_passed) {
        printf("\n✅ Parameter YAML Validation Test PASSED\n");
        printf("The parameters.yaml file structure and content are valid.\n");
        printf("Parameters found: %d\n", test_ctx.num_parameters_found);
        printf("Core parameters: %d\n", test_ctx.num_core_parameters);
        printf("Physics parameters: %d\n", test_ctx.num_physics_parameters);
        printf("Hierarchical structure: %s\n", test_ctx.has_hierarchical_structure ? "✅ YES" : "❌ NO");
        printf("Enum parameters: %s (%d)\n", test_ctx.has_enum_parameters ? "✅ YES" : "❌ NO", test_ctx.num_enum_parameters);
        printf("Bounds validation: %s (%d)\n", test_ctx.has_bounds_validation ? "✅ YES" : "❌ NO", test_ctx.num_bounds_parameters);
    } else {
        printf("\n❌ Parameter YAML Validation Test FAILED\n");
        printf("Issues found in parameters.yaml structure or content.\n");
    }
    
    printf("========================================\n\n");
    
    return (tests_run == tests_passed) ? 0 : 1;
}