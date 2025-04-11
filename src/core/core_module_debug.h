#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include "core_module_system.h"
#include "core_module_error.h"

/**
 * @file core_module_debug.h
 * @brief Module debugging utilities for SAGE
 * 
 * This file provides utilities for debugging module implementations,
 * including function tracing, state inspection, and diagnostic tools.
 */

/* Maximum length of a trace log entry */
#define MAX_TRACE_ENTRY_LENGTH 256

/* Maximum number of trace entries to keep */
#define MAX_TRACE_ENTRIES 1000

/**
 * Trace entry severity levels
 */
typedef enum {
    TRACE_LEVEL_DEBUG = 0,   /* Detailed debug information */
    TRACE_LEVEL_INFO = 1,    /* Informational messages */
    TRACE_LEVEL_WARNING = 2, /* Warning conditions */
    TRACE_LEVEL_ERROR = 3,   /* Error conditions */
    TRACE_LEVEL_FATAL = 4    /* Fatal conditions */
} trace_level_t;

/**
 * Trace entry structure
 * 
 * Represents a single entry in the module trace log
 */
typedef struct {
    trace_level_t level;                  /* Entry severity level */
    char message[MAX_TRACE_ENTRY_LENGTH]; /* Entry message */
    int module_id;                        /* ID of the module that generated the entry */
    double timestamp;                     /* Time when the entry was created */
    const char *function;                 /* Function that generated the entry */
    const char *file;                     /* File where the entry was generated */
    int line;                             /* Line number where the entry was generated */
} module_trace_entry_t;

/**
 * Module trace configuration
 * 
 * Controls the behavior of the module tracing system
 */
typedef struct {
    bool enabled;                /* Whether tracing is enabled */
    trace_level_t min_level;     /* Minimum level to log */
    bool log_to_console;         /* Whether to log to console */
    bool log_to_file;            /* Whether to log to file */
    char log_file[PATH_MAX];     /* Path to log file */
    bool circular_buffer;        /* Whether to use a circular buffer */
    int buffer_size;             /* Size of the trace buffer */
} module_trace_config_t;

/**
 * Module debug context structure
 * 
 * Manages debug state and configuration for a module
 */
struct module_debug_context {
    bool tracing_enabled;                /* Whether tracing is enabled for this module */
    trace_level_t min_trace_level;       /* Minimum level to trace */
    int module_id;                       /* ID of the module */
    struct module_error_context *error_context; /* Reference to the module's error context */
    
    /* Trace buffer */
    module_trace_entry_t *trace_entries; /* Array of trace entries */
    int trace_count;                     /* Total traces recorded */
    int current_trace_index;             /* Current position in trace buffer */
    bool trace_overflow;                 /* Whether trace buffer has overflowed */
};

/**
 * Initialize the module debugging system
 * 
 * Sets up the module trace system with the given configuration.
 * 
 * @param config Trace configuration
 * @return true on success, false on failure
 */
bool module_debug_init(const module_trace_config_t *config);

/**
 * Clean up the module debugging system
 * 
 * Releases resources used by the module trace system.
 * 
 * @return true on success, false on failure
 */
bool module_debug_cleanup(void);

/**
 * Add an entry to the module trace log
 * 
 * @param level Entry severity level
 * @param module_id ID of the module generating the entry
 * @param function Function generating the entry
 * @param file File where the entry is generated
 * @param line Line number where the entry is generated
 * @param format Format string for the message
 * @param ... Arguments for the format string
 * @return true on success, false on failure
 */
bool module_trace_log(
    trace_level_t level,
    int module_id,
    const char *function,
    const char *file,
    int line,
    const char *format,
    ...
);

/**
 * Get the module trace log
 * 
 * Retrieves the current trace log entries.
 * 
 * @param entries Output buffer for trace entries
 * @param max_entries Maximum number of entries to retrieve
 * @param num_entries Output pointer for the number of entries retrieved
 * @return true on success, false on failure
 */
bool module_trace_get_log(
    module_trace_entry_t *entries,
    int max_entries,
    int *num_entries
);

/**
 * Clear the module trace log
 * 
 * Removes all entries from the trace log.
 * 
 * @return true on success, false on failure
 */
bool module_trace_clear_log(void);

/**
 * Format a trace entry as text
 * 
 * @param entry Trace entry to format
 * @param output Output buffer for the formatted text
 * @param size Size of the output buffer
 * @return true on success, false on failure
 */
bool module_trace_format_entry(
    const module_trace_entry_t *entry,
    char *output,
    size_t size
);

/**
 * Write the module trace log to a file
 * 
 * @param filename Path to the output file
 * @return true on success, false on failure
 */
bool module_trace_write_to_file(const char *filename);

/**
 * Set the minimum trace level
 * 
 * Only entries with a level greater than or equal to the minimum
 * level will be logged.
 * 
 * @param level Minimum trace level
 * @return true on success, false on failure
 */
bool module_trace_set_min_level(trace_level_t level);

/**
 * Enable or disable module tracing
 * 
 * @param enabled Whether to enable tracing
 * @return true on success, false on failure
 */
bool module_trace_set_enabled(bool enabled);

/**
 * Check if module tracing is enabled
 * 
 * @return true if tracing is enabled, false otherwise
 */
bool module_trace_is_enabled(void);

/**
 * Convenience macros for tracing
 * 
 * These macros automatically include the current function, file, and line
 * information in the trace entry.
 */
#define MODULE_TRACE_DEBUG(module_id, format, ...) \
    module_trace_log(TRACE_LEVEL_DEBUG, module_id, __func__, __FILE__, __LINE__, format, ##__VA_ARGS__)

#define MODULE_TRACE_INFO(module_id, format, ...) \
    module_trace_log(TRACE_LEVEL_INFO, module_id, __func__, __FILE__, __LINE__, format, ##__VA_ARGS__)

#define MODULE_TRACE_WARNING(module_id, format, ...) \
    module_trace_log(TRACE_LEVEL_WARNING, module_id, __func__, __FILE__, __LINE__, format, ##__VA_ARGS__)

#define MODULE_TRACE_ERROR(module_id, format, ...) \
    module_trace_log(TRACE_LEVEL_ERROR, module_id, __func__, __FILE__, __LINE__, format, ##__VA_ARGS__)

#define MODULE_TRACE_FATAL(module_id, format, ...) \
    module_trace_log(TRACE_LEVEL_FATAL, module_id, __func__, __FILE__, __LINE__, format, ##__VA_ARGS__)

/* Function tracing macros */
#define MODULE_TRACE_ENTER(module_id) \
    module_trace_log(TRACE_LEVEL_DEBUG, module_id, __func__, __FILE__, __LINE__, "Entering %s", __func__)

#define MODULE_TRACE_EXIT(module_id) \
    module_trace_log(TRACE_LEVEL_DEBUG, module_id, __func__, __FILE__, __LINE__, "Exiting %s", __func__)

#define MODULE_TRACE_EXIT_STATUS(module_id, status) \
    module_trace_log(TRACE_LEVEL_DEBUG, module_id, __func__, __FILE__, __LINE__, "Exiting %s with status %d", __func__, status)

/**
 * Initialize a module debug context
 * 
 * Allocates and initializes a debug context structure for a module.
 * 
 * @param module Pointer to the module interface
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
int module_debug_context_init(struct base_module *module);

/**
 * Clean up a module debug context
 * 
 * Releases resources used by a debug context structure.
 * 
 * @param module Pointer to the module interface
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
int module_debug_context_cleanup(struct base_module *module);

/**
 * Configure tracing for a module
 * 
 * Sets tracing options for a specific module.
 * 
 * @param module Pointer to the module interface
 * @param enabled Whether to enable tracing for this module
 * @param min_level Minimum trace level for this module
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
int module_set_trace_options(struct base_module *module, bool enabled, trace_level_t min_level);

#ifdef __cplusplus
}
#endif