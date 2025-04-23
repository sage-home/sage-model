#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "core_allvars.h"
#include "core_pipeline_system.h"

/* Initialize the physics pipeline */
int physics_pipeline_initialize(void);

/* Execute a physics module step */
int physics_step_executor(
    struct pipeline_step *step,
    struct base_module *module,
    void *module_data,
    struct pipeline_context *context
);

/* Execute a function with pipeline callback tracking */
int pipeline_execute_with_callback(
    struct pipeline_context *context,
    int caller_id,
    int callee_id,
    const char *function_name,
    void *module_data,
    int (*func)(void *, struct pipeline_context *)
);

#ifdef __cplusplus
}
#endif