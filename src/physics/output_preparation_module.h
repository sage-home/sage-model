#ifndef OUTPUT_PREPARATION_MODULE_H
#define OUTPUT_PREPARATION_MODULE_H

#include "../core/core_allvars.h"
#include "../core/core_module_system.h"
#include "../core/core_pipeline_system.h"
#include "../core/core_properties.h"

// Function prototypes
extern int init_output_preparation_module(void);
extern int cleanup_output_preparation_module(void);
extern int register_output_preparation_module(struct module_pipeline *pipeline);

// Module function prototypes
extern int output_preparation_execute(void *module_data, struct pipeline_context *ctx);

#endif // OUTPUT_PREPARATION_MODULE_H