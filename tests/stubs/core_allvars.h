#ifndef CORE_ALLVARS_STUBS_H
#define CORE_ALLVARS_STUBS_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define MAX_STRING_LEN 512
#define STEPS 20

struct GALAXY
{
    int32_t SnapNum;
    int32_t Type;
    
    int32_t GalaxyNr;
    int32_t CentralGal;
    int32_t HaloNr;
    long long MostBoundID;
    
    int32_t mergeType;
    int32_t mergeIntoID;
    int32_t mergeIntoSnapNum;
    float dT;
    
    /* (sub)halo properties */
    float Pos[3];
    float Vel[3];
    int Len;
    float Mvir;
    float deltaMvir;
    float CentralMvir;
    float Rvir;
    float Vvir;
    float Vmax;
    
    /* baryonic reservoirs */
    float ColdGas;
    float StellarMass;
    float BulgeMass;
    float HotGas;
    float EjectedMass;
    float BlackHoleMass;
    float ICS;
    
    /* Extension mechanism - placed at the end for binary compatibility */
    void **extension_data;        /* Array of pointers to module-specific data */
    int num_extensions;           /* Number of registered extensions */
    uint64_t extension_flags;     /* Bitmap to track which extensions are in use */
};

#endif /* CORE_ALLVARS_STUBS_H */
