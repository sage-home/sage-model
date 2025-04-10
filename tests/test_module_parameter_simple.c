#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

#define MAX_PARAM_NAME 64
#define MAX_PARAM_STRING 256
#define MAX_PARAM_DESCRIPTION 256
#define MAX_PARAM_UNITS 32

/* Test parameter types and struct definitions */
enum param_type {
    PARAM_TYPE_INT,
    PARAM_TYPE_FLOAT,
    PARAM_TYPE_DOUBLE,
    PARAM_TYPE_BOOL,
    PARAM_TYPE_STRING
};

/* Parameter error codes */
enum param_status {
    PARAM_SUCCESS = 0,
    PARAM_ERROR = -1,
    PARAM_NOT_FOUND = -2,
    PARAM_TYPE_MISMATCH = -3,
    PARAM_OUT_OF_BOUNDS = -4,
    PARAM_ALREADY_EXISTS = -5
};

/* Parameter structure */
typedef struct {
    char name[MAX_PARAM_NAME];
    enum param_type type;
    union {
        struct { int min; int max; } int_range;
        struct { float min; float max; } float_range;
        struct { double min; double max; } double_range;
    } limits;
    union {
        int int_val;
        float float_val;
        double double_val;
        bool bool_val;
        char string_val[MAX_PARAM_STRING];
    } value;
    bool has_limits;
    char description[MAX_PARAM_DESCRIPTION];
    char units[MAX_PARAM_UNITS];
    int module_id;
} param_t;

/* Parameter registry */
typedef struct {
    param_t *parameters;
    int num_parameters;
    int capacity;
} param_registry_t;

/* Initialize parameter registry */
int param_registry_init(param_registry_t *registry) {
    if (registry == NULL) {
        return PARAM_ERROR;
    }
    
    registry->parameters = malloc(10 * sizeof(param_t));
    if (registry->parameters == NULL) {
        return PARAM_ERROR;
    }
    
    registry->num_parameters = 0;
    registry->capacity = 10;
    
    return PARAM_SUCCESS;
}

/* Free parameter registry */
int param_registry_free(param_registry_t *registry) {
    if (registry == NULL) {
        return PARAM_ERROR;
    }
    
    if (registry->parameters != NULL) {
        free(registry->parameters);
        registry->parameters = NULL;
    }
    
    registry->num_parameters = 0;
    registry->capacity = 0;
    
    return PARAM_SUCCESS;
}

/* Create an integer parameter */
int create_int_param(param_t *param, const char *name, int value, int min, int max, 
                   const char *description, const char *units, int module_id) {
    if (param == NULL || name == NULL) {
        return PARAM_ERROR;
    }
    
    memset(param, 0, sizeof(param_t));
    
    strncpy(param->name, name, MAX_PARAM_NAME - 1);
    param->type = PARAM_TYPE_INT;
    param->value.int_val = value;
    param->module_id = module_id;
    
    if (min != max) {
        param->has_limits = true;
        param->limits.int_range.min = min;
        param->limits.int_range.max = max;
    }
    
    if (description != NULL) {
        strncpy(param->description, description, MAX_PARAM_DESCRIPTION - 1);
    }
    
    if (units != NULL) {
        strncpy(param->units, units, MAX_PARAM_UNITS - 1);
    }
    
    return PARAM_SUCCESS;
}

/* Register a parameter */
int register_param(param_registry_t *registry, const param_t *param) {
    if (registry == NULL || param == NULL) {
        return PARAM_ERROR;
    }
    
    /* Check for existing parameter */
    for (int i = 0; i < registry->num_parameters; i++) {
        if (strcmp(registry->parameters[i].name, param->name) == 0 &&
            registry->parameters[i].module_id == param->module_id) {
            return PARAM_ALREADY_EXISTS;
        }
    }
    
    /* Check if we need to resize */
    if (registry->num_parameters >= registry->capacity) {
        int new_capacity = registry->capacity * 2;
        param_t *new_params = realloc(registry->parameters, new_capacity * sizeof(param_t));
        if (new_params == NULL) {
            return PARAM_ERROR;
        }
        
        registry->parameters = new_params;
        registry->capacity = new_capacity;
    }
    
    /* Add the parameter */
    registry->parameters[registry->num_parameters] = *param;
    registry->num_parameters++;
    
    return PARAM_SUCCESS;
}

/* Find a parameter */
int find_param(param_registry_t *registry, const char *name, int module_id) {
    if (registry == NULL || name == NULL) {
        return PARAM_ERROR;
    }
    
    for (int i = 0; i < registry->num_parameters; i++) {
        if (strcmp(registry->parameters[i].name, name) == 0 &&
            registry->parameters[i].module_id == module_id) {
            return i;
        }
    }
    
    return PARAM_NOT_FOUND;
}

/* Get an integer parameter */
int get_int_param(param_registry_t *registry, const char *name, int module_id, int *value) {
    if (registry == NULL || name == NULL || value == NULL) {
        return PARAM_ERROR;
    }
    
    int index = find_param(registry, name, module_id);
    if (index < 0) {
        return index;
    }
    
    param_t *param = &registry->parameters[index];
    if (param->type != PARAM_TYPE_INT) {
        return PARAM_TYPE_MISMATCH;
    }
    
    *value = param->value.int_val;
    return PARAM_SUCCESS;
}

/* Set an integer parameter */
int set_int_param(param_registry_t *registry, const char *name, int module_id, int value) {
    if (registry == NULL || name == NULL) {
        return PARAM_ERROR;
    }
    
    int index = find_param(registry, name, module_id);
    if (index < 0) {
        return index;
    }
    
    param_t *param = &registry->parameters[index];
    if (param->type != PARAM_TYPE_INT) {
        return PARAM_TYPE_MISMATCH;
    }
    
    /* Check bounds */
    if (param->has_limits) {
        if (value < param->limits.int_range.min || value > param->limits.int_range.max) {
            return PARAM_OUT_OF_BOUNDS;
        }
    }
    
    param->value.int_val = value;
    return PARAM_SUCCESS;
}

/* Main test function */
int main(void) {
    printf("Running simplified parameter tests...\n");

    /* Test parameter registry */
    param_registry_t registry;
    int status = param_registry_init(&registry);
    assert(status == PARAM_SUCCESS);
    assert(registry.num_parameters == 0);
    assert(registry.capacity > 0);
    
    /* Create and register parameter */
    param_t param;
    status = create_int_param(&param, "test_param", 42, 0, 100, "Test parameter", NULL, 1);
    assert(status == PARAM_SUCCESS);
    assert(param.type == PARAM_TYPE_INT);
    assert(param.value.int_val == 42);
    assert(param.has_limits == true);
    assert(param.limits.int_range.min == 0);
    assert(param.limits.int_range.max == 100);
    
    status = register_param(&registry, &param);
    assert(status == PARAM_SUCCESS);
    assert(registry.num_parameters == 1);
    
    /* Test parameter lookup */
    int index = find_param(&registry, "test_param", 1);
    assert(index >= 0);
    
    index = find_param(&registry, "nonexistent", 1);
    assert(index == PARAM_NOT_FOUND);
    
    /* Test parameter retrieval */
    int value;
    status = get_int_param(&registry, "test_param", 1, &value);
    assert(status == PARAM_SUCCESS);
    assert(value == 42);
    
    /* Test parameter setting */
    status = set_int_param(&registry, "test_param", 1, 50);
    assert(status == PARAM_SUCCESS);
    
    status = get_int_param(&registry, "test_param", 1, &value);
    assert(status == PARAM_SUCCESS);
    assert(value == 50);
    
    /* Test bounds checking */
    status = set_int_param(&registry, "test_param", 1, 200);
    assert(status == PARAM_OUT_OF_BOUNDS);
    
    /* Cleanup */
    status = param_registry_free(&registry);
    assert(status == PARAM_SUCCESS);
    
    printf("All simplified parameter tests passed!\n");
    return 0;
}
