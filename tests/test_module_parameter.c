#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

/* Using standalone test header to avoid circular dependency issues */
#include "test_module_parameter_standalone.h"

/* Utility function to assert with a message */
#define assert_msg(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "Assertion failed: %s\n  Message: %s\n  File: %s, Line: %d\n", \
                #cond, msg, __FILE__, __LINE__); \
        abort(); \
    } \
} while (0)

/* Test parameter registry initialization and cleanup */
void test_registry_init_free(void) {
    printf("Testing parameter registry initialization and cleanup... ");
    
    module_parameter_registry_t registry;
    int status = module_parameter_registry_init(&registry);
    assert_msg(status == MODULE_PARAM_SUCCESS, "Failed to initialize parameter registry");
    assert_msg(registry.num_parameters == 0, "Registry should start with 0 parameters");
    assert_msg(registry.capacity > 0, "Registry should have capacity");
    assert_msg(registry.parameters != NULL, "Parameters array should be allocated");
    
    status = module_parameter_registry_free(&registry);
    assert_msg(status == MODULE_PARAM_SUCCESS, "Failed to free parameter registry");
    assert_msg(registry.parameters == NULL, "Parameters array should be NULL after free");
    assert_msg(registry.num_parameters == 0, "Registry should have 0 parameters after free");
    assert_msg(registry.capacity == 0, "Registry capacity should be 0 after free");
    
    printf("OK\n");
}

/* Test parameter registration */
void test_parameter_registration(void) {
    printf("Testing parameter registration... ");
    
    module_parameter_registry_t registry;
    module_parameter_registry_init(&registry);
    
    /* Create test parameters of different types */
    module_parameter_t int_param;
    module_create_parameter_int(&int_param, "test_int", 42, 0, 100, "Test integer parameter", "units", 1);
    
    module_parameter_t float_param;
    module_create_parameter_float(&float_param, "test_float", 3.14f, 0.0f, 10.0f, "Test float parameter", "units", 1);
    
    module_parameter_t double_param;
    module_create_parameter_double(&double_param, "test_double", 2.71828, 0.0, 10.0, "Test double parameter", "units", 1);
    
    module_parameter_t bool_param;
    module_create_parameter_bool(&bool_param, "test_bool", true, "Test boolean parameter", 1);
    
    module_parameter_t string_param;
    module_create_parameter_string(&string_param, "test_string", "hello world", "Test string parameter", 1);
    
    /* Register parameters */
    int status = module_register_parameter(&registry, &int_param);
    assert_msg(status == MODULE_PARAM_SUCCESS, "Failed to register int parameter");
    
    status = module_register_parameter(&registry, &float_param);
    assert_msg(status == MODULE_PARAM_SUCCESS, "Failed to register float parameter");
    
    status = module_register_parameter(&registry, &double_param);
    assert_msg(status == MODULE_PARAM_SUCCESS, "Failed to register double parameter");
    
    status = module_register_parameter(&registry, &bool_param);
    assert_msg(status == MODULE_PARAM_SUCCESS, "Failed to register bool parameter");
    
    status = module_register_parameter(&registry, &string_param);
    assert_msg(status == MODULE_PARAM_SUCCESS, "Failed to register string parameter");
    
    /* Verify all parameters were registered */
    assert_msg(registry.num_parameters == 5, "Registry should have 5 parameters");
    
    /* Verify parameter lookup works */
    int index = module_find_parameter(&registry, "test_int", 1);
    assert_msg(index >= 0, "Parameter test_int not found");
    
    index = module_find_parameter(&registry, "test_float", 1);
    assert_msg(index >= 0, "Parameter test_float not found");
    
    index = module_find_parameter(&registry, "test_double", 1);
    assert_msg(index >= 0, "Parameter test_double not found");
    
    index = module_find_parameter(&registry, "test_bool", 1);
    assert_msg(index >= 0, "Parameter test_bool not found");
    
    index = module_find_parameter(&registry, "test_string", 1);
    assert_msg(index >= 0, "Parameter test_string not found");
    
    /* Verify non-existent parameter */
    index = module_find_parameter(&registry, "nonexistent", 1);
    assert_msg(index == MODULE_PARAM_NOT_FOUND, "Non-existent parameter should not be found");
    
    /* Test parameter duplication detection */
    status = module_register_parameter(&registry, &int_param);
    assert_msg(status == MODULE_PARAM_ALREADY_EXISTS, "Duplicate parameter should be detected");
    
    /* Verify parameter count unchanged */
    assert_msg(registry.num_parameters == 5, "Registry should still have 5 parameters");
    
    /* Clean up */
    module_parameter_registry_free(&registry);
    
    printf("OK\n");
}

/* Test parameter retrieval */
void test_parameter_retrieval(void) {
    printf("Testing parameter retrieval... ");
    
    module_parameter_registry_t registry;
    module_parameter_registry_init(&registry);
    
    /* Create and register test parameters */
    module_parameter_t int_param;
    module_create_parameter_int(&int_param, "test_int", 42, 0, 100, "Test integer parameter", "units", 1);
    module_register_parameter(&registry, &int_param);
    
    module_parameter_t float_param;
    module_create_parameter_float(&float_param, "test_float", 3.14f, 0.0f, 10.0f, "Test float parameter", "units", 1);
    module_register_parameter(&registry, &float_param);
    
    module_parameter_t double_param;
    module_create_parameter_double(&double_param, "test_double", 2.71828, 0.0, 10.0, "Test double parameter", "units", 1);
    module_register_parameter(&registry, &double_param);
    
    module_parameter_t bool_param;
    module_create_parameter_bool(&bool_param, "test_bool", true, "Test boolean parameter", 1);
    module_register_parameter(&registry, &bool_param);
    
    module_parameter_t string_param;
    module_create_parameter_string(&string_param, "test_string", "hello world", "Test string parameter", 1);
    module_register_parameter(&registry, &string_param);
    
    /* Test type-specific retrieval */
    int int_value;
    int status = module_get_parameter_int(&registry, "test_int", 1, &int_value);
    assert_msg(status == MODULE_PARAM_SUCCESS, "Failed to get int parameter");
    assert_msg(int_value == 42, "Retrieved int value doesn't match");
    
    float float_value;
    status = module_get_parameter_float(&registry, "test_float", 1, &float_value);
    assert_msg(status == MODULE_PARAM_SUCCESS, "Failed to get float parameter");
    assert_msg(float_value == 3.14f, "Retrieved float value doesn't match");
    
    double double_value;
    status = module_get_parameter_double(&registry, "test_double", 1, &double_value);
    assert_msg(status == MODULE_PARAM_SUCCESS, "Failed to get double parameter");
    assert_msg(double_value == 2.71828, "Retrieved double value doesn't match");
    
    bool bool_value;
    status = module_get_parameter_bool(&registry, "test_bool", 1, &bool_value);
    assert_msg(status == MODULE_PARAM_SUCCESS, "Failed to get bool parameter");
    assert_msg(bool_value == true, "Retrieved bool value doesn't match");
    
    char string_value[MAX_PARAM_STRING];
    status = module_get_parameter_string(&registry, "test_string", 1, string_value, MAX_PARAM_STRING);
    assert_msg(status == MODULE_PARAM_SUCCESS, "Failed to get string parameter");
    assert_msg(strcmp(string_value, "hello world") == 0, "Retrieved string value doesn't match");
    
    /* Test type mismatch handling */
    status = module_get_parameter_int(&registry, "test_float", 1, &int_value);
    assert_msg(status == MODULE_PARAM_TYPE_MISMATCH, "Type mismatch should be detected");
    
    status = module_get_parameter_float(&registry, "test_int", 1, &float_value);
    assert_msg(status == MODULE_PARAM_TYPE_MISMATCH, "Type mismatch should be detected");
    
    /* Test non-existent parameter */
    status = module_get_parameter_int(&registry, "nonexistent", 1, &int_value);
    assert_msg(status == MODULE_PARAM_NOT_FOUND, "Nonexistent parameter should return NOT_FOUND");
    
    /* Test generic parameter retrieval */
    module_parameter_t param;
    status = module_get_parameter(&registry, "test_int", 1, &param);
    assert_msg(status == MODULE_PARAM_SUCCESS, "Failed to get parameter");
    assert_msg(param.type == MODULE_PARAM_TYPE_INT, "Retrieved parameter type doesn't match");
    assert_msg(param.value.int_val == 42, "Retrieved parameter value doesn't match");
    assert_msg(strcmp(param.name, "test_int") == 0, "Retrieved parameter name doesn't match");
    
    /* Clean up */
    module_parameter_registry_free(&registry);
    
    printf("OK\n");
}

/* Test parameter setting */
void test_parameter_setting(void) {
    printf("Testing parameter setting... ");
    
    module_parameter_registry_t registry;
    module_parameter_registry_init(&registry);
    
    /* Create and register test parameters */
    module_parameter_t int_param;
    module_create_parameter_int(&int_param, "test_int", 42, 0, 100, "Test integer parameter", "units", 1);
    module_register_parameter(&registry, &int_param);
    
    module_parameter_t float_param;
    module_create_parameter_float(&float_param, "test_float", 3.14f, 0.0f, 10.0f, "Test float parameter", "units", 1);
    module_register_parameter(&registry, &float_param);
    
    module_parameter_t double_param;
    module_create_parameter_double(&double_param, "test_double", 2.71828, 0.0, 10.0, "Test double parameter", "units", 1);
    module_register_parameter(&registry, &double_param);
    
    module_parameter_t bool_param;
    module_create_parameter_bool(&bool_param, "test_bool", true, "Test boolean parameter", 1);
    module_register_parameter(&registry, &bool_param);
    
    module_parameter_t string_param;
    module_create_parameter_string(&string_param, "test_string", "hello world", "Test string parameter", 1);
    module_register_parameter(&registry, &string_param);
    
    /* Test type-specific setting */
    int status = module_set_parameter_int(&registry, "test_int", 1, 84);
    assert_msg(status == MODULE_PARAM_SUCCESS, "Failed to set int parameter");
    
    int int_value;
    module_get_parameter_int(&registry, "test_int", 1, &int_value);
    assert_msg(int_value == 84, "Set int value doesn't match");
    
    status = module_set_parameter_float(&registry, "test_float", 1, 6.28f);
    assert_msg(status == MODULE_PARAM_SUCCESS, "Failed to set float parameter");
    
    float float_value;
    module_get_parameter_float(&registry, "test_float", 1, &float_value);
    assert_msg(float_value == 6.28f, "Set float value doesn't match");
    
    status = module_set_parameter_double(&registry, "test_double", 1, 3.14159);
    assert_msg(status == MODULE_PARAM_SUCCESS, "Failed to set double parameter");
    
    double double_value;
    module_get_parameter_double(&registry, "test_double", 1, &double_value);
    assert_msg(double_value == 3.14159, "Set double value doesn't match");
    
    status = module_set_parameter_bool(&registry, "test_bool", 1, false);
    assert_msg(status == MODULE_PARAM_SUCCESS, "Failed to set bool parameter");
    
    bool bool_value;
    module_get_parameter_bool(&registry, "test_bool", 1, &bool_value);
    assert_msg(bool_value == false, "Set bool value doesn't match");
    
    status = module_set_parameter_string(&registry, "test_string", 1, "changed value");
    assert_msg(status == MODULE_PARAM_SUCCESS, "Failed to set string parameter");
    
    char string_value[MAX_PARAM_STRING];
    module_get_parameter_string(&registry, "test_string", 1, string_value, MAX_PARAM_STRING);
    assert_msg(strcmp(string_value, "changed value") == 0, "Set string value doesn't match");
    
    /* Test type mismatch handling */
    status = module_set_parameter_int(&registry, "test_float", 1, 42);
    assert_msg(status == MODULE_PARAM_TYPE_MISMATCH, "Type mismatch should be detected");
    
    /* Test bounds checking */
    status = module_set_parameter_int(&registry, "test_int", 1, 200);
    assert_msg(status == MODULE_PARAM_OUT_OF_BOUNDS, "Out of bounds value should be detected");
    
    /* Test non-existent parameter */
    status = module_set_parameter_int(&registry, "nonexistent", 1, 42);
    assert_msg(status == MODULE_PARAM_NOT_FOUND, "Nonexistent parameter should cause error");
    
    /* Clean up */
    module_parameter_registry_free(&registry);
    
    printf("OK\n");
}

/* Test parameter validation */
void test_parameter_validation(void) {
    printf("Testing parameter validation... ");
    
    /* Create valid parameter */
    module_parameter_t valid_param;
    module_create_parameter_int(&valid_param, "valid_param", 42, 0, 100, "Valid parameter", "units", 1);
    
    /* Test valid parameter */
    bool result = module_validate_parameter(&valid_param);
    assert_msg(result == true, "Valid parameter should validate");
    
    /* Test valid parameter bounds */
    result = module_check_parameter_bounds(&valid_param);
    assert_msg(result == true, "Valid parameter should be within bounds");
    
    /* Test invalid bounds */
    module_parameter_t invalid_bounds;
    module_create_parameter_int(&invalid_bounds, "invalid_bounds", 42, 100, 0, "Invalid bounds parameter", "units", 1);
    
    result = module_validate_parameter(&invalid_bounds);
    assert_msg(result == false, "Parameter with invalid bounds should not validate");
    
    /* Test out of bounds value */
    module_parameter_t out_of_bounds;
    module_create_parameter_int(&out_of_bounds, "out_of_bounds", 200, 0, 100, "Out of bounds parameter", "units", 1);
    
    result = module_check_parameter_bounds(&out_of_bounds);
    assert_msg(result == false, "Parameter with out of bounds value should fail bounds check");
    
    printf("OK\n");
}

/* Test parameter import/export functions */
void test_parameter_import_export(void) {
    printf("Testing parameter import/export... ");
    
    /* Create a parameter registry with some test parameters */
    module_parameter_registry_t registry;
    module_parameter_registry_init(&registry);
    
    /* Create and register test parameters */
    module_parameter_t int_param;
    module_create_parameter_int(&int_param, "test_int", 42, 0, 100, "Test integer parameter", "units", 1);
    module_register_parameter(&registry, &int_param);
    
    module_parameter_t float_param;
    module_create_parameter_float(&float_param, "test_float", 3.14f, 0.0f, 10.0f, "Test float parameter", "units", 1);
    module_register_parameter(&registry, &float_param);
    
    module_parameter_t double_param;
    module_create_parameter_double(&double_param, "test_double", 2.71828, 0.0, 10.0, "Test double parameter", "units", 1);
    module_register_parameter(&registry, &double_param);
    
    module_parameter_t bool_param;
    module_create_parameter_bool(&bool_param, "test_bool", true, "Test boolean parameter", 1);
    module_register_parameter(&registry, &bool_param);
    
    module_parameter_t string_param;
    module_create_parameter_string(&string_param, "test_string", "hello world", "Test string parameter", 1);
    module_register_parameter(&registry, &string_param);
    
    /* Save parameters to file */
    char temp_filename[] = "test_parameters.json";
    int status = module_save_parameters_to_file(&registry, temp_filename);
    assert_msg(status == MODULE_PARAM_SUCCESS, "Failed to save parameters to file");
    
    /* Clean up the original registry */
    module_parameter_registry_free(&registry);
    
    /* Create a new registry */
    module_parameter_registry_init(&registry);
    
    /* Load parameters from file */
    status = module_load_parameters_from_file(&registry, temp_filename);
    assert_msg(status == MODULE_PARAM_SUCCESS, "Failed to load parameters from file");
    
    /* Verify loaded parameters */
    assert_msg(registry.num_parameters == 5, "Registry should have 5 parameters after loading");
    
    int int_value;
    status = module_get_parameter_int(&registry, "test_int", 1, &int_value);
    assert_msg(status == MODULE_PARAM_SUCCESS, "Failed to get loaded int parameter");
    assert_msg(int_value == 42, "Loaded int value doesn't match");
    
    float float_value;
    status = module_get_parameter_float(&registry, "test_float", 1, &float_value);
    assert_msg(status == MODULE_PARAM_SUCCESS, "Failed to get loaded float parameter");
    assert_msg(float_value == 3.14f, "Loaded float value doesn't match");
    
    double double_value;
    status = module_get_parameter_double(&registry, "test_double", 1, &double_value);
    assert_msg(status == MODULE_PARAM_SUCCESS, "Failed to get loaded double parameter");
    assert_msg(double_value == 2.71828, "Loaded double value doesn't match");
    
    bool bool_value;
    status = module_get_parameter_bool(&registry, "test_bool", 1, &bool_value);
    assert_msg(status == MODULE_PARAM_SUCCESS, "Failed to get loaded bool parameter");
    assert_msg(bool_value == true, "Loaded bool value doesn't match");
    
    char string_value[MAX_PARAM_STRING];
    status = module_get_parameter_string(&registry, "test_string", 1, string_value, MAX_PARAM_STRING);
    assert_msg(status == MODULE_PARAM_SUCCESS, "Failed to get loaded string parameter");
    assert_msg(strcmp(string_value, "hello world") == 0, "Loaded string value doesn't match");
    
    /* Clean up */
    module_parameter_registry_free(&registry);
    remove(temp_filename);  /* Delete the temporary file */
    
    printf("OK\n");
}

/* Main test function */
int main(void) {
    printf("Running parameter system tests...\n");
    
    test_registry_init_free();
    test_parameter_registration();
    test_parameter_retrieval();
    test_parameter_setting();
    test_parameter_validation();
    test_parameter_import_export();
    
    printf("All parameter system tests passed!\n");
    return 0;
}
