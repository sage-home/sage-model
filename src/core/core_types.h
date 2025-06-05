#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/**
 * @file core_types.h
 * @brief Common type definitions for SAGE
 * 
 * This file defines common types, constants, and enumerations used throughout
 * the SAGE system. It's designed to be included by other header files to avoid
 * circular dependencies.
 */

/* Common constants */
#define MAX_MODULE_NAME 64
#define MAX_MODULE_VERSION 32
#define MAX_MODULE_AUTHOR 64
#define MAX_MODULE_DESCRIPTION 256
#define MAX_ERROR_MESSAGE 256

/**
 * Module type identifiers
 * 
 * Core-agnostic module type system. The core only needs to know about
 * the concept of module types, not specific physics implementations.
 * Physics modules register themselves with their own type identifiers.
 */
enum module_type {
    MODULE_TYPE_UNKNOWN = 0,
    /* Physics modules define their own types at registration time */
    MODULE_TYPE_MAX = 1000  /* Reserve space for physics module types */
};

/**
 * Pipeline execution phases
 * 
 * Defines the different execution contexts in which modules can operate:
 * - HALO: Calculations that happen once per halo (outside galaxy loop)
 * - GALAXY: Calculations that happen for each galaxy
 * - POST: Calculations that happen after processing all galaxies (per step)
 * - FINAL: Calculations that happen after all steps are complete
 */
enum pipeline_execution_phase {
    PIPELINE_PHASE_NONE = 0,    /* No phase - initial state or reset */
    PIPELINE_PHASE_HALO = 1,    /* Execute once per halo (outside galaxy loop) */
    PIPELINE_PHASE_GALAXY = 2,  /* Execute for each galaxy (inside galaxy loop) */
    PIPELINE_PHASE_POST = 4,    /* Execute after processing all galaxies (for each integration step) */
    PIPELINE_PHASE_FINAL = 8    /* Execute after all steps are complete, before exiting evolve_galaxies */
};

/**
 * Version structure
 * 
 * Used for semantic versioning of modules
 */
struct module_version {
    int major;       /* Major version (incompatible API changes) */
    int minor;       /* Minor version (backwards-compatible functionality) */
    int patch;       /* Patch version (backwards-compatible bug fixes) */
};

/* Forward declarations for commonly used structures */
struct base_module;
struct pipeline_context;
struct pipeline_step;

#ifdef __cplusplus
}
#endif