/**
 * @file test_module_system.h
 * @brief Helper functions for module system testing
 * 
 * This header file contains helper functions and definitions for testing
 * the module system, including error propagation, recovery, and performance.
 */

#ifndef TEST_MODULE_SYSTEM_H
#define TEST_MODULE_SYSTEM_H

#include <stdbool.h>

// Helper for multi-error test
void force_error_in_module(const char *module_name, int error_code);
bool invoke_multi_branch_chain(void);

// Helper for recovery test
void setup_modules_with_recovery(void);
#define FALLBACK_RESULT 42

// Helper for performance test
void setup_many_modules(int count);
void create_deep_dependency_chain(int depth);
bool execute_deep_dependency_chain(void);
double get_wall_time(void);
#define MAX_ACCEPTABLE_TIME 0.1  // 100ms

#endif /* TEST_MODULE_SYSTEM_H */
