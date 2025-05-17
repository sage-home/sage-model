#pragma once

#include "core_module_system.h"
#include "core_pipeline_system.h"

// Module factory function type
typedef struct base_module* (*module_factory_fn)(void);

// Maximum number of module factories to register
#define MAX_MODULE_FACTORIES 32

// Register a module factory
int pipeline_register_module_factory(enum module_type type, const char *name, module_factory_fn factory);

// Register standard modules
void pipeline_register_standard_modules(void);

// Create default pipeline with standard modules
struct module_pipeline *pipeline_create_with_standard_modules(void);

// Enable or disable extension property usage
void pipeline_set_use_extensions(int enable);

// Export number_factories for testing purposes
extern int num_factories;
