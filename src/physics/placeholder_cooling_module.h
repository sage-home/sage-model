/**
 * @file placeholder_cooling_module.h
 * @brief Header for placeholder cooling module
 */

#ifndef PLACEHOLDER_COOLING_MODULE_H
#define PLACEHOLDER_COOLING_MODULE_H

#include "../core/core_module_system.h"

/* External module definition */
extern struct base_module placeholder_cooling_module;

/* Declare the factory function */
struct base_module *placeholder_cooling_module_factory(void);

#endif // PLACEHOLDER_COOLING_MODULE_H
