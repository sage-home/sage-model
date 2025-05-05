#include "core_properties_sync.h"
#include "core_properties.h"    // For GALAXY_PROP_* macros and PROPERTY_META
#include "core_allvars.h"       // For struct GALAXY definition with direct fields
#include "core_logging.h"       // For logging errors/debug messages
#include <string.h>             // For memcpy

void sync_direct_to_properties(struct GALAXY *galaxy) {
    if (galaxy == NULL) {
        LOG_ERROR("sync_direct_to_properties: galaxy pointer is NULL.");
        return;
    }
    if (galaxy->properties == NULL) {
        LOG_ERROR("sync_direct_to_properties: galaxy->properties pointer is NULL for GalaxyNr %d.", galaxy->GalaxyNr);
        return;
    }
    LOG_DEBUG("Syncing direct fields -> properties for GalaxyNr %d", galaxy->GalaxyNr);

    // Basic properties
    GALAXY_PROP_StellarMass(galaxy) = galaxy->StellarMass;
    GALAXY_PROP_ColdGas(galaxy) = galaxy->ColdGas;
    GALAXY_PROP_BulgeMass(galaxy) = galaxy->BulgeMass;
    GALAXY_PROP_HotGas(galaxy) = galaxy->HotGas;
    GALAXY_PROP_EjectedMass(galaxy) = galaxy->EjectedMass;
    GALAXY_PROP_BlackHoleMass(galaxy) = galaxy->BlackHoleMass;
    GALAXY_PROP_ICS(galaxy) = galaxy->ICS;
    
    // Metals
    GALAXY_PROP_MetalsColdGas(galaxy) = galaxy->MetalsColdGas;
    GALAXY_PROP_MetalsStellarMass(galaxy) = galaxy->MetalsStellarMass;
    GALAXY_PROP_MetalsBulgeMass(galaxy) = galaxy->MetalsBulgeMass;
    GALAXY_PROP_MetalsHotGas(galaxy) = galaxy->MetalsHotGas;
    GALAXY_PROP_MetalsEjectedMass(galaxy) = galaxy->MetalsEjectedMass;
    GALAXY_PROP_MetalsICS(galaxy) = galaxy->MetalsICS;
    
    // Galaxy properties
    GALAXY_PROP_Type(galaxy) = galaxy->Type;
    GALAXY_PROP_SnapNum(galaxy) = galaxy->SnapNum;
    GALAXY_PROP_GalaxyNr(galaxy) = galaxy->GalaxyNr;
    GALAXY_PROP_CentralGal(galaxy) = galaxy->CentralGal;
    GALAXY_PROP_HaloNr(galaxy) = galaxy->HaloNr;
    GALAXY_PROP_MostBoundID(galaxy) = galaxy->MostBoundID;
    GALAXY_PROP_GalaxyIndex(galaxy) = galaxy->GalaxyIndex;
    GALAXY_PROP_CentralGalaxyIndex(galaxy) = galaxy->CentralGalaxyIndex;
    GALAXY_PROP_mergeType(galaxy) = galaxy->mergeType;
    GALAXY_PROP_mergeIntoID(galaxy) = galaxy->mergeIntoID;
    GALAXY_PROP_mergeIntoSnapNum(galaxy) = galaxy->mergeIntoSnapNum;
    GALAXY_PROP_dT(galaxy) = galaxy->dT;
    
    // Halo properties
    GALAXY_PROP_Len(galaxy) = galaxy->Len;
    GALAXY_PROP_Mvir(galaxy) = galaxy->Mvir;
    GALAXY_PROP_deltaMvir(galaxy) = galaxy->deltaMvir;
    GALAXY_PROP_CentralMvir(galaxy) = galaxy->CentralMvir;
    GALAXY_PROP_Rvir(galaxy) = galaxy->Rvir;
    GALAXY_PROP_Vvir(galaxy) = galaxy->Vvir;
    GALAXY_PROP_Vmax(galaxy) = galaxy->Vmax;
    
    // Position and velocity (fixed size arrays)
    for (int i = 0; i < 3; i++) {
        GALAXY_PROP_Pos_ELEM(galaxy, i) = galaxy->Pos[i];
        GALAXY_PROP_Vel_ELEM(galaxy, i) = galaxy->Vel[i];
    }
    
    // Star formation histories (fixed size arrays with STEPS elements)
    for (int i = 0; i < STEPS; i++) {
        GALAXY_PROP_SfrDisk_ELEM(galaxy, i) = galaxy->SfrDisk[i];
        GALAXY_PROP_SfrBulge_ELEM(galaxy, i) = galaxy->SfrBulge[i];
        GALAXY_PROP_SfrDiskColdGas_ELEM(galaxy, i) = galaxy->SfrDiskColdGas[i];
        GALAXY_PROP_SfrDiskColdGasMetals_ELEM(galaxy, i) = galaxy->SfrDiskColdGasMetals[i];
        GALAXY_PROP_SfrBulgeColdGas_ELEM(galaxy, i) = galaxy->SfrBulgeColdGas[i];
        GALAXY_PROP_SfrBulgeColdGasMetals_ELEM(galaxy, i) = galaxy->SfrBulgeColdGasMetals[i];
    }
    
    // Misc properties
    GALAXY_PROP_DiskScaleRadius(galaxy) = galaxy->DiskScaleRadius;
    GALAXY_PROP_MergTime(galaxy) = galaxy->MergTime;
    GALAXY_PROP_Cooling(galaxy) = galaxy->Cooling;
    GALAXY_PROP_Heating(galaxy) = galaxy->Heating;
    GALAXY_PROP_r_heat(galaxy) = galaxy->r_heat;
    GALAXY_PROP_QuasarModeBHaccretionMass(galaxy) = galaxy->QuasarModeBHaccretionMass;
    GALAXY_PROP_TimeOfLastMajorMerger(galaxy) = galaxy->TimeOfLastMajorMerger;
    GALAXY_PROP_TimeOfLastMinorMerger(galaxy) = galaxy->TimeOfLastMinorMerger;
    GALAXY_PROP_OutflowRate(galaxy) = galaxy->OutflowRate;
    GALAXY_PROP_TotalSatelliteBaryons(galaxy) = galaxy->TotalSatelliteBaryons;
    
    // Infall properties
    GALAXY_PROP_infallMvir(galaxy) = galaxy->infallMvir;
    GALAXY_PROP_infallVvir(galaxy) = galaxy->infallVvir;
    GALAXY_PROP_infallVmax(galaxy) = galaxy->infallVmax;
    
    // Note: Dynamic arrays defined only in the properties struct don't need syncing in this direction.
}

void sync_properties_to_direct(struct GALAXY *galaxy) {
    if (galaxy == NULL) {
        LOG_ERROR("sync_properties_to_direct: galaxy pointer is NULL.");
        return;
    }
    if (galaxy->properties == NULL) {
        LOG_ERROR("sync_properties_to_direct: galaxy->properties pointer is NULL for GalaxyNr %d.", galaxy->GalaxyNr);
        return;
    }
    LOG_DEBUG("Syncing properties -> direct fields for GalaxyNr %d", galaxy->GalaxyNr);

    // Basic properties
    galaxy->StellarMass = GALAXY_PROP_StellarMass(galaxy);
    galaxy->ColdGas = GALAXY_PROP_ColdGas(galaxy);
    galaxy->BulgeMass = GALAXY_PROP_BulgeMass(galaxy);
    galaxy->HotGas = GALAXY_PROP_HotGas(galaxy);
    galaxy->EjectedMass = GALAXY_PROP_EjectedMass(galaxy);
    galaxy->BlackHoleMass = GALAXY_PROP_BlackHoleMass(galaxy);
    galaxy->ICS = GALAXY_PROP_ICS(galaxy);
    
    // Metals
    galaxy->MetalsColdGas = GALAXY_PROP_MetalsColdGas(galaxy);
    galaxy->MetalsStellarMass = GALAXY_PROP_MetalsStellarMass(galaxy);
    galaxy->MetalsBulgeMass = GALAXY_PROP_MetalsBulgeMass(galaxy);
    galaxy->MetalsHotGas = GALAXY_PROP_MetalsHotGas(galaxy);
    galaxy->MetalsEjectedMass = GALAXY_PROP_MetalsEjectedMass(galaxy);
    galaxy->MetalsICS = GALAXY_PROP_MetalsICS(galaxy);
    
    // Galaxy properties
    galaxy->Type = GALAXY_PROP_Type(galaxy);
    galaxy->SnapNum = GALAXY_PROP_SnapNum(galaxy);
    galaxy->GalaxyNr = GALAXY_PROP_GalaxyNr(galaxy);
    galaxy->CentralGal = GALAXY_PROP_CentralGal(galaxy);
    galaxy->HaloNr = GALAXY_PROP_HaloNr(galaxy);
    galaxy->MostBoundID = GALAXY_PROP_MostBoundID(galaxy);
    galaxy->GalaxyIndex = GALAXY_PROP_GalaxyIndex(galaxy);
    galaxy->CentralGalaxyIndex = GALAXY_PROP_CentralGalaxyIndex(galaxy);
    galaxy->mergeType = GALAXY_PROP_mergeType(galaxy);
    galaxy->mergeIntoID = GALAXY_PROP_mergeIntoID(galaxy);
    galaxy->mergeIntoSnapNum = GALAXY_PROP_mergeIntoSnapNum(galaxy);
    galaxy->dT = GALAXY_PROP_dT(galaxy);
    
    // Halo properties
    galaxy->Len = GALAXY_PROP_Len(galaxy);
    galaxy->Mvir = GALAXY_PROP_Mvir(galaxy);
    galaxy->deltaMvir = GALAXY_PROP_deltaMvir(galaxy);
    galaxy->CentralMvir = GALAXY_PROP_CentralMvir(galaxy);
    galaxy->Rvir = GALAXY_PROP_Rvir(galaxy);
    galaxy->Vvir = GALAXY_PROP_Vvir(galaxy);
    galaxy->Vmax = GALAXY_PROP_Vmax(galaxy);
    
    // Position and velocity (fixed size arrays)
    for (int i = 0; i < 3; i++) {
        galaxy->Pos[i] = GALAXY_PROP_Pos_ELEM(galaxy, i);
        galaxy->Vel[i] = GALAXY_PROP_Vel_ELEM(galaxy, i);
    }
    
    // Star formation histories (fixed size arrays with STEPS elements)
    for (int i = 0; i < STEPS; i++) {
        galaxy->SfrDisk[i] = GALAXY_PROP_SfrDisk_ELEM(galaxy, i);
        galaxy->SfrBulge[i] = GALAXY_PROP_SfrBulge_ELEM(galaxy, i);
        galaxy->SfrDiskColdGas[i] = GALAXY_PROP_SfrDiskColdGas_ELEM(galaxy, i);
        galaxy->SfrDiskColdGasMetals[i] = GALAXY_PROP_SfrDiskColdGasMetals_ELEM(galaxy, i);
        galaxy->SfrBulgeColdGas[i] = GALAXY_PROP_SfrBulgeColdGas_ELEM(galaxy, i);
        galaxy->SfrBulgeColdGasMetals[i] = GALAXY_PROP_SfrBulgeColdGasMetals_ELEM(galaxy, i);
    }
    
    // Misc properties
    galaxy->DiskScaleRadius = GALAXY_PROP_DiskScaleRadius(galaxy);
    galaxy->MergTime = GALAXY_PROP_MergTime(galaxy);
    galaxy->Cooling = GALAXY_PROP_Cooling(galaxy);
    galaxy->Heating = GALAXY_PROP_Heating(galaxy);
    galaxy->r_heat = GALAXY_PROP_r_heat(galaxy);
    galaxy->QuasarModeBHaccretionMass = GALAXY_PROP_QuasarModeBHaccretionMass(galaxy);
    galaxy->TimeOfLastMajorMerger = GALAXY_PROP_TimeOfLastMajorMerger(galaxy);
    galaxy->TimeOfLastMinorMerger = GALAXY_PROP_TimeOfLastMinorMerger(galaxy);
    galaxy->OutflowRate = GALAXY_PROP_OutflowRate(galaxy);
    galaxy->TotalSatelliteBaryons = GALAXY_PROP_TotalSatelliteBaryons(galaxy);
    
    // Infall properties
    galaxy->infallMvir = GALAXY_PROP_infallMvir(galaxy);
    galaxy->infallVvir = GALAXY_PROP_infallVvir(galaxy);
    galaxy->infallVmax = GALAXY_PROP_infallVmax(galaxy);
    
    // Note: Dynamic arrays defined only in the properties struct don't need syncing here.
}