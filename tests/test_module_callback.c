#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../src/core/core_module_system.h"
#include "../src/core/core_module_callback.h"
#include "../src/core/core_logging.h"

/* Test module initialization function */
static int test_module_initialize(struct params *params, void **module_data) {
    /* Allocate some mock data */
    *module_data = malloc(sizeof(int));
    *((int *)*module_data) = 42;
    
    return MODULE_STATUS_SUCCESS;
}

/* Test module cleanup function */
static int test_module_cleanup(void *module_data) {
    /* Free the mock data */
    free(module_data);
    
    return MODULE_STATUS_SUCCESS;
}

/* Second test module initialization function */
static int test_module2_initialize(struct params *params, void **module_data) {
    /* Allocate some mock data */
    *module_data = malloc(sizeof(int));
    *((int *)*module_data) = 100;
    
    return MODULE_STATUS_SUCCESS;
}

/* Second test module cleanup function */
static int test_module2_cleanup(void *module_data) {
    /* Free the mock data */
    free(module_data);
    
    return MODULE_STATUS_SUCCESS;
}

/* First test module interface */
struct base_module test_module = {
    .name = "TestModule",
    .version = "1.0.0",
    .author = "Test Author",
    .type = MODULE_TYPE_COOLING,
    .initialize = test_module_initialize,
    .cleanup = test_module_cleanup
};

/* Second test module interface */
struct base_module test_module2 = {
    .name = "TestModule2",
    .version = "1.0.0",
    .author = "Test Author",
    .type = MODULE_TYPE_STAR_FORMATION,
    .initialize = test_module2_initialize,
    .cleanup = test_module2_cleanup
};

/* Test type definitions for callbacks */
typedef struct {
    int galaxy_index;
    double dt;
} cooling_args_t;

typedef struct {
    int value;
} context_t;

/* Test function to be registered */
static double test_calculate_cooling(void *args, void *context) {
    cooling_args_t *cooling_args = (cooling_args_t *)args;
    context_t *ctx = (context_t *)context;
    
    printf("Called test_calculate_cooling for galaxy %d with dt=%.2f\n", 
           cooling_args->galaxy_index, cooling_args->dt);
    
    /* Use the context if provided */
    if (context != NULL) {
        printf("Context value: %d\n", ctx->value);
    }
    
    return 0.5;  /* Mock cooling rate */
}

/* Test function for the second module */
static int test_form_stars(void *args, void *context) {
    int *galaxy_index = (int *)args;
    context_t *ctx = (context_t *)context;
    
    printf("Called test_form_stars for galaxy %d\n", *galaxy_index);
    
    /* Use the context if provided */
    if (context != NULL) {
        printf("Context value: %d\n", ctx->value);
    }
    
    return 10;  /* Mock number of stars formed */
}

int main(int argc, char *argv[]) {
    /* Initialize test */
    initialize_logging(NULL);
    
    printf("\n=== Module Callback System Test ===\n");
    
    /* Test module system initialization */
    int status = module_system_initialize();
    assert(status == MODULE_STATUS_SUCCESS);
    printf("Module system initialized successfully.\n");
    
    /* Register test modules */
    status = module_register(&test_module);
    assert(status == MODULE_STATUS_SUCCESS);
    assert(test_module.module_id >= 0);
    printf("First test module registered with ID %d.\n", test_module.module_id);
    
    status = module_register(&test_module2);
    assert(status == MODULE_STATUS_SUCCESS);
    assert(test_module2.module_id >= 0);
    printf("Second test module registered with ID %d.\n", test_module2.module_id);
    
    /* Initialize the modules */
    status = module_initialize(test_module.module_id, NULL);
    assert(status == MODULE_STATUS_SUCCESS);
    printf("First test module initialized.\n");
    
    status = module_initialize(test_module2.module_id, NULL);
    assert(status == MODULE_STATUS_SUCCESS);
    printf("Second test module initialized.\n");
    
    /* Activate the modules */
    status = module_set_active(test_module.module_id);
    assert(status == MODULE_STATUS_SUCCESS);
    printf("First test module activated.\n");
    
    status = module_set_active(test_module2.module_id);
    assert(status == MODULE_STATUS_SUCCESS);
    printf("Second test module activated.\n");
    
    /* Register functions with the modules */
    status = module_register_function(
        test_module.module_id,
        "calculate_cooling",
        (void *)test_calculate_cooling,
        FUNCTION_TYPE_DOUBLE,
        "double (cooling_args_t *, context_t *)",
        "Calculate cooling rate for a galaxy"
    );
    assert(status == MODULE_STATUS_SUCCESS);
    printf("Function registered with first test module.\n");
    
    status = module_register_function(
        test_module2.module_id,
        "form_stars",
        (void *)test_form_stars,
        FUNCTION_TYPE_INT,
        "int (int *, context_t *)",
        "Form stars in a galaxy"
    );
    assert(status == MODULE_STATUS_SUCCESS);
    printf("Function registered with second test module.\n");
    
    /* Declare dependencies between modules */
    status = module_declare_simple_dependency(
        test_module.module_id,
        MODULE_TYPE_STAR_FORMATION,
        NULL,  /* Any module of the correct type */
        false  /* Optional dependency */
    );
    assert(status == MODULE_STATUS_SUCCESS);
    printf("Simple dependency declared for first test module.\n");
    
    status = module_declare_simple_dependency(
        test_module2.module_id,
        MODULE_TYPE_COOLING,
        NULL,  /* Any module of the correct type */
        true   /* Required dependency */
    );
    assert(status == MODULE_STATUS_SUCCESS);
    printf("Simple dependency declared for second test module.\n");
    
    /* Test Phase 3: Call Stack Management functions */
    printf("\n=== Testing Phase 3: Call Stack Management ===\n");
    
    /* Test call stack operations */
    status = module_call_stack_push(0, test_module.module_id, "test_function", NULL);
    assert(status == MODULE_STATUS_SUCCESS);
    printf("Call frame pushed onto stack.\n");
    
    /* Check call depth */
    int depth = module_call_stack_get_depth();
    assert(depth == 1);
    printf("Call stack depth: %d\n", depth);
    
    /* Get the current frame */
    module_call_frame_t frame;
    status = module_call_stack_get_current_frame(&frame);
    assert(status == MODULE_STATUS_SUCCESS);
    printf("Current frame: caller=%d, callee=%d, function=%s\n", 
           frame.caller_module_id, frame.callee_module_id, frame.function_name);
    
    /* Get trace of the call stack */
    char trace_buffer[1024];
    status = module_call_stack_get_trace(trace_buffer, sizeof(trace_buffer));
    assert(status == MODULE_STATUS_SUCCESS);
    printf("Call stack trace:\n%s\n", trace_buffer);
    
    /* Test module call validation */
    status = module_call_validate(0, test_module.module_id);
    printf("Call validation result: %d\n", status);
    
    /* Push a second frame to test more complex stack operations */
    status = module_call_stack_push(test_module.module_id, test_module2.module_id, "another_function", NULL);
    assert(status == MODULE_STATUS_SUCCESS);
    printf("Second call frame pushed onto stack.\n");
    
    /* Find a module in the call stack */
    int position = module_call_stack_find_module(test_module.module_id, true);
    assert(position >= 0);
    printf("Found module %d as caller at position %d\n", test_module.module_id, position);
    
    /* Get trace with multiple frames */
    status = module_call_stack_get_trace(trace_buffer, sizeof(trace_buffer));
    assert(status == MODULE_STATUS_SUCCESS);
    printf("Updated call stack trace:\n%s\n", trace_buffer);
    
    /* Pop frames from the stack */
    status = module_call_stack_pop();
    assert(status == MODULE_STATUS_SUCCESS);
    printf("Second call frame popped from stack.\n");
    
    status = module_call_stack_pop();
    assert(status == MODULE_STATUS_SUCCESS);
    printf("First call frame popped from stack.\n");
    
    /* The stack should now be empty */
    depth = module_call_stack_get_depth();
    assert(depth == 0);
    printf("Call stack is now empty (depth: %d)\n", depth);
    
    /* Test Phase 4: Module Invocation functions */
    printf("\n=== Testing Phase 4: Module Invocation ===\n");
    
    /* Prepare arguments and context for invoke test */
    cooling_args_t cooling_args = {
        .galaxy_index = 42,
        .dt = 0.25
    };
    
    context_t context = {
        .value = 100
    };
    
    double cooling_result = 0.0;
    
    /* First we need to add a dependency for caller 0 to module 0 */
    status = module_declare_simple_dependency(
        0,  /* caller_id (use 0 for test harness) */
        MODULE_TYPE_COOLING,
        NULL,  /* Any module of the correct type */
        true  /* Required dependency */
    );
    assert(status == MODULE_STATUS_SUCCESS);
    printf("Added dependency from test harness to cooling module.\n");
    
    /* Test module_invoke by type */
    printf("Testing module_invoke by type...\n");
    status = module_invoke(
        0,  /* caller_id (use 0 for test harness) */
        MODULE_TYPE_COOLING,
        NULL,  /* Use active module of this type */
        "calculate_cooling",
        &context,
        &cooling_args,
        &cooling_result
    );
    assert(status == MODULE_STATUS_SUCCESS);
    printf("Module invoke result: status=%d, cooling_result=%.2f\n", status, cooling_result);
    
    /* Test module_invoke by name */
    printf("Testing module_invoke by name...\n");
    cooling_result = 0.0;
    status = module_invoke(
        0,  /* caller_id (use 0 for test harness) */
        MODULE_TYPE_UNKNOWN,
        "TestModule",  /* Use specific module by name */
        "calculate_cooling",
        &context,
        &cooling_args,
        &cooling_result
    );
    assert(status == MODULE_STATUS_SUCCESS);
    printf("Module invoke result: status=%d, cooling_result=%.2f\n", status, cooling_result);
    
    /* Test module_invoke with star formation module */
    printf("Testing module_invoke with star formation module...\n");
    int galaxy_index = 42;
    int stars_formed = 0;
    status = module_invoke(
        test_module.module_id,  /* caller_id */
        MODULE_TYPE_STAR_FORMATION,
        NULL,  /* Use active module of this type */
        "form_stars",
        &context,
        &galaxy_index,
        &stars_formed
    );
    assert(status == MODULE_STATUS_SUCCESS);
    printf("Module invoke result: status=%d, stars_formed=%d\n", status, stars_formed);
    
    /* Test invalid function name */
    printf("Testing module_invoke with invalid function name...\n");
    status = module_invoke(
        0,
        MODULE_TYPE_COOLING,
        NULL,
        "non_existent_function",
        NULL,
        NULL,
        NULL
    );
    assert(status != MODULE_STATUS_SUCCESS);
    printf("Module invoke with invalid function gave expected error: status=%d\n", status);
    
    /* Clean up */
    status = module_cleanup(test_module.module_id);
    assert(status == MODULE_STATUS_SUCCESS);
    printf("First test module cleaned up.\n");
    
    status = module_cleanup(test_module2.module_id);
    assert(status == MODULE_STATUS_SUCCESS);
    printf("Second test module cleaned up.\n");
    
    status = module_system_cleanup();
    assert(status == MODULE_STATUS_SUCCESS);
    printf("Module system cleaned up.\n");
    
    printf("\nAll tests passed!\n");
    cleanup_logging();
    
    return 0;
}