#pragma once

#ifdef __cplusplus
extern "C" {
#endif

    #include "core_allvars.h"

    /*
     * Parameter view structures
     *
     * These structures provide focused views of the parameters needed by
     * each physics module. Each view contains only the parameters relevant
     * to its specific module, reducing coupling and improving modularity.
     */

    /*
     * Cooling parameters view
     *
     * Contains parameters needed for gas cooling and heating calculations.
     */
    struct cooling_params_view {
        /* Cosmology parameters needed for cooling */
        double Omega;
        double OmegaLambda;
        double Hubble_h;
        
        /* Physics flags and parameters */
        int32_t AGNrecipeOn;
        double RadioModeEfficiency;
        
        /* Units needed for cooling calculations */
        double UnitDensity_in_cgs;
        double UnitTime_in_s;
        double UnitEnergy_in_cgs;
        double UnitMass_in_g;
        
        /* Pointer back to full params for occasional needs */
        const struct params *full_params;
    };

    /*
     * Star formation parameters view
     *
     * Contains parameters needed for star formation calculations.
     */
    struct star_formation_params_view {
        /* Physics flags and parameters for star formation */
        int32_t SFprescription;
        double SfrEfficiency;
        double RecycleFraction;
        double Yield;
        double FracZleaveDisk;
        
        /* Pointer back to full params */
        const struct params *full_params;
    };

    /*
     * Feedback parameters view
     *
     * Contains parameters needed for supernova feedback calculations.
     */
    struct feedback_params_view {
        /* Physics flags and parameters for feedback */
        int32_t SupernovaRecipeOn;
        double FeedbackReheatingEpsilon;
        double FeedbackEjectionEfficiency;
        double EnergySNcode;
        double EtaSNcode;
        
        /* Pointer back to full params */
        const struct params *full_params;
    };

    /*
     * AGN parameters view
     *
     * Contains parameters needed for AGN feedback calculations.
     */
    struct agn_params_view {
        /* Physics parameters for AGN */
        int32_t AGNrecipeOn;
        double RadioModeEfficiency;
        double QuasarModeEfficiency;
        double BlackHoleGrowthRate;
        
        /* Units needed for AGN calculations */
        double UnitMass_in_g;
        double UnitTime_in_s;
        double UnitEnergy_in_cgs;
        
        /* Pointer back to full params */
        const struct params *full_params;
    };

    /*
     * Merger parameters view
     *
     * Contains parameters needed for galaxy merger calculations.
     */
    struct merger_params_view {
        /* Physics parameters for mergers */
        double ThreshMajorMerger;
        double ThresholdSatDisruption;
        
        /* Pointer back to full params */
        const struct params *full_params;
    };

    /*
     * Reincorporation parameters view
     *
     * Contains parameters needed for gas reincorporation calculations.
     */
    struct reincorporation_params_view {
        /* Physics parameters for reincorporation */
        double ReIncorporationFactor;
        
        /* Pointer back to full params */
        const struct params *full_params;
    };

    /*
     * Disk instability parameters view
     *
     * Contains parameters needed for disk instability calculations.
     */
    struct disk_instability_params_view {
        /* Physics flags for disk instability */
        int32_t DiskInstabilityOn;
        
        /* Pointer back to full params */
        const struct params *full_params;
    };

    /* Accessor functions to initialize parameter views */
    extern void initialize_cooling_params_view(struct cooling_params_view *view, const struct params *params);
    extern void initialize_star_formation_params_view(struct star_formation_params_view *view, const struct params *params);
    extern void initialize_feedback_params_view(struct feedback_params_view *view, const struct params *params);
    extern void initialize_agn_params_view(struct agn_params_view *view, const struct params *params);
    extern void initialize_merger_params_view(struct merger_params_view *view, const struct params *params);
    extern void initialize_reincorporation_params_view(struct reincorporation_params_view *view, const struct params *params);
    extern void initialize_disk_instability_params_view(struct disk_instability_params_view *view, const struct params *params);
    
    /* Validation functions for parameter views */
    extern bool validate_cooling_params_view(const struct cooling_params_view *view);
    extern bool validate_star_formation_params_view(const struct star_formation_params_view *view);
    extern bool validate_feedback_params_view(const struct feedback_params_view *view);
    extern bool validate_agn_params_view(const struct agn_params_view *view);
    extern bool validate_merger_params_view(const struct merger_params_view *view);
    extern bool validate_reincorporation_params_view(const struct reincorporation_params_view *view);
    extern bool validate_disk_instability_params_view(const struct disk_instability_params_view *view);

#ifdef __cplusplus
}
#endif
