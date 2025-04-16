#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <sys/time.h>

#include "core_module_debug.h"
#include "core_logging.h"
#include "core_mymalloc.h"

/* Module trace system state */
static struct {
    bool initialized;                        /* Whether the system is initialized */
    module_trace_config_t config;            /* Trace configuration */
    module_trace_entry_t *entries;           /* Trace entries */
    int num_entries;                         /* Number of entries in the log */
    int next_entry;                          /* Next entry to write (for circular buffer) */
    FILE *log_file;                          /* Log file handle */
} trace_system = {0};

/* Initialize the module debugging system */
bool module_debug_init(const module_trace_config_t *config) {
    if (!config) {
        LOG_ERROR("Invalid parameters to module_debug_init");
        return false;
    }
    
    /* Check if already initialized */
    if (trace_system.initialized) {
        LOG_WARNING("Module debugging system already initialized");
        return true;
    }
    
    /* Copy the configuration */
    memcpy(&trace_system.config, config, sizeof(module_trace_config_t));
    
    /* Set default buffer size if not specified */
    if (trace_system.config.buffer_size <= 0) {
        trace_system.config.buffer_size = MAX_TRACE_ENTRIES;
    }
    
    /* Allocate the trace buffer */
    trace_system.entries = (module_trace_entry_t *)mymalloc(
        trace_system.config.buffer_size * sizeof(module_trace_entry_t)
    );
    
    if (!trace_system.entries) {
        LOG_ERROR("Failed to allocate trace buffer");
        return false;
    }
    
    /* Initialize trace state */
    trace_system.num_entries = 0;
    trace_system.next_entry = 0;
    
    /* Open log file if needed */
    if (trace_system.config.log_to_file && trace_system.config.log_file[0]) {
        trace_system.log_file = fopen(trace_system.config.log_file, "w");
        if (!trace_system.log_file) {
            LOG_WARNING("Failed to open trace log file: %s", trace_system.config.log_file);
            /* Continue without file logging */
        } else {
            /* Write log header */
            fprintf(trace_system.log_file, "# SAGE Module Trace Log\n");
            fprintf(trace_system.log_file, "# Timestamp, Level, Module ID, Function, File, Line, Message\n");
        }
    }
    
    trace_system.initialized = true;
    
    /* Log initialization */
    MODULE_TRACE_INFO(-1, "Module debugging system initialized");
    
    return true;
}

/* Clean up the module debugging system */
bool module_debug_cleanup(void) {
    if (!trace_system.initialized) {
        return true;
    }
    
    /* Log cleanup */
    MODULE_TRACE_INFO(-1, "Module debugging system shutting down");
    
    /* Close log file if open */
    if (trace_system.log_file) {
        fclose(trace_system.log_file);
        trace_system.log_file = NULL;
    }
    
    /* Free trace buffer */
    if (trace_system.entries) {
        myfree(trace_system.entries);
        trace_system.entries = NULL;
    }
    
    /* Reset state */
    trace_system.initialized = false;
    trace_system.num_entries = 0;
    trace_system.next_entry = 0;
    
    return true;
}

/* Get current timestamp */
static double get_timestamp(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1000000.0;
}

/* Helper function to format trace entry components */
static void format_trace_components(
    const module_trace_entry_t *entry,
    char *timestamp_buffer, size_t timestamp_size,
    const char **level_str,
    int *millis
) {
    /* Format timestamp */
    time_t t = (time_t)entry->timestamp;
    struct tm *tm = localtime(&t);
    strftime(timestamp_buffer, timestamp_size, "%Y-%m-%d %H:%M:%S", tm);
    
    /* Calculate milliseconds */
    *millis = (int)((entry->timestamp - (int)entry->timestamp) * 1000);
    
    /* Format trace level */
    switch (entry->level) {
        case TRACE_LEVEL_DEBUG:
            *level_str = "DEBUG";
            break;
        case TRACE_LEVEL_INFO:
            *level_str = "INFO";
            break;
        case TRACE_LEVEL_WARNING:
            *level_str = "WARNING";
            break;
        case TRACE_LEVEL_ERROR:
            *level_str = "ERROR";
            break;
        case TRACE_LEVEL_FATAL:
            *level_str = "FATAL";
            break;
        default:
            *level_str = "UNKNOWN";
            break;
    }
}

/* Add an entry to the module trace log */
bool module_trace_log(
    trace_level_t level,
    int module_id,
    const char *function,
    const char *file,
    int line,
    const char *format,
    ...
) {
    /* Check if tracing is enabled */
    if (!trace_system.initialized || !trace_system.config.enabled) {
        return false;
    }
    
    /* Check minimum trace level */
    if (level < trace_system.config.min_level) {
        return true;
    }
    
    /* Get the next entry to write */
    module_trace_entry_t *entry;
    
    if (trace_system.config.circular_buffer) {
        /* Use circular buffer */
        entry = &trace_system.entries[trace_system.next_entry];
        trace_system.next_entry = (trace_system.next_entry + 1) % trace_system.config.buffer_size;
        if (trace_system.num_entries < trace_system.config.buffer_size) {
            trace_system.num_entries++;
        }
    } else {
        /* Use linear buffer */
        if (trace_system.num_entries >= trace_system.config.buffer_size) {
            /* Buffer is full */
            return false;
        }
        
        entry = &trace_system.entries[trace_system.num_entries++];
    }
    
    /* Fill in the entry */
    entry->level = level;
    entry->module_id = module_id;
    entry->timestamp = get_timestamp();
    
    /* Copy function and file information */
    entry->function = function;
    entry->file = file;
    entry->line = line;
    
    /* Format the message */
    va_list args;
    va_start(args, format);
    vsnprintf(entry->message, MAX_TRACE_ENTRY_LENGTH, format, args);
    va_end(args);
    
    /* Log to console if enabled */
    if (trace_system.config.log_to_console) {
        char formatted[MAX_TRACE_ENTRY_LENGTH * 2];
        module_trace_format_entry(entry, formatted, sizeof(formatted));
        printf("%s\n", formatted);
    }
    
    /* Log to file if enabled */
    if (trace_system.config.log_to_file && trace_system.log_file) {
        char timestamp[32];
        const char *level_str;
        int millis;
        format_trace_components(entry, timestamp, sizeof(timestamp), &level_str, &millis);
        
        /* Write to file */
        fprintf(trace_system.log_file, "%s.%03d, %s, %d, %s, %s, %d, %s\n",
               timestamp, millis,
               level_str, entry->module_id, 
               entry->function ? entry->function : "unknown",
               entry->file ? entry->file : "unknown",
               entry->line, entry->message);
        fflush(trace_system.log_file);
    }
    
    return true;
}

/* Get the module trace log */
bool module_trace_get_log(
    module_trace_entry_t *entries,
    int max_entries,
    int *num_entries
) {
    if (!trace_system.initialized || !entries || max_entries <= 0 || !num_entries) {
        return false;
    }
    
    /* Determine number of entries to copy */
    int count = trace_system.num_entries;
    if (count > max_entries) {
        count = max_entries;
    }
    
    /* Copy entries */
    if (trace_system.config.circular_buffer && trace_system.num_entries >= trace_system.config.buffer_size) {
        /* Copy oldest entries first (circular buffer is full) */
        int start = trace_system.next_entry;
        int first_chunk = trace_system.config.buffer_size - start;
        
        if (first_chunk > count) {
            first_chunk = count;
        }
        
        /* Copy first chunk (from start to end of buffer) */
        memcpy(entries, &trace_system.entries[start], first_chunk * sizeof(module_trace_entry_t));
        
        if (count > first_chunk) {
            /* Copy second chunk (from beginning of buffer) */
            memcpy(&entries[first_chunk], trace_system.entries, 
                  (count - first_chunk) * sizeof(module_trace_entry_t));
        }
    } else {
        /* Copy entries from beginning */
        memcpy(entries, trace_system.entries, count * sizeof(module_trace_entry_t));
    }
    
    *num_entries = count;
    return true;
}

/* Clear the module trace log */
bool module_trace_clear_log(void) {
    if (!trace_system.initialized) {
        return false;
    }
    
    trace_system.num_entries = 0;
    trace_system.next_entry = 0;
    
    return true;
}

/* Format a trace entry as text */
bool module_trace_format_entry(
    const module_trace_entry_t *entry,
    char *output,
    size_t size
) {
    if (!entry || !output || size == 0) {
        return false;
    }
    
    /* Get formatted components */
    char timestamp[32];
    const char *level_str;
    int millis;
    format_trace_components(entry, timestamp, sizeof(timestamp), &level_str, &millis);
    
    /* Format the entry */
    snprintf(output, size, "[%s.%03d] [%s] [Module %d] %s",
             timestamp, millis, level_str, entry->module_id, entry->message);
    
    /* Add file and line information if available */
    if (entry->file && entry->line > 0) {
        size_t len = strlen(output);
        snprintf(output + len, size - len, " (%s:%d)", entry->file, entry->line);
    }
    
    return true;
}

/* Write the module trace log to a file */
bool module_trace_write_to_file(const char *filename) {
    if (!trace_system.initialized || !filename) {
        return false;
    }
    
    /* Open the file */
    FILE *file = fopen(filename, "w");
    if (!file) {
        LOG_ERROR("Failed to open trace log file: %s", filename);
        return false;
    }
    
    /* Write header */
    fprintf(file, "# SAGE Module Trace Log\n");
    fprintf(file, "# Timestamp, Level, Module ID, Function, File, Line, Message\n");
    
    /* Get the trace log */
    module_trace_entry_t *entries = (module_trace_entry_t *)mymalloc(
        trace_system.num_entries * sizeof(module_trace_entry_t)
    );
    
    if (!entries) {
        LOG_ERROR("Failed to allocate memory for trace log");
        fclose(file);
        return false;
    }
    
    int count;
    if (!module_trace_get_log(entries, trace_system.num_entries, &count)) {
        LOG_ERROR("Failed to get trace log");
        myfree(entries);
        fclose(file);
        return false;
    }
    
    /* Write entries */
    for (int i = 0; i < count; i++) {
        const module_trace_entry_t *entry = &entries[i];
        
        char timestamp[32];
        const char *level_str;
        int millis;
        format_trace_components(entry, timestamp, sizeof(timestamp), &level_str, &millis);
        
        /* Write to file */
        fprintf(file, "%s.%03d, %s, %d, %s, %s, %d, %s\n",
               timestamp, millis,
               level_str, entry->module_id, 
               entry->function ? entry->function : "unknown",
               entry->file ? entry->file : "unknown",
               entry->line, entry->message);
    }
    
    /* Clean up */
    myfree(entries);
    fclose(file);
    
    return true;
}

/* Set the minimum trace level */
bool module_trace_set_min_level(trace_level_t level) {
    if (!trace_system.initialized) {
        return false;
    }
    
    trace_system.config.min_level = level;
    return true;
}

/* Enable or disable module tracing */
bool module_trace_set_enabled(bool enabled) {
    if (!trace_system.initialized) {
        return false;
    }
    
    trace_system.config.enabled = enabled;
    return true;
}

/* Check if module tracing is enabled */
bool module_trace_is_enabled(void) {
    return trace_system.initialized && trace_system.config.enabled;
}

/**
 * Initialize a module debug context
 * 
 * Allocates and initializes a debug context structure for a module.
 * 
 * @param module Pointer to the module interface
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
int module_debug_context_init(struct base_module *module) {
    if (module == NULL) {
        LOG_ERROR("NULL module pointer passed to module_debug_context_init");
        return MODULE_STATUS_INVALID_ARGS;
    }
    
    /* Check if already initialized */
    if (module->debug_context != NULL) {
        LOG_WARNING("Module '%s' already has a debug context", module->name);
        return MODULE_STATUS_ALREADY_INITIALIZED;
    }
    
    /* Allocate debug context */
    struct module_debug_context *context = (struct module_debug_context *)mymalloc(sizeof(struct module_debug_context));
    if (context == NULL) {
        LOG_ERROR("Failed to allocate memory for module debug context");
        return MODULE_STATUS_OUT_OF_MEMORY;
    }
    
    /* Initialize context fields */
    memset(context, 0, sizeof(struct module_debug_context));
    context->tracing_enabled = true;
    context->min_trace_level = TRACE_LEVEL_INFO;
    context->module_id = module->module_id;
    context->error_context = module->error_context;
    
    /* Initialize trace buffer (if needed in the future) */
    context->trace_entries = NULL;
    context->trace_count = 0;
    context->current_trace_index = 0;
    context->trace_overflow = false;
    
    /* Assign to module */
    module->debug_context = context;
    
    LOG_DEBUG("Initialized debug context for module '%s'", module->name);
    return MODULE_STATUS_SUCCESS;
}

/**
 * Clean up a module debug context
 * 
 * Releases resources used by a debug context structure.
 * 
 * @param module Pointer to the module interface
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
int module_debug_context_cleanup(struct base_module *module) {
    if (module == NULL) {
        LOG_ERROR("NULL module pointer passed to module_debug_context_cleanup");
        return MODULE_STATUS_INVALID_ARGS;
    }
    
    /* Check if initialized */
    if (module->debug_context == NULL) {
        LOG_WARNING("Module '%s' has no debug context to clean up", module->name);
        return MODULE_STATUS_NOT_INITIALIZED;
    }
    
    /* Free trace buffer if allocated */
    if (module->debug_context->trace_entries != NULL) {
        myfree(module->debug_context->trace_entries);
        module->debug_context->trace_entries = NULL;
    }
    
    /* Free debug context */
    myfree(module->debug_context);
    module->debug_context = NULL;
    
    LOG_DEBUG("Cleaned up debug context for module '%s'", module->name);
    return MODULE_STATUS_SUCCESS;
}

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
int module_set_trace_options(struct base_module *module, bool enabled, trace_level_t min_level) {
    if (module == NULL) {
        LOG_ERROR("NULL module pointer passed to module_set_trace_options");
        return MODULE_STATUS_INVALID_ARGS;
    }
    
    /* Ensure debug context is initialized */
    if (module->debug_context == NULL) {
        int status = module_debug_context_init(module);
        if (status != MODULE_STATUS_SUCCESS) {
            LOG_ERROR("Failed to initialize debug context for module '%s'", module->name);
            return status;
        }
    }
    
    /* Update tracing options */
    module->debug_context->tracing_enabled = enabled;
    module->debug_context->min_trace_level = min_level;
    
    LOG_DEBUG("Updated tracing options for module '%s' (enabled: %d, min_level: %d)",
             module->name, enabled, min_level);
    return MODULE_STATUS_SUCCESS;
}