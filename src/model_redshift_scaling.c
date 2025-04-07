#include "core_allvars.h"
#include "model_redshift_scaling.h"
#include <math.h>

// Scale a parameter according to the selected method
double scale_parameter(double base_param, int scaling_method, double scaling_param, double redshift) {
    switch(scaling_method) {
        case SCALING_POWER_LAW:
            return base_param * pow(1.0 + redshift, scaling_param);
        
        case SCALING_EXPONENTIAL:
            return base_param * exp(scaling_param * redshift);
            
        case SCALING_NONE:
        default:
            return base_param;
    }
}

// Apply redshift scaling to star formation efficiency
double get_redshift_scaled_sf_efficiency(const struct params *run_params, double redshift) {
    if (!run_params->ScaleSfrEfficiency)
        return run_params->SfrEfficiency;
        
    return scale_parameter(
        run_params->SfrEfficiency,
        run_params->SfrScalingMethod,
        run_params->SfrRedshiftScaling,
        redshift
    );
}

// Apply redshift scaling to feedback ejection
double get_redshift_scaled_feedback_ejection(const struct params *run_params, double redshift) {
    if (!run_params->ScaleFeedbackEjection)
        return run_params->FeedbackEjectionEfficiency;
        
    return scale_parameter(
        run_params->FeedbackEjectionEfficiency,
        run_params->FeedbackScalingMethod,
        run_params->FeedbackRedshiftScaling,
        redshift
    );
}

// Apply redshift scaling to gas reincorporation
double get_redshift_scaled_reincorp_factor(const struct params *run_params, double redshift) {
    if (!run_params->ScaleReIncorporation)
        return run_params->ReIncorporationFactor;
        
    return scale_parameter(
        run_params->ReIncorporationFactor,
        run_params->ReIncorpScalingMethod,
        run_params->ReIncorpRedshiftScaling,
        redshift
    );
}

// Apply redshift scaling to quasar mode efficiency
double get_redshift_scaled_quasar_efficiency(const struct params *run_params, double redshift) {
    if (!run_params->ScaleQuasarRadioModes)
        return run_params->QuasarModeEfficiency;
        
    return scale_parameter(
        run_params->QuasarModeEfficiency,
        run_params->QuasarRadioScalingMethod,
        run_params->QuasarRedshiftScaling,
        redshift
    );
}

// Apply redshift scaling to radio mode efficiency
double get_redshift_scaled_radio_efficiency(const struct params *run_params, double redshift) {
    if (!run_params->ScaleQuasarRadioModes)
        return run_params->RadioModeEfficiency;
        
    return scale_parameter(
        run_params->RadioModeEfficiency,
        run_params->QuasarRadioScalingMethod,
        run_params->RadioModeRedshiftScaling,
        redshift
    );
}

// Initialize default values for all scaling parameters
void init_redshift_scaling_params(struct params *run_params) {
    // Default: no scaling (all works as original model)
    run_params->ScaleSfrEfficiency = 0;
    run_params->ScaleFeedbackEjection = 0;
    run_params->ScaleReIncorporation = 0;
    run_params->ScaleQuasarRadioModes = 0;
    
    // Default to power law scaling 
    run_params->SfrScalingMethod = SCALING_POWER_LAW;
    run_params->FeedbackScalingMethod = SCALING_POWER_LAW;
    run_params->ReIncorpScalingMethod = SCALING_POWER_LAW;
    run_params->QuasarRadioScalingMethod = SCALING_POWER_LAW;
    
    // Default scaling parameters (no effect if scaling is disabled)
    // Values chosen to be neutral (power of 0 = no change)
    run_params->SfrRedshiftScaling = 0.0;
    run_params->FeedbackRedshiftScaling = 0.0;
    run_params->ReIncorpRedshiftScaling = 0.0;
    run_params->QuasarRedshiftScaling = 0.0;
    run_params->RadioModeRedshiftScaling = 0.0;
}