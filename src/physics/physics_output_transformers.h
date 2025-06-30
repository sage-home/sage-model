/**
 * @file physics_output_transformers.h
 * @brief Interface for physics output transformer functions
 *
 * This file declares the transformer functions that convert
 * internal property representations to output format.
 */

#ifndef PHYSICS_OUTPUT_TRANSFORMERS_H
#define PHYSICS_OUTPUT_TRANSFORMERS_H

#include "../core/core_allvars.h"
#include "../core/core_properties.h"

// Cooling and heating transformers
int transform_output_Cooling(const struct GALAXY *galaxy, property_id_t output_prop_id, 
                           void *output_buffer_element_ptr, const struct params *run_params);

int transform_output_Heating(const struct GALAXY *galaxy, property_id_t output_prop_id, 
                          void *output_buffer_element_ptr, const struct params *run_params);

// Time transformers
int transform_output_TimeOfLastMajorMerger(const struct GALAXY *galaxy, property_id_t output_prop_id, 
                                        void *output_buffer_element_ptr, const struct params *run_params);

int transform_output_TimeOfLastMinorMerger(const struct GALAXY *galaxy, property_id_t output_prop_id, 
                                        void *output_buffer_element_ptr, const struct params *run_params);

// Rate transformers
int transform_output_OutflowRate(const struct GALAXY *galaxy, property_id_t output_prop_id, 
                              void *output_buffer_element_ptr, const struct params *run_params);

// SFR transformers (derive scalar from arrays)
int derive_output_SfrDisk(const struct GALAXY *galaxy, property_id_t output_prop_id, 
                       void *output_buffer_element_ptr, const struct params *run_params);

int derive_output_SfrBulge(const struct GALAXY *galaxy, property_id_t output_prop_id, 
                        void *output_buffer_element_ptr, const struct params *run_params);

// SFR metallicity transformers (derive from gas and metal arrays)
int derive_output_SfrDiskZ(const struct GALAXY *galaxy, property_id_t output_prop_id, 
                        void *output_buffer_element_ptr, const struct params *run_params);

int derive_output_SfrBulgeZ(const struct GALAXY *galaxy, property_id_t output_prop_id, 
                         void *output_buffer_element_ptr, const struct params *run_params);

// Infall property transformers (Type-based filtering for legacy compatibility)
int transform_output_infallMvir(const struct GALAXY *galaxy, property_id_t output_prop_id, 
                               void *output_buffer_element_ptr, const struct params *run_params);

int transform_output_infallVvir(const struct GALAXY *galaxy, property_id_t output_prop_id, 
                               void *output_buffer_element_ptr, const struct params *run_params);

int transform_output_infallVmax(const struct GALAXY *galaxy, property_id_t output_prop_id, 
                               void *output_buffer_element_ptr, const struct params *run_params);

#endif /* PHYSICS_OUTPUT_TRANSFORMERS_H */