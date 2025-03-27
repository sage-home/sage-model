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

/* Test module interface */
struct base_module test_module = {
    .name = "TestModule",
    .version = "1.0.0",
    .author = "Test Author",
    .type = MODULE_TYPE_COOLING,
    .initialize = test_module_initialize,
    .cleanup = test_module_cleanup
};

/* Test function to be registered */
static double test_calculate_cooling(int galaxy_index, double dt, void *context) {
    printf("Called test_calculate_cooling for galaxy %d with dt=%.2f\n", galaxy_index, dt);
    
    /* Use the context if provided */
    if (context != NULL) {
        printf("Context value: %d\n", *((int *)context));
    }
    
    return 0.5;  /* Mock cooling rate */
}

int main(int argc, char *argv[]) {
    /* Initialize test */
    initialize_logging(NULL);
    
    printf("\n=== Module Callback System Test ===\n");
    
    /* Test module system initialization */
    int status = module_system_initialize();
    assert(status == MODULE_STATUS_SUCCESS);
    printf("Module system initialized successfully.\n");
    
    /* Register a test module */
    status = module_register(&test_module);
    assert(status == MODULE_STATUS_SUCCESS);
    assert(test_module.module_id >= 0);
    printf("Test module registered with ID %d.\n", test_module.module_id);
    
    /* Initialize the module */
    status = module_initialize(test_module.module_id, NULL);
    assert(status == MODULE_STATUS_SUCCESS);
    printf("Test module initialized.\n");
    
    /* Register a function with the module */
    status = module_register_function(
        test_module.module_id,
        "calculate_cooling",
        (void *)test_calculate_cooling,
        FUNCTION_TYPE_DOUBLE,
        "double (int, double, void *)",
        "Calculate cooling rate for a galaxy"
    );
    assert(status == MODULE_STATUS_SUCCESS);
    printf("Function registered with test module.\n");
    
    /* Declare a simple dependency */
    status = module_declare_simple_dependency(
        test_module.module_id,
        MODULE_TYPE_STAR_FORMATION,
        NULL,  /* Any module of the correct type */
        false  /* Optional dependency */
    );
    assert(status == MODULE_STATUS_SUCCESS);
    printf("Simple dependency declared for test module.\n");
    
    /* Declare a dependency with version constraints */
    status = module_declare_dependency(
        test_module.module_id,
        MODULE_TYPE_COOLING,
        "CoolingModule",  /* Specific named module */
        true,  /* Required dependency */
        "1.2.0",  /* Minimum version */
        "2.0.0",  /* Maximum version */
        false  /* Not exact match */
    );
    assert(status == MODULE_STATUS_SUCCESS);
    printf("Versioned dependency declared for test module.\n");
    
    /* Test call stack */
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
    status = module_call_stack_push(test_module.module_id, 0, "another_function", NULL);
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
    
    /* Test circular dependency detection - this should not return true
       because even though the module appears twice in the stack, it's
       not in a circular pattern (once as caller, once as callee) */
    bool circular = module_call_stack_check_circular(test_module.module_id);
    printf("Circular dependency check: %s\n", circular ? "detected" : "not detected");
    
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
    
    /* Clean up */
    status = module_cleanup(test_module.module_id);
    assert(status == MODULE_STATUS_SUCCESS);
    printf("Test module cleaned up.\n");
    
    status = module_system_cleanup();
    assert(status == MODULE_STATUS_SUCCESS);
    printf("Module system cleaned up.\n");
    
    printf("\nAll tests passed!\n");
    cleanup_logging();
    
    return 0;
}