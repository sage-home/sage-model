/* File: utils.h */
/*
  This file is a part of the Corrfunc package
  Copyright (C) 2015-- Manodeep Sinha (manodeep@gmail.com)
  License: MIT LICENSE. See LICENSE file under the top-level
  directory at https://github.com/manodeep/Corrfunc/
*/

#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>    //defines int64_t datatype -> *exactly* 8 bytes int
#include <time.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/types.h>
#include <signal.h>    // For raise() and SIGTRAP
#include "core_logging.h"  // For LOG_ERROR

// Assert macro for runtime verification that logs errors
#define sage_assert(cond, msg) do { if(!(cond)) { LOG_ERROR("%s", (msg)); } } while(0)

// Pointer corruption detection macros for 32-bit pointer truncation debugging
#define IS_POINTER_CORRUPTED(p) \
    (((uintptr_t)(p) != (uintptr_t)NULL) && (((uintptr_t)(p) & 0xFFFFFFFF00000000ULL) == 0xFFFFFFFF00000000ULL))

#define VALIDATE_PROPERTIES_POINTER(g, context_msg) \
    do { \
        if ((g) != NULL && IS_POINTER_CORRUPTED((g)->properties)) { \
            LOG_ERROR("FATAL MEMORY CORRUPTION in %s: GalaxyIndex %llu has corrupted properties pointer %p.", \
                      (context_msg), (g)->GalaxyIndex, (g)->properties); \
            LOG_ERROR("FATAL: Terminating to prevent invalid scientific results"); \
            raise(SIGTRAP); /* Trigger debugger at the point of detection */ \
        } \
    } while (0)

// More aggressive validation macros for deeper debugging
#define VALIDATE_GALAXY_ARRAY(arr, count, context_msg) \
    do { \
        for (int _i = 0; _i < (count); _i++) { \
            if ((arr) != NULL && IS_POINTER_CORRUPTED((arr)[_i].properties)) { \
                LOG_ERROR("FATAL ARRAY CORRUPTION in %s: Galaxy[%d] GalaxyIndex %llu has corrupted properties pointer %p.", \
                          (context_msg), _i, (arr)[_i].GalaxyIndex, (arr)[_i].properties); \
                LOG_ERROR("FATAL: Terminating to prevent invalid scientific results"); \
                raise(SIGTRAP); \
            } \
        } \
    } while (0)

#ifndef LOG_WARNING
#define LOG_WARNING(fmt, ...) fprintf(stderr, "WARNING: " fmt "\n", ##__VA_ARGS__)
#endif

#define MEMORY_INTEGRITY_CHECK(ptr, old_ptr, context_msg) \
    do { \
        if ((ptr) != (old_ptr)) { \
            LOG_WARNING("MEMORY REALLOCATION in %s: ptr changed from %p to %p", \
                       (context_msg), (old_ptr), (ptr)); \
        } \
    } while (0)

// Enhanced validation with detailed memory information
#define VALIDATE_PROPERTIES_DETAILED(g, context_msg) \
    do { \
        if ((g) != NULL) { \
            if ((g)->properties == NULL) { \
                LOG_DEBUG("NULL properties pointer in %s: GalaxyIndex %llu", \
                         (context_msg), (g)->GalaxyIndex); \
            } else if (IS_POINTER_CORRUPTED((g)->properties)) { \
                LOG_ERROR("CORRUPTION DETECTED in %s: GalaxyIndex %llu has corrupted properties pointer %p.", \
                          (context_msg), (g)->GalaxyIndex, (g)->properties); \
                LOG_ERROR("Memory pattern analysis: high bits = 0x%08llx, low bits = 0x%08llx", \
                          ((uintptr_t)(g)->properties >> 32), ((uintptr_t)(g)->properties & 0xFFFFFFFF)); \
                raise(SIGTRAP); \
            } else { \
                LOG_DEBUG("Valid properties pointer in %s: GalaxyIndex %llu -> %p", \
                         (context_msg), (g)->GalaxyIndex, (g)->properties); \
            } \
        } \
    } while (0)

#ifdef __cplusplus
extern "C" {
#endif

    extern int my_snprintf(char *buffer, int len, const char *format, ...)
        __attribute__((format(printf, 3, 4)));
    extern char *get_time_string(struct timeval t0, struct timeval t1);
    extern int64_t getnumlines(const char *fname,const char comment);
    extern size_t myfread(void *ptr, const size_t size, const size_t nmemb, FILE * stream);
    extern size_t myfwrite(const void *ptr, const size_t size, const size_t nmemb, FILE * stream);
    extern int myfseek(FILE * stream, const long offset, const int whence);
    extern ssize_t mypread(int fd, void *ptr, const size_t nbytes, off_t offset);
    extern ssize_t mypwrite(int fd, const void *ptr, const size_t nbytes, off_t offset);
    extern ssize_t mywrite(int fd, const void *buf, size_t nbytes);
    extern int AlmostEqualRelativeAndAbs_double(double A, double B, const double maxDiff, const double maxRelDiff);

#ifdef __cplusplus
}
#endif
