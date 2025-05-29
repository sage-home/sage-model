/**
 * @file core_logging.c
 * @brief Implementation of the enhanced error logging system for SAGE
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <errno.h>

#include "core_logging.h"
#include "core_allvars.h"
#include "core_mymalloc.h"

/* Static global logging state */
static struct logging_state global_logging_state = {
    .log_file = NULL,
    .initialized = false
};

/* String representations of log levels */
static const char *log_level_names[] = {
    "TRACE",
    "DEBUG",
    "INFO",
    "NOTICE",
    "WARNING",
    "ERROR",
    "CRITICAL"
};

/* ANSI color codes for different log levels (if terminal supports it) */
static const char *log_level_colors[] = {
    "\033[90m",  /* Gray for TRACE */
    "\033[36m",  /* Cyan for DEBUG */
    "\033[32m",  /* Green for INFO */
    "\033[34m",  /* Blue for NOTICE */
    "\033[33m",  /* Yellow for WARNING */
    "\033[31m",  /* Red for ERROR */
    "\033[1;31m" /* Bold Red for CRITICAL */
};

/* Reset ANSI color code */
static const char *color_reset = "\033[0m";

/**
 * Check if stdout is a terminal to determine if we should use colors
 */
static bool is_terminal(FILE *stream) {
    return isatty(fileno(stream)) == 1;
}

/**
 * Get string representation of a log level
 */
static const char *get_log_level_name(log_level_t level) {
    if (level < LOG_LEVEL_TRACE || level > LOG_LEVEL_CRITICAL) {
        return "UNKNOWN";
    }
    return log_level_names[level];
}

/**
 * Get current time as a formatted string
 */
static void get_current_time(char *buffer, size_t buffer_size) {
    time_t now = time(NULL);
    struct tm tm_now;
    struct timeval tv;
    
    gettimeofday(&tv, NULL);
    localtime_r(&now, &tm_now);
    
    strftime(buffer, buffer_size, "%Y-%m-%d %H:%M:%S", &tm_now);
    
    /* Append milliseconds */
    char ms_buffer[8];
    snprintf(ms_buffer, sizeof(ms_buffer), ".%03ld", (long)(tv.tv_usec / 1000));
    strncat(buffer, ms_buffer, buffer_size - strlen(buffer) - 1);
}

/**
 * Write a log message to a specific destination
 */
static void write_log_to_destination(FILE *dest, const struct logging_params_view *config,
                                   log_level_t level, const char *file, int line, 
                                   const char *func, const char *message,
                                   const struct evolution_context *ctx) {
    char time_buffer[32];
    const int mpi_rank = config->full_params ? config->full_params->runtime.ThisTask : -1;
    const bool use_colors = is_terminal(dest);
    
    /* Create log message prefix based on style */
    switch (config->prefix_style) {
        case LOG_PREFIX_DETAILED:
            get_current_time(time_buffer, sizeof(time_buffer));
            fprintf(dest, "[%s] ", time_buffer);
            
            if (config->include_mpi_rank && mpi_rank >= 0) {
                fprintf(dest, "[MPI:%d] ", mpi_rank);
            }
            
            if (use_colors) {
                fprintf(dest, "%s[%s]%s [%s:%d %s] ", 
                        log_level_colors[level], get_log_level_name(level), color_reset,
                        file, line, func);
            } else {
                fprintf(dest, "[%s] [%s:%d %s] ", 
                        get_log_level_name(level), file, line, func);
            }
            
            /* Add galaxy context information if available and enabled */
            if (ctx != NULL && config->include_extra_context) {
                fprintf(dest, "[Halo:%d Snap:%d Gals:%d] ", 
                        ctx->halo_nr, ctx->halo_snapnum, ctx->ngal);
            }
            break;
            
        case LOG_PREFIX_SIMPLE:
            if (use_colors) {
                fprintf(dest, "%s[%s]%s ", 
                        log_level_colors[level], get_log_level_name(level), color_reset);
            } else {
                fprintf(dest, "[%s] ", get_log_level_name(level));
            }
            break;
            
        case LOG_PREFIX_NONE:
        default:
            /* No prefix */
            break;
    }
    
    /* Write the actual message */
    fprintf(dest, "%s\n", message);
    
    /* Ensure immediate output for ERROR and higher levels */
    if (level >= LOG_LEVEL_ERROR) {
        fflush(dest);
    }
}

/**
 * Write a log message to all configured destinations
 */
static void write_log(log_level_t level, const char *file, int line, 
                    const char *func, const char *message,
                    const struct evolution_context *ctx) {
    const struct logging_params_view *config = &global_logging_state.config;
    
    /* Skip if message level is below the configured minimum level */
    if (level < config->min_level || !global_logging_state.initialized) {
        return;
    }
    
    /* Always show ERROR and CRITICAL logs regardless of verbosity */
    if (level >= LOG_LEVEL_ERROR) {
        write_log_to_destination(stderr, config, level, file, line, func, message, ctx);
        return;
    }
    
#ifdef VERBOSE
    /* In verbose mode, show all logs */
    /* Write to stdout if configured */
    if (config->destinations & LOG_DEST_STDOUT) {
        write_log_to_destination(stdout, config, level, file, line, func, message, ctx);
    }
    
    /* Write to stderr if configured */
    if (config->destinations & LOG_DEST_STDERR) {
        write_log_to_destination(stderr, config, level, file, line, func, message, ctx);
    }
    
    /* Write to log file if configured */
    if ((config->destinations & LOG_DEST_FILE) && global_logging_state.log_file != NULL) {
        write_log_to_destination(global_logging_state.log_file, config, level, file, line, func, message, ctx);
    }
#else
    /* In non-verbose mode, show only WARNING and above */
    if (level >= LOG_LEVEL_WARNING) {
        if (config->destinations & LOG_DEST_STDERR) {
            write_log_to_destination(stderr, config, level, file, line, func, message, ctx);
        }
    }
#endif
}

/**
 * Initialize the logging system with minimum level and output
 */
void logging_init(log_level_t min_level, FILE *output) {
    struct logging_params_view view = {0};
    
    view.min_level = min_level;
    view.prefix_style = LOG_PREFIX_DETAILED;
    view.destinations = LOG_DEST_STDERR;  /* Default to stderr */
    view.include_mpi_rank = false;
    view.disable_assertions = false;
    view.include_extra_context = true;
    
    memcpy(&global_logging_state.config, &view, sizeof(view));
    
    if (output != NULL) {
        global_logging_state.log_file = output;
        global_logging_state.config.destinations |= LOG_DEST_FILE;
    }
    
    global_logging_state.initialized = true;
    
    /* Log a message to show successful initialization */
    log_message(LOG_LEVEL_INFO, __FILE__, __LINE__, __func__, 
              "Logging system initialized (min level: %s)", 
              get_log_level_name(min_level));
}

/**
 * Set global log level
 */
void logging_set_level(log_level_t level) {
    if (!global_logging_state.initialized) {
        /* Initialize with default settings if not already initialized */
        logging_init(level, NULL);
        return;
    }
    
    global_logging_state.config.min_level = level;
    
    /* Log the level change if not too verbose */
    if (level <= LOG_LEVEL_INFO) {
        log_message(LOG_LEVEL_INFO, __FILE__, __LINE__, __func__, 
                  "Log level changed to %s", get_log_level_name(level));
    }
}

/**
 * Get current global log level
 */
log_level_t logging_get_level(void) {
    if (!global_logging_state.initialized) {
        return LOG_LEVEL_INFO;  // Return default level if not initialized
    }
    
    return global_logging_state.config.min_level;
}

/**
 * Initialize the logging system with parameters
 */
int initialize_logging(const struct params *params) {
    if (global_logging_state.initialized) {
        return 0; /* Already initialized */
    }
    
    /* Initialize the configuration */
    initialize_logging_params_view(&global_logging_state.config, params);
    
    /* For simplicity, we'll disable file logging for now and use stderr */
    global_logging_state.config.destinations &= ~LOG_DEST_FILE;
    global_logging_state.config.destinations |= LOG_DEST_STDERR;
    
    global_logging_state.initialized = true;
    
    /* Log a message to show successful initialization */
    int ThisTask = (params != NULL) ? params->runtime.ThisTask : -1;
    if (ThisTask == 0 || ThisTask == -1) {
        log_message(LOG_LEVEL_INFO, __FILE__, __LINE__, __func__, 
                  "Logging system initialized (min level: %s)", 
                  get_log_level_name(global_logging_state.config.min_level));
    }
    
    return 0;
}

/**
 * Clean up the logging system
 */
int cleanup_logging(void) {
    if (!global_logging_state.initialized) {
        return 0; /* Not initialized */
    }
    
    /* Log a message to show shutdown */
    log_message(LOG_LEVEL_INFO, __FILE__, __LINE__, __func__, "Logging system shutting down");
    
    /* Close log file if it was opened */
    if (global_logging_state.log_file != NULL && 
        global_logging_state.log_file != stdout && 
        global_logging_state.log_file != stderr) {
        fclose(global_logging_state.log_file);
        global_logging_state.log_file = NULL;
    }
    
    global_logging_state.initialized = false;
    
    return 0;
}

/**
 * Log a message with a specific severity level
 */
void log_message(log_level_t level, const char *file, int line, const char *func, const char *format, ...) {
    /* Skip processing if logging isn't initialized or level is filtered */
    if (!global_logging_state.initialized || level < global_logging_state.config.min_level) {
        return;
    }
    
    /* Format the message */
    char message[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    
    /* Write to all configured destinations */
    write_log(level, file, line, func, message, NULL);
    
    /* For fatal errors, abort the program */
    if (level == LOG_LEVEL_CRITICAL) {
        /* Ensure all logs are flushed */
        if (global_logging_state.log_file != NULL) {
            fflush(global_logging_state.log_file);
        }
        fflush(stdout);
        fflush(stderr);
        
        /* Abort the program */
        abort();
    }
}

/**
 * Log a module-specific message
 */
void log_module_message(const char *module, log_level_t level, const char *file, int line, const char *func, const char *format, ...) {
    /* Skip processing if logging isn't initialized or level is filtered */
    if (!global_logging_state.initialized || level < global_logging_state.config.min_level) {
        return;
    }
    
    /* Format the message with module prefix */
    char module_message[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(module_message, sizeof(module_message), format, args);
    va_end(args);
    
    /* Create full message with module name */
    char full_message[1100];
    snprintf(full_message, sizeof(full_message), "[%s] %s", module, module_message);
    
    /* Write to all configured destinations */
    write_log(level, file, line, func, full_message, NULL);
    
    /* For fatal errors, abort the program */
    if (level == LOG_LEVEL_CRITICAL) {
        /* Ensure all logs are flushed */
        if (global_logging_state.log_file != NULL) {
            fflush(global_logging_state.log_file);
        }
        fflush(stdout);
        fflush(stderr);
        
        /* Abort the program */
        abort();
    }
}

/**
 * Log a message with context information
 */
void context_log_message(const struct evolution_context *ctx, log_level_t level, 
                        const char *file, int line, const char *func, 
                        const char *format, ...) {
    /* Skip processing if logging isn't initialized or level is filtered */
    if (!global_logging_state.initialized || level < global_logging_state.config.min_level) {
        return;
    }
    
    /* Format the message */
    char message[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    
    /* Write to all configured destinations */
    write_log(level, file, line, func, message, ctx);
    
    /* For fatal errors, abort the program */
    if (level == LOG_LEVEL_CRITICAL) {
        /* Ensure all logs are flushed */
        if (global_logging_state.log_file != NULL) {
            fflush(global_logging_state.log_file);
        }
        fflush(stdout);
        fflush(stderr);
        
        /* Abort the program */
        abort();
    }
}

/**
 * Assert a condition and log a message if it fails
 */
bool assert_log(bool condition, const char *file, int line, const char *func, 
               const char *format, ...) {
    /* If the condition is true, or assertions are disabled, do nothing */
    if (condition || (global_logging_state.initialized && 
                     global_logging_state.config.disable_assertions)) {
        return true;
    }
    
    /* Format the error message */
    char message[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    
    /* Log the assertion failure */
    char assert_message[1536];
    snprintf(assert_message, sizeof(assert_message), 
            "Assertion failed: %s", message);
    
    /* Write to all configured destinations */
    write_log(LOG_LEVEL_ERROR, file, line, func, assert_message, NULL);
    
    #ifdef NDEBUG
    /* In release builds (NDEBUG defined), don't abort */
    return false;
    #else
    /* In debug builds, abort the program */
    abort();
    return false; /* Not reached, but avoids compiler warning */
    #endif
}

/**
 * Validate a parameter and log a message if it fails
 */
bool validate_param(bool condition, const char *file, int line, const char *func, 
                  const char *format, ...) {
    /* If the condition is true, do nothing */
    if (condition) {
        return true;
    }
    
    /* Format the error message */
    char message[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    
    /* Log the validation failure */
    char validate_message[1536];
    snprintf(validate_message, sizeof(validate_message), 
            "Parameter validation failed: %s", message);
    
    /* Write to all configured destinations */
    write_log(LOG_LEVEL_WARNING, file, line, func, validate_message, NULL);
    
    return false;
}

/**
 * Check if a specific log level is enabled
 */
bool is_log_level_enabled(log_level_t level) {
    return global_logging_state.initialized && level >= global_logging_state.config.min_level;
}

/**
 * Get the current logging state
 */
struct logging_state *get_logging_state(void) {
    return &global_logging_state;
}

/* These functions are now implemented in core_parameter_views.c */
