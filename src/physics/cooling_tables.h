#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file cooling_tables.h
 * @brief Cooling table functionality for the cooling module
 * 
 * This file provides functions to read and interpolate cooling tables
 * for metal-dependent cooling rate calculations.
 */

/**
 * Read cooling function tables from files
 * 
 * @param root_dir Root directory of the SAGE installation
 */
void read_cooling_functions(const char *root_dir);

/**
 * Get metal-dependent cooling rate
 * 
 * @param logTemp log10 of temperature in Kelvin
 * @param logZ log10 of metallicity
 * @return Cooling rate Lambda(T,Z)
 */
double get_metaldependent_cooling_rate(const double logTemp, double logZ);

#ifdef __cplusplus
}
#endif