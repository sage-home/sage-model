/**
 * @file placeholder_hdf5_macros.h
 * @brief Placeholder macros for physics properties in core-only mode
 *
 * This file provides placeholder macros for physics properties that
 * aren't defined in the core-only mode. These macros simply return
 * default values to allow HDF5 output to compile in core-only builds.
 */

#ifndef PLACEHOLDER_HDF5_MACROS_H
#define PLACEHOLDER_HDF5_MACROS_H

struct GALAXY;

/* Define placeholder macros that return 0 or 0.0 for physics properties */
#define GALAXY_PROP_ColdGas(g) 0.0f
#define GALAXY_PROP_StellarMass(g) 0.0f
#define GALAXY_PROP_BulgeMass(g) 0.0f
#define GALAXY_PROP_HotGas(g) 0.0f
#define GALAXY_PROP_EjectedMass(g) 0.0f
#define GALAXY_PROP_BlackHoleMass(g) 0.0f
#define GALAXY_PROP_DiskScaleRadius(g) 0.0f
#define GALAXY_PROP_Cooling(g) 0.0
#define GALAXY_PROP_Heating(g) 0.0
#define GALAXY_PROP_TimeOfLastMajorMerger(g) 0.0f
#define GALAXY_PROP_TimeOfLastMinorMerger(g) 0.0f

#endif /* PLACEHOLDER_HDF5_MACROS_H */