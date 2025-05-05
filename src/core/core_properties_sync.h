#ifndef CORE_PROPERTIES_SYNC_H
#define CORE_PROPERTIES_SYNC_H

#include "core_allvars.h" // Needed for struct GALAXY definition

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Copies data from direct fields in struct GALAXY to the properties struct.
 * @param galaxy Pointer to the GALAXY structure.
 */
void sync_direct_to_properties(struct GALAXY *galaxy);

/**
 * @brief Copies data from the properties struct back to direct fields in struct GALAXY.
 * @param galaxy Pointer to the GALAXY structure.
 */
void sync_properties_to_direct(struct GALAXY *galaxy);

#ifdef __cplusplus
}
#endif

#endif // CORE_PROPERTIES_SYNC_H