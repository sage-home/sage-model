#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "config_legacy.h"
#include "../memory.h"
#include "../core_mymalloc.h"

// Constants from original implementation
enum datatypes {
    DOUBLE = 1,
    STRING = 2,
    INT = 3
};

#define MAXTAGS          300  /* Max number of parameters */
#define MAXTAGLEN         50  /* Max number of characters in the string param tags */

// String to enum conversion functions
int string_to_tree_type(const char *tree_type_str) {
    if (!tree_type_str) return lhalo_binary;
    
    if (strcmp(tree_type_str, "lhalo_binary") == 0) return lhalo_binary;
    if (strcmp(tree_type_str, "lhalo_hdf5") == 0) return lhalo_hdf5;
    if (strcmp(tree_type_str, "genesis_hdf5") == 0) return genesis_hdf5;
    if (strcmp(tree_type_str, "consistent_trees_ascii") == 0) return consistent_trees_ascii;
    if (strcmp(tree_type_str, "consistent_trees_hdf5") == 0) return consistent_trees_hdf5;
    if (strcmp(tree_type_str, "gadget4_hdf5") == 0) return gadget4_hdf5;
    else {
        // Optional: Log a warning if verbose mode is on
        fprintf(stderr, "Warning: Unknown tree type '%s', defaulting to lhalo_binary.\n", tree_type_str);
        return lhalo_binary;
    }
}

int string_to_output_format(const char *output_format_str) {
    if (!output_format_str) return sage_binary;
    
    if (strcmp(output_format_str, "sage_binary") == 0) return sage_binary;
    if (strcmp(output_format_str, "sage_hdf5") == 0) return sage_hdf5;
    if (strcmp(output_format_str, "lhalo_binary_output") == 0) return lhalo_binary_output;
    else {
        // Optional: Log a warning if verbose mode is on
        fprintf(stderr, "Warning: Unknown output format '%s', defaulting to sage_binary.\n", output_format_str);
        return sage_binary;
    }
}

int string_to_forest_dist_scheme(const char *forest_dist_str) {
    if (!forest_dist_str) return uniform_in_forests;
    
    if (strcmp(forest_dist_str, "uniform_in_forests") == 0) return uniform_in_forests;
    if (strcmp(forest_dist_str, "linear_in_nhalos") == 0) return linear_in_nhalos;
    if (strcmp(forest_dist_str, "quadratic_in_nhalos") == 0) return quadratic_in_nhalos;
    if (strcmp(forest_dist_str, "exponent_in_nhalos") == 0) return exponent_in_nhalos;
    if (strcmp(forest_dist_str, "generic_power_in_nhalos") == 0) return generic_power_in_nhalos;
    else {
        // Optional: Log a warning if verbose mode is on
        fprintf(stderr, "Warning: Unknown forest distribution scheme '%s', defaulting to uniform_in_forests.\n", forest_dist_str);
        return uniform_in_forests;
    }
}

// Main legacy .par file reading function
int config_read_legacy_par(config_t *config, const char *filename) {
    if (!config || !filename) {
        return CONFIG_ERROR_INVALID_STATE;
    }
    
    // Allocate params structure
    config->params = sage_malloc(sizeof(struct params));
    if (!config->params) {
        snprintf(config->last_error, sizeof(config->last_error),
                "Failed to allocate memory for configuration parameters");
        return CONFIG_ERROR_MEMORY;
    }
    config->owns_params = true;
    
    // Use internal parameter reading function (extracted from existing code)
    int result = read_parameter_file_internal(filename, config->params);
    if (result != 0) {
        snprintf(config->last_error, sizeof(config->last_error),
                "Failed to read parameter file: %s", filename);
        sage_free(config->params);
        config->params = NULL;
        config->owns_params = false;
        return CONFIG_ERROR_PARSE;
    }
    
    config->format = CONFIG_FORMAT_LEGACY_PAR;
    strncpy(config->source_file, filename, sizeof(config->source_file) - 1);
    config->source_file[sizeof(config->source_file) - 1] = '\0';
    
    return CONFIG_SUCCESS;
}

// Internal parameter file reading function (refactored from original core_read_parameter_file.c)
int read_parameter_file_internal(const char *fname, struct params *run_params) {
    int errorFlag = 0;
    int *used_tag = 0;
    char my_treetype[MAX_STRING_LEN], my_outputformat[MAX_STRING_LEN], my_forest_dist_scheme[MAX_STRING_LEN];
    
    /*  recipe parameters  */
    int NParam = 0;
    char ParamTag[MAXTAGS][MAXTAGLEN + 1];
    int  ParamID[MAXTAGS];
    void *ParamAddr[MAXTAGS];

    /* Ensure that all strings will be NULL terminated */
    for(int i=0;i<MAXTAGS;i++) {
        ParamTag[i][MAXTAGLEN] = '\0';
    }

    NParam = 0;

#ifdef VERBOSE
    const int ThisTask = run_params->ThisTask;

    if(ThisTask == 0) {
        fprintf(stdout, "\nreading parameter file:\n\n");
    }
#endif

    // Set up parameter tags and addresses (extracted from original code)
    strncpy(ParamTag[NParam], "FileNameGalaxies", MAXTAGLEN);
    ParamAddr[NParam] = run_params->FileNameGalaxies;
    ParamID[NParam++] = STRING;

    strncpy(ParamTag[NParam], "OutputDir", MAXTAGLEN);
    ParamAddr[NParam] = run_params->OutputDir;
    ParamID[NParam++] = STRING;

    strncpy(ParamTag[NParam], "TreeType", MAXTAGLEN);
    ParamAddr[NParam] = my_treetype;
    ParamID[NParam++] = STRING;

    strncpy(ParamTag[NParam], "TreeName", MAXTAGLEN);
    ParamAddr[NParam] = run_params->TreeName;
    ParamID[NParam++] = STRING;

    strncpy(ParamTag[NParam], "SimulationDir", MAXTAGLEN);
    ParamAddr[NParam] = run_params->SimulationDir;
    ParamID[NParam++] = STRING;

    strncpy(ParamTag[NParam], "FileWithSnapList", MAXTAGLEN);
    ParamAddr[NParam] = run_params->FileWithSnapList;
    ParamID[NParam++] = STRING;

    strncpy(ParamTag[NParam], "LastSnapshotNr", MAXTAGLEN);
    ParamAddr[NParam] = &(run_params->LastSnapshotNr);
    ParamID[NParam++] = INT;

    strncpy(ParamTag[NParam], "FirstFile", MAXTAGLEN);
    ParamAddr[NParam] = &(run_params->FirstFile);
    ParamID[NParam++] = INT;

    strncpy(ParamTag[NParam], "LastFile", MAXTAGLEN);
    ParamAddr[NParam] = &(run_params->LastFile);
    ParamID[NParam++] = INT;

    strncpy(ParamTag[NParam], "NumSimulationTreeFiles", MAXTAGLEN);
    ParamAddr[NParam] = &(run_params->NumSimulationTreeFiles);
    ParamID[NParam++] = INT;

    strncpy(ParamTag[NParam], "ThreshMajorMerger", MAXTAGLEN);
    ParamAddr[NParam] = &(run_params->ThreshMajorMerger);
    ParamID[NParam++] = DOUBLE;

    strncpy(ParamTag[NParam], "RecycleFraction", MAXTAGLEN);
    ParamAddr[NParam] = &(run_params->RecycleFraction);
    ParamID[NParam++] = DOUBLE;

    strncpy(ParamTag[NParam], "ReIncorporationFactor", MAXTAGLEN);
    ParamAddr[NParam] = &(run_params->ReIncorporationFactor);
    ParamID[NParam++] = DOUBLE;

    strncpy(ParamTag[NParam], "UnitVelocity_in_cm_per_s", MAXTAGLEN);
    ParamAddr[NParam] = &(run_params->UnitVelocity_in_cm_per_s);
    ParamID[NParam++] = DOUBLE;

    strncpy(ParamTag[NParam], "UnitLength_in_cm", MAXTAGLEN);
    ParamAddr[NParam] = &(run_params->UnitLength_in_cm);
    ParamID[NParam++] = DOUBLE;

    strncpy(ParamTag[NParam], "UnitMass_in_g", MAXTAGLEN);
    ParamAddr[NParam] = &(run_params->UnitMass_in_g);
    ParamID[NParam++] = DOUBLE;

    strncpy(ParamTag[NParam], "Hubble_h", MAXTAGLEN);
    ParamAddr[NParam] = &(run_params->Hubble_h);
    ParamID[NParam++] = DOUBLE;

    strncpy(ParamTag[NParam], "ReionizationOn", MAXTAGLEN);
    ParamAddr[NParam] = &(run_params->ReionizationOn);
    ParamID[NParam++] = INT;

    strncpy(ParamTag[NParam], "SupernovaRecipeOn", MAXTAGLEN);
    ParamAddr[NParam] = &(run_params->SupernovaRecipeOn);
    ParamID[NParam++] = INT;

    strncpy(ParamTag[NParam], "DiskInstabilityOn", MAXTAGLEN);
    ParamAddr[NParam] = &(run_params->DiskInstabilityOn);
    ParamID[NParam++] = INT;

    strncpy(ParamTag[NParam], "SFprescription", MAXTAGLEN);
    ParamAddr[NParam] = &(run_params->SFprescription);
    ParamID[NParam++] = INT;

    strncpy(ParamTag[NParam], "AGNrecipeOn", MAXTAGLEN);
    ParamAddr[NParam] = &(run_params->AGNrecipeOn);
    ParamID[NParam++] = INT;

    strncpy(ParamTag[NParam], "BaryonFrac", MAXTAGLEN);
    ParamAddr[NParam] = &(run_params->BaryonFrac);
    ParamID[NParam++] = DOUBLE;

    strncpy(ParamTag[NParam], "Omega", MAXTAGLEN);
    ParamAddr[NParam] = &(run_params->Omega);
    ParamID[NParam++] = DOUBLE;

    strncpy(ParamTag[NParam], "OmegaLambda", MAXTAGLEN);
    ParamAddr[NParam] = &(run_params->OmegaLambda);
    ParamID[NParam++] = DOUBLE;

    strncpy(ParamTag[NParam], "PartMass", MAXTAGLEN);
    ParamAddr[NParam] = &(run_params->PartMass);
    ParamID[NParam++] = DOUBLE;

    strncpy(ParamTag[NParam], "BoxSize", MAXTAGLEN);
    ParamAddr[NParam] = &(run_params->BoxSize);
    ParamID[NParam++] = DOUBLE;

    strncpy(ParamTag[NParam], "EnergySN", MAXTAGLEN);
    ParamAddr[NParam] = &(run_params->EnergySN);
    ParamID[NParam++] = DOUBLE;

    strncpy(ParamTag[NParam], "EtaSN", MAXTAGLEN);
    ParamAddr[NParam] = &(run_params->EtaSN);
    ParamID[NParam++] = DOUBLE;

    strncpy(ParamTag[NParam], "Yield", MAXTAGLEN);
    ParamAddr[NParam] = &(run_params->Yield);
    ParamID[NParam++] = DOUBLE;

    strncpy(ParamTag[NParam], "FracZleaveDisk", MAXTAGLEN);
    ParamAddr[NParam] = &(run_params->FracZleaveDisk);
    ParamID[NParam++] = DOUBLE;

    strncpy(ParamTag[NParam], "SfrEfficiency", MAXTAGLEN);
    ParamAddr[NParam] = &(run_params->SfrEfficiency);
    ParamID[NParam++] = DOUBLE;

    strncpy(ParamTag[NParam], "FeedbackReheatingEpsilon", MAXTAGLEN);
    ParamAddr[NParam] = &(run_params->FeedbackReheatingEpsilon);
    ParamID[NParam++] = DOUBLE;

    strncpy(ParamTag[NParam], "FeedbackEjectionEfficiency", MAXTAGLEN);
    ParamAddr[NParam] = &(run_params->FeedbackEjectionEfficiency);
    ParamID[NParam++] = DOUBLE;

    strncpy(ParamTag[NParam], "BlackHoleGrowthRate", MAXTAGLEN);
    ParamAddr[NParam] = &(run_params->BlackHoleGrowthRate);
    ParamID[NParam++] = DOUBLE;

    strncpy(ParamTag[NParam], "RadioModeEfficiency", MAXTAGLEN);
    ParamAddr[NParam] = &(run_params->RadioModeEfficiency);
    ParamID[NParam++] = DOUBLE;

    strncpy(ParamTag[NParam], "QuasarModeEfficiency", MAXTAGLEN);
    ParamAddr[NParam] = &(run_params->QuasarModeEfficiency);
    ParamID[NParam++] = DOUBLE;

    strncpy(ParamTag[NParam], "Reionization_z0", MAXTAGLEN);
    ParamAddr[NParam] = &(run_params->Reionization_z0);
    ParamID[NParam++] = DOUBLE;

    strncpy(ParamTag[NParam], "Reionization_zr", MAXTAGLEN);
    ParamAddr[NParam] = &(run_params->Reionization_zr);
    ParamID[NParam++] = DOUBLE;

    strncpy(ParamTag[NParam], "ThresholdSatDisruption", MAXTAGLEN);
    ParamAddr[NParam] = &(run_params->ThresholdSatDisruption);
    ParamID[NParam++] = DOUBLE;

    strncpy(ParamTag[NParam], "NumOutputs", MAXTAGLEN);
    ParamAddr[NParam] = &(run_params->NumSnapOutputs);
    ParamID[NParam++] = INT;

    strncpy(ParamTag[NParam], "OutputFormat", MAXTAGLEN);
    ParamAddr[NParam] = my_outputformat;
    ParamID[NParam++] = STRING;

    strncpy(ParamTag[NParam], "ForestDistributionScheme", MAXTAGLEN);
    ParamAddr[NParam] = my_forest_dist_scheme;
    ParamID[NParam++] = STRING;

    strncpy(ParamTag[NParam], "ExponentForestDistributionScheme", MAXTAGLEN);
    ParamAddr[NParam] = &(run_params->Exponent_Forest_Dist_Scheme);
    ParamID[NParam++] = DOUBLE;

    used_tag = mymalloc(sizeof(int) * NParam);
    for(int i=0; i<NParam; i++) {
        used_tag[i]=1;
    }

    FILE *fd = fopen(fname, "r");
    if (fd == NULL) {
        fprintf(stderr,"Parameter file '%s' not found.\n", fname);
        myfree(used_tag);
        return FILE_NOT_FOUND;
    }

    char buffer[MAX_STRING_LEN];
    while(fgets(&(buffer[0]), MAX_STRING_LEN, fd) != NULL) {
        char buf1[MAX_STRING_LEN], buf2[MAX_STRING_LEN];
        char fmt[MAX_STRING_LEN];
        snprintf(fmt, MAX_STRING_LEN, "%%%ds %%%ds[^\n]", MAX_STRING_LEN-1, MAX_STRING_LEN-1);
        if(sscanf(buffer, fmt, buf1, buf2) < 2) {
            continue;
        }

        if(buf1[0] == '%' || buf1[0] == '-') { /* the second condition is checking for output snapshots -- that line starts with "->" */
            continue;
        }

        /* Allowing for spaces in the filenames (but requires comments to ALWAYS start with '%' or ';') */
        int buf2len = strnlen(buf2, MAX_STRING_LEN-1);
        for(int i=0;i<=buf2len;i++) {
            if(buf2[i] == '%' || buf2[i] == ';' || buf2[i] == '#') {
                int null_pos = i;
                //Ignore all preceeding whitespace
                for(int j=i-1;j>=0;j--) {
                    null_pos = isblank(buf2[j]) ? j:null_pos;
                }
                buf2[null_pos] = '\0';
                break;
            }
        }
        buf2len = strnlen(buf2, MAX_STRING_LEN-1);
        while(buf2len > 0 && isblank(buf2[buf2len-1])) {
            buf2len--;
        }
        buf2[buf2len] = '\0';

        int j=-1;
        for(int i = 0; i < NParam; i++) {
            if(strncasecmp(buf1, ParamTag[i], MAX_STRING_LEN-1) == 0) {
                j = i;
                ParamTag[i][0] = 0;
                used_tag[i] = 0;
                break;
            }
        }

        if(j >= 0) {
#ifdef VERBOSE
            if(run_params->ThisTask == 0) {
                fprintf(stdout, "%35s\t%10s\n", buf1, buf2);
            }
#endif

            switch (ParamID[j])
                {
                case DOUBLE:
                    *((double *) ParamAddr[j]) = atof(buf2);
                    break;
                case STRING:
                    snprintf(ParamAddr[j], MAX_STRING_LEN, "%s", buf2);
                    break;
                case INT:
                    *((int *) ParamAddr[j]) = atoi(buf2);
                    break;
                }
        } else {
            fprintf(stderr, "Error in file %s:   Tag '%s' not allowed or multiply defined.\n", fname, buf1);
            errorFlag = 1;
        }
    }
    fclose(fd);

    const size_t outlen = strlen(run_params->OutputDir);
    if(outlen > 0) {
        if(run_params->OutputDir[outlen - 1] != '/')
            strcat(run_params->OutputDir, "/");
    }

    for(int i = 0; i < NParam; i++) {
        if(used_tag[i]) {
            fprintf(stderr, "Error. I miss a value for tag '%s' in parameter file '%s'.\n", ParamTag[i], fname);
            errorFlag = 1;
        }
    }

    if(errorFlag) {
        myfree(used_tag);
        return 1;
    }
#ifdef VERBOSE
    fprintf(stdout, "\n");
#endif

    // Convert string enums to enum values
    run_params->TreeType = string_to_tree_type(my_treetype);
    run_params->OutputFormat = string_to_output_format(my_outputformat);
    run_params->ForestDistributionScheme = string_to_forest_dist_scheme(my_forest_dist_scheme);

    if( ! (run_params->LastSnapshotNr+1 > 0 && run_params->LastSnapshotNr+1 < ABSOLUTEMAXSNAPS) ) {
        fprintf(stderr,"LastSnapshotNr = %d should be in [0, %d) \n", run_params->LastSnapshotNr, ABSOLUTEMAXSNAPS);
        myfree(used_tag);
        return 1;
    }
    run_params->SimMaxSnaps = run_params->LastSnapshotNr + 1;

    if(!(run_params->NumSnapOutputs == -1 || (run_params->NumSnapOutputs > 0 && run_params->NumSnapOutputs <= ABSOLUTEMAXSNAPS))) {
        fprintf(stderr,"NumOutputs must be -1 or between 1 and %i\n", ABSOLUTEMAXSNAPS);
        myfree(used_tag);
        return 1;
    }

    // read in the output snapshot list
    if(run_params->NumSnapOutputs == -1) {
        run_params->NumSnapOutputs = run_params->SimMaxSnaps;
        for (int i=run_params->NumSnapOutputs-1; i>=0; i--) {
            run_params->ListOutputSnaps[i] = i;
        }
#ifdef VERBOSE
        if(run_params->ThisTask == 0) {
            fprintf(stdout, "all %d snapshots selected for output\n", run_params->NumSnapOutputs);
        }
#endif
    } else {
#ifdef VERBOSE
        if(run_params->ThisTask == 0) {
            fprintf(stdout, "%d snapshots selected for output: ", run_params->NumSnapOutputs);
        }
#endif

        // reopen the parameter file
        fd = fopen(fname, "r");

        int done = 0;
        while(!feof(fd) && !done) {
            char buf[MAX_STRING_LEN];

            /* scan down to find the line with the snapshots */
            if(fscanf(fd, "%s", buf) == 0) continue;
            if(strcmp(buf, "->") == 0) {
                // read the snapshots into ListOutputSnaps
                for(int i=0; i<run_params->NumSnapOutputs; i++) {
                    if(fscanf(fd, "%d", &(run_params->ListOutputSnaps[i])) == 1) {
#ifdef VERBOSE
                        if(run_params->ThisTask == 0) {
                            fprintf(stdout, "%d ", run_params->ListOutputSnaps[i]);
                        }
#endif
                    }
                }
                done = 1;
                break;
            }
        }

        fclose(fd);
        if(! done ) {
            fprintf(stderr,"Error: Could not properly parse output snapshots\n");
            myfree(used_tag);
            return 2;
        }
#ifdef VERBOSE
        fprintf(stdout, "\n");
#endif
    }

    myfree(used_tag);
    return 0;
}