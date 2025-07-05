#include <stdio.h>
#include <stdarg.h>

/* Stubs for logging functions used in io_memory_map.c */
void log_message(int level, const char* file, int line, const char* func, const char* fmt, ...) {
    (void)level;  // Suppress unused parameter warning
    (void)file;   // Suppress unused parameter warning
    (void)line;   // Suppress unused parameter warning
    (void)func;   // Suppress unused parameter warning
    (void)fmt;    // Suppress unused parameter warning
    
    // Completely suppress all log messages in test
    // No output to stderr or stdout
}