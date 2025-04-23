/**
 * @file core_logging.h
 * @brief Enhanced error logging system for SAGE
 * 
 * This file defines a comprehensive logging system that supports different 
 * severity levels, provides contextual information, and integrates with 
 * the evolution context and modules.
 */

#ifndef CORE_LOGGING_H
#define CORE_LOGGING_H

#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include "core_allvars.h"

/**
 * @brief Log severity levels
 * 
 * Define the different severity levels for log messages.
 * Each level is more severe than the previous one.
 */
typedef enum log_level {
    LOG_LEVEL_TRACE,    /**< Detailed flow (only in debug builds) */
    LOG_LEVEL_DEBUG,    /**< Information useful for debugging */
    LOG_LEVEL_INFO,     /**< General information about execution flow */
    LOG_LEVEL_NOTICE,   /**< Important but normal events */
    LOG_LEVEL_WARNING,  /**< Concerning but non-fatal issues */
    LOG_LEVEL_ERROR,    /**< Errors that prevent specific operations */
    LOG_LEVEL_CRITICAL, /**< Errors that prevent further execution */
    LOG_LEVEL_OFF       /**< No logging */
} log_level_t;

/**
 * @brief Log message prefix style
 * 
 * Define the style of prefixes in log messages.
 */
typedef enum {
    LOG_PREFIX_NONE = 0,    /**< No prefix */
    LOG_PREFIX_SIMPLE = 1,  /**< Simple prefix with level */
    LOG_PREFIX_DETAILED = 2 /**< Detailed prefix with level, time, file, line */
} log_prefix_style_t;

/**
 * @brief Log destination
 * 
 * Define where log messages should be sent.
 */
typedef enum {
    LOG_DEST_STDOUT = 0x01,  /**< Standard output */
    LOG_DEST_STDERR = 0x02,  /**< Standard error */
    LOG_DEST_FILE = 0x04     /**< Log file */
} log_destination_t;

/**
 * @brief Logging configuration parameters view
 * 
 * Contains parameters needed for logging configuration.
 */
struct logging_params_view {
    /** Minimum level of messages to log */
    log_level_t min_level;
    
    /** Style of log message prefixes */
    log_prefix_style_t prefix_style;
    
    /** Where to send log messages (can be combined) */
    int destinations;
    
    /** Path to log file (if LOG_DEST_FILE is set) */
    char log_file_path[MAX_STRING_LEN];
    
    /** Whether to include MPI rank in log messages */
    bool include_mpi_rank;
    
    /** Whether to disable assertions in debug builds */
    bool disable_assertions;
    
    /** Allow for adding custom fields to log prefixes */
    bool include_extra_context;
    
    /** Pointer back to full params */
    const struct params *full_params;
};

/**
 * @brief Logging state structure
 * 
 * Contains the runtime state for the logging system.
 */
struct logging_state {
    /** Pointer to logging configuration */
    struct logging_params_view config;
    
    /** File handle for the log file (if used) */
    FILE *log_file;
    
    /** Whether the logging system has been initialized */
    bool initialized;
};

/**
 * @brief Initialize the logging system
 * 
 * Sets up the logging system with minimum level and output destination.
 * 
 * @param min_level Minimum log level to display
 * @param output File handle for output (NULL for stderr)
 */
extern void logging_init(log_level_t min_level, FILE *output);

/**
 * @brief Set global log level
 * 
 * Changes the minimum log level that will be displayed.
 * 
 * @param level New minimum log level
 */
extern void logging_set_level(log_level_t level);

/**
 * @brief Initialize the logging system with parameters
 * 
 * Sets up the logging system with the provided configuration.
 * 
 * @param params The SAGE parameter structure
 * @return 0 on success, non-zero on failure
 */
extern int initialize_logging(const struct params *params);

/**
 * @brief Clean up the logging system
 * 
 * Performs cleanup for the logging system, including closing any open files.
 * 
 * @return 0 on success, non-zero on failure
 */
extern int cleanup_logging(void);

/**
 * @brief Initialize logging parameter view
 * 
 * Initializes a logging parameter view from the full parameter structure.
 * 
 * @param view The logging parameter view to initialize
 * @param params The full parameter structure
 */
extern void initialize_logging_params_view(struct logging_params_view *view, const struct params *params);

/**
 * @brief Validate logging parameters view
 * 
 * Checks for internal consistency and validates logging configuration.
 * 
 * @param view The logging parameter view to validate
 * @return true if validation passed, false if any issues found
 */
extern bool validate_logging_params_view(const struct logging_params_view *view);

/**
 * @brief Log a message with a specific severity level
 * 
 * Core logging function that formats and outputs a message to the configured destinations.
 * 
 * @param level The severity level of the message
 * @param file The source file where the log occurs
 * @param line The line number where the log occurs
 * @param func The function where the log occurs
 * @param format The printf-style format string
 * @param ... Additional arguments for the format string
 */
extern void log_message(log_level_t level, const char *file, int line, const char *func, const char *format, ...);

/**
 * @brief Log a module-specific message
 * 
 * Logs a message with a module name for better identification of the source.
 * 
 * @param module The module name to include in the log
 * @param level The severity level of the message
 * @param file The source file where the log occurs
 * @param line The line number where the log occurs
 * @param func The function where the log occurs
 * @param format The printf-style format string
 * @param ... Additional arguments for the format string
 */
extern void log_module_message(const char *module, log_level_t level, const char *file, int line, const char *func, const char *format, ...);

/**
 * @brief Log a message with context information
 * 
 * Same as log_message but includes evolution context information.
 * 
 * @param ctx The evolution context to include in the log
 * @param level The severity level of the message
 * @param file The source file where the log occurs
 * @param line The line number where the log occurs
 * @param func The function where the log occurs
 * @param format The printf-style format string
 * @param ... Additional arguments for the format string
 */
extern void context_log_message(const struct evolution_context *ctx, log_level_t level, 
                              const char *file, int line, const char *func, 
                              const char *format, ...);

/**
 * @brief Assert a condition and log a message if it fails
 * 
 * Checks a condition and logs a message if it fails. In debug builds,
 * it will abort the program, but in release builds it will just log.
 * 
 * @param condition The condition to check
 * @param file The source file where the assertion occurs
 * @param line The line number where the assertion occurs
 * @param func The function where the assertion occurs
 * @param format The printf-style format string for the error message
 * @param ... Additional arguments for the format string
 * @return true if the assertion passed, false if it failed
 */
extern bool assert_log(bool condition, const char *file, int line, const char *func, 
                     const char *format, ...);

/**
 * @brief Validate a parameter and log a message if it fails
 * 
 * Checks a parameter condition and logs a message if it fails. Similar to
 * assert_log, but specialized for parameter validation.
 * 
 * @param condition The condition to check
 * @param file The source file where the validation occurs
 * @param line The line number where the validation occurs
 * @param func The function where the validation occurs
 * @param format The printf-style format string for the error message
 * @param ... Additional arguments for the format string
 * @return true if the validation passed, false if it failed
 */
extern bool validate_param(bool condition, const char *file, int line, const char *func, 
                         const char *format, ...);

/**
 * @brief Check if a specific log level is enabled
 * 
 * Determines if messages at the specified level would be logged.
 * Useful for avoiding expensive computations for debug messages.
 * 
 * @param level The log level to check
 * @return true if the level is enabled, false otherwise
 */
extern bool is_log_level_enabled(log_level_t level);

/**
 * @brief Get the current logging state
 * 
 * Returns the current logging state structure.
 * 
 * @return Pointer to the current logging state
 */
extern struct logging_state *get_logging_state(void);

/* Convenience macros for logging */

/**
 * @brief Log a trace message
 * 
 * @param format The printf-style format string
 * @param ... Additional arguments for the format string
 */
#define LOG_TRACE(...) \
    log_message(LOG_LEVEL_TRACE, __FILE__, __LINE__, __func__, __VA_ARGS__)

/**
 * @brief Log a debug message
 * 
 * @param format The printf-style format string
 * @param ... Additional arguments for the format string
 */
#define LOG_DEBUG(...) \
    log_message(LOG_LEVEL_DEBUG, __FILE__, __LINE__, __func__, __VA_ARGS__)

/**
 * @brief Log an informational message
 * 
 * @param format The printf-style format string
 * @param ... Additional arguments for the format string
 */
#define LOG_INFO(...) \
    log_message(LOG_LEVEL_INFO, __FILE__, __LINE__, __func__, __VA_ARGS__)

/**
 * @brief Log a notice message
 * 
 * @param format The printf-style format string
 * @param ... Additional arguments for the format string
 */
#define LOG_NOTICE(...) \
    log_message(LOG_LEVEL_NOTICE, __FILE__, __LINE__, __func__, __VA_ARGS__)

/**
 * @brief Log a warning message
 * 
 * @param format The printf-style format string
 * @param ... Additional arguments for the format string
 */
#define LOG_WARNING(...) \
    log_message(LOG_LEVEL_WARNING, __FILE__, __LINE__, __func__, __VA_ARGS__)

/**
 * @brief Log an error message
 * 
 * @param format The printf-style format string
 * @param ... Additional arguments for the format string
 */
#define LOG_ERROR(...) \
    log_message(LOG_LEVEL_ERROR, __FILE__, __LINE__, __func__, __VA_ARGS__)

/**
 * @brief Log a critical error message
 * 
 * @param format The printf-style format string
 * @param ... Additional arguments for the format string
 */
#define LOG_CRITICAL(...) \
    log_message(LOG_LEVEL_CRITICAL, __FILE__, __LINE__, __func__, __VA_ARGS__)

/**
 * @brief Log a module-specific message
 * 
 * @param module The module name
 * @param level The severity level
 * @param format The printf-style format string
 * @param ... Additional arguments for the format string
 */
#define MODULE_LOG(module, level, ...) \
    log_module_message(module, level, __FILE__, __LINE__, __func__, __VA_ARGS__)

/**
 * @brief Log a message with evolution context
 * 
 * @param ctx The evolution context
 * @param level The severity level
 * @param format The printf-style format string
 * @param ... Additional arguments for the format string
 */
#define CONTEXT_LOG(ctx, level, format, ...) \
    context_log_message(ctx, level, __FILE__, __LINE__, __func__, format, ##__VA_ARGS__)

/**
 * @brief Assert a condition and log if it fails
 * 
 * @param condition The condition to check
 * @param format The printf-style format string for the error message
 * @param ... Additional arguments for the format string
 */
#define ASSERT(condition, format, ...) \
    assert_log((condition), __FILE__, __LINE__, __func__, format, ##__VA_ARGS__)

/**
 * @brief Validate a parameter and log if it fails
 * 
 * @param condition The condition to check
 * @param format The printf-style format string for the error message
 * @param ... Additional arguments for the format string
 */
#define VALIDATE_PARAM(condition, format, ...) \
    validate_param((condition), __FILE__, __LINE__, __func__, format, ##__VA_ARGS__)

/**
 * @brief Return if assertion fails
 * 
 * Asserts a condition and returns the specified value if it fails.
 * 
 * @param condition The condition to check
 * @param retval The value to return if the assertion fails
 * @param format The printf-style format string for the error message
 * @param ... Additional arguments for the format string
 */
#define ASSERT_RETURN(condition, retval, format, ...) \
    do { \
        if (!assert_log((condition), __FILE__, __LINE__, __func__, format, ##__VA_ARGS__)) \
            return (retval); \
    } while (0)

/**
 * @brief Return if parameter validation fails
 * 
 * Validates a parameter and returns the specified value if it fails.
 * 
 * @param condition The condition to check
 * @param retval The value to return if the validation fails
 * @param format The printf-style format string for the error message
 * @param ... Additional arguments for the format string
 */
#define VALIDATE_PARAM_RETURN(condition, retval, format, ...) \
    do { \
        if (!validate_param((condition), __FILE__, __LINE__, __func__, format, ##__VA_ARGS__)) \
            return (retval); \
    } while (0)

#endif /* CORE_LOGGING_H */