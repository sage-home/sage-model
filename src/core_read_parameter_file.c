#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h> /* for isblank()*/

#include "core_allvars.h"
#include "core_mymalloc.h"

enum datatypes {
    DOUBLE = 1,
    STRING = 2,
    INT = 3
};

#define MAXTAGS          300  /* Max number of parameters */
#define MAXTAGLEN         50  /* Max number of characters in the string param tags */

int compare_ints_descending (const void* p1, const void* p2);

int compare_ints_descending (const void* p1, const void* p2)
{
    int i1 = *(int*) p1;
    int i2 = *(int*) p2;
    if (i1 < i2) {
        return 1;
    } else if (i1 == i2) {
        return 0;
    } else {
        return -1;
    }
 }

int read_parameter_file(const char *fname, struct params *run_params)
{
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

    strncpy(ParamTag[NParam], "MassLoadingOn", MAXTAGLEN);
    ParamAddr[NParam] = &(run_params->MassLoadingOn);
    ParamID[NParam++] = INT;

    strncpy(ParamTag[NParam], "DynamicalTimeResolutionFactor", MAXTAGLEN);
    ParamAddr[NParam] = &(run_params->DynamicalTimeResolutionFactor);
    ParamID[NParam++] = INT;

    used_tag = mymalloc(sizeof(int) * NParam);
    for(int i=0; i<NParam; i++) {
        used_tag[i]=1;
    }

    FILE *fd = fopen(fname, "r");
    if (fd == NULL) {
        fprintf(stderr,"Parameter file '%s' not found.\n", fname);
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
            if(ThisTask == 0) {
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
        ABORT(1);
    }
#ifdef VERBOSE
    fprintf(stdout, "\n");
#endif

    if( ! (run_params->LastSnapshotNr+1 > 0 && run_params->LastSnapshotNr+1 < ABSOLUTEMAXSNAPS) ) {
        fprintf(stderr,"LastSnapshotNr = %d should be in [0, %d) \n", run_params->LastSnapshotNr, ABSOLUTEMAXSNAPS);
        ABORT(1);
    }
    run_params->SimMaxSnaps = run_params->LastSnapshotNr + 1;

    if(!(run_params->NumSnapOutputs == -1 || (run_params->NumSnapOutputs > 0 && run_params->NumSnapOutputs <= ABSOLUTEMAXSNAPS))) {
        fprintf(stderr,"NumOutputs must be -1 or between 1 and %i\n", ABSOLUTEMAXSNAPS);
        ABORT(1);
    }

    // read in the output snapshot list
    if(run_params->NumSnapOutputs == -1) {
        run_params->NumSnapOutputs = run_params->SimMaxSnaps;
        for (int i=run_params->NumSnapOutputs-1; i>=0; i--) {
            run_params->ListOutputSnaps[i] = i;
        }
#ifdef VERBOSE
        if(ThisTask == 0) {
            fprintf(stdout, "all %d snapshots selected for output\n", run_params->NumSnapOutputs);
        }
#endif
    } else {
#ifdef VERBOSE
        if(ThisTask == 0) {
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
                        if(ThisTask == 0) {
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
            ABORT(2);
        }
#ifdef VERBOSE
        fprintf(stdout, "\n");
#endif
    }


    if(run_params->FirstFile < 0 || run_params->LastFile < 0 || run_params->LastFile < run_params->FirstFile) {
        fprintf(stderr,"Error: FirstFile = %d and LastFile = %d must both be >=0 *AND* LastFile "
                        "should be larger than   FirstFile.\nProbably a typo in the parameter-file. "
                        "Please change to appropriate values...exiting\n",
                        run_params->FirstFile, run_params->LastFile);
        ABORT(EXIT_FAILURE);
    }

    /* sort the output snapshot numbers in descending order (in case the user didn't do that already) MS: 24th Oct, 2023 */
    qsort(run_params->ListOutputSnaps, run_params->NumSnapOutputs, sizeof(run_params->ListOutputSnaps[0]), compare_ints_descending);

    /* Check for duplicate snapshot outputs */
    int num_dup_snaps = 0;
    for(int ii=1;ii<run_params->NumSnapOutputs;ii++) {
        const int dsnap = run_params->ListOutputSnaps[ii-1] - run_params->ListOutputSnaps[ii];
        if(dsnap == 0) {
            fprintf(stderr,"Error: Found duplicate snapshots in the list of desired output snapshots\n");
            fprintf(stderr,"Duplicate value = %d in position = %d (out of %d total output snapshots requested)\n",
                            run_params->ListOutputSnaps[ii], ii, run_params->NumSnapOutputs);
            num_dup_snaps++;
        }
    }
    if(num_dup_snaps != 0) {
        fprintf(stderr,"Error: Found %d duplicate snapshots - please remove them from the parameter file and then re-run sage\n\n", num_dup_snaps);
        ABORT(EXIT_FAILURE);
    }

    /* because in the default case of 'lhalo-binary', nothing
       gets written to "treeextension", we need to
       null terminate tree-extension first  */
    run_params->TreeExtension[0] = '\0';

    // Check tree type is valid.
    if (strncmp(my_treetype, "lhalo_hdf5", 511)   == 0 ||
        strncmp(my_treetype, "genesis_hdf5", 511) == 0 ||
        strncmp(my_treetype, "gadget4_hdf5", 511) == 0
        ) {
#ifndef HDF5
        fprintf(stderr, "You have specified to use a HDF5 file but have not compiled with the HDF5 option enabled.\n");
        fprintf(stderr, "Please check your file type and compiler options.\n");
        ABORT(EXIT_FAILURE);
#endif
        // strncmp returns 0 if the two strings are equal.
        // only relevant options are HDF5 or binary files. Consistent-trees is *always* ascii (with different filename extensions)
        snprintf(run_params->TreeExtension, 511, ".hdf5");
    }

#define CHECK_VALID_ENUM_IN_PARAM_FILE(paramname, num_enum_types, enum_names, enum_values, string_value) { \
        int found = 0;                                                  \
        for(int i=0;i<num_enum_types;i++) {                             \
            if (strcasecmp(string_value, enum_names[i]) == 0) {         \
                run_params->paramname = enum_values[i];                 \
                found = 1;                                              \
                break;                                                  \
            }                                                           \
        }                                                               \
        if(found == 0) {                                                \
            fprintf(stderr, #paramname " field contains unsupported value of '%s' is not supported\n", string_value); \
            fprintf(stderr," Please choose one of the values -- \n");   \
            for(int i=0;i<num_enum_types;i++) {                         \
                fprintf(stderr, #paramname " = '%s'\n", enum_names[i]); \
            }                                                           \
            ABORT(EXIT_FAILURE);                                        \
        }                                                               \
 }

    const char tree_names[][MAXTAGLEN] = {"lhalo_hdf5", "lhalo_binary", "genesis_hdf5",
                                          "consistent_trees_ascii", "consistent_trees_hdf5",
                                          "gadget4_hdf5"};
    const enum Valid_TreeTypes tree_enums[] = {lhalo_hdf5, lhalo_binary, genesis_hdf5,
                                               consistent_trees_ascii, consistent_trees_hdf5,
                                               gadget4_hdf5};
    const int nvalid_tree_types  = sizeof(tree_names)/(MAXTAGLEN*sizeof(char));
    BUILD_BUG_OR_ZERO((nvalid_tree_types == (int) num_tree_types), number_of_tree_types_is_incorrect);
    CHECK_VALID_ENUM_IN_PARAM_FILE(TreeType, nvalid_tree_types, tree_names, tree_enums, my_treetype);

    /* Check output data type is valid. */
#ifndef HDF5
    if(strncmp(my_outputformat, "sage_hdf5", MAX_STRING_LEN-1) == 0) {
        fprintf(stderr, "You have specified to use HDF5 output format but have not compiled with the HDF5 option enabled.\n");
        fprintf(stderr, "Please check your file type and compiler options.\n");
        ABORT(EXIT_FAILURE);
    }
#endif

    const char format_names[][MAXTAGLEN] = {"sage_binary", "sage_hdf5", "lhalo_binary_output"};
    const enum Valid_OutputFormats format_enums[] = {sage_binary, sage_hdf5, lhalo_binary_output};
    const int nvalid_format_types  = sizeof(format_names)/(MAXTAGLEN*sizeof(char));
    XRETURN(nvalid_format_types == 3, EXIT_FAILURE, "nvalid_format_types = %d should have been 3\n", nvalid_format_types);
    CHECK_VALID_ENUM_IN_PARAM_FILE(OutputFormat, nvalid_format_types, format_names, format_enums, my_outputformat);

    /* Check that the way forests are distributed over (MPI) tasks is valid */
    const char scheme_names[][MAXTAGLEN] = {"uniform_in_forests", "linear_in_nhalos", "quadratic_in_nhalos", "exponent_in_nhalos", "generic_power_in_nhalos"};
    const enum Valid_Forest_Distribution_Schemes scheme_enums[] = {uniform_in_forests, linear_in_nhalos,
                                                                   quadratic_in_nhalos, exponent_in_nhalos, generic_power_in_nhalos};
    const int nvalid_scheme_types  = sizeof(scheme_names)/(MAXTAGLEN*sizeof(char));
    XRETURN(nvalid_scheme_types == num_forest_weight_types, EXIT_FAILURE, "nvalid_format_types = %d should have been %d\n",
            nvalid_format_types, num_forest_weight_types);

    CHECK_VALID_ENUM_IN_PARAM_FILE(ForestDistributionScheme, nvalid_scheme_types, scheme_names, scheme_enums, my_forest_dist_scheme);
#undef CHECK_VALID_ENUM_IN_PARAM_FILE


    /* Check that exponent supplied is non-negative (for cases where the exponent will be used) */
    if((run_params->ForestDistributionScheme == exponent_in_nhalos || run_params->ForestDistributionScheme == generic_power_in_nhalos)
       && run_params->Exponent_Forest_Dist_Scheme < 0) {
        fprintf(stderr,"Error: You have requested a power-law exponent but the exponent = %e must be greater than 0\n",
                run_params->Exponent_Forest_Dist_Scheme);
        fprintf(stderr,"Please change the value for the parameter 'ExponentForestDistributionScheme' in the parameter file (%s)\n", fname);
        ABORT(EXIT_FAILURE);
    }

    myfree(used_tag);
    return EXIT_SUCCESS;
}


#undef MAXTAGS
#undef MAXTAGLEN
