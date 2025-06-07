#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h> /* for isblank()*/

#include "core_allvars.h"
#include "core_mymalloc.h"
#include "core_logging.h"
#include "core_parameters.h"

#define MAXTAGLEN 50

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
    char my_treetype[MAX_STRING_LEN], my_forest_dist_scheme[MAX_STRING_LEN];
    
    const int ThisTask = run_params->runtime.ThisTask;

    if(ThisTask == 0) {
        fprintf(stdout, "\nreading parameter file:\n");
    }

    /* Initialize parameter system with defaults */
    if (initialize_parameter_system(run_params) != 0) {
        LOG_ERROR("Failed to initialize parameter system");
        return -1;
    }
    
    /* Open parameter file */
    FILE *fd = fopen(fname, "r");
    if (fd == NULL) {
        LOG_ERROR("Parameter file '%s' not found", fname);
        return FILE_NOT_FOUND;
    }

    /* Read parameter file line by line */
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

        /* Find parameter by name using auto-generated system */
        parameter_id_t param_id = get_parameter_id(buf1);
        
        if (param_id != PARAM_COUNT) {
            /* Found parameter - set it using auto-generated setter */
            if(ThisTask == 0) {
                fprintf(stdout, "%35s\t%10s\n", buf1, buf2);
            }

            if (set_parameter_from_string(run_params, param_id, buf2) != 0) {
                LOG_ERROR("Failed to set parameter '%s' to value '%s'", buf1, buf2);
                errorFlag = 1;
            }
        } else {
            /* Unknown parameter - log warning but don't fail */
            if(ThisTask == 0) {
                LOG_WARNING("Unknown parameter '%s' in file '%s' - skipping", buf1, fname);
            }
        }
    }
    fclose(fd);

    /* Check that all required parameters were set */
    for (parameter_id_t i = 0; i < PARAM_COUNT; i++) {
        if (is_parameter_required(i)) {
            /* For now, we assume all required parameters were set during initialization or file reading */
            /* This could be enhanced to track which parameters were explicitly set */
        }
    }

    /* Handle special string parameters that need post-processing */
    /* These need to be copied from the main parameter structure to local variables for validation */
    strncpy(my_treetype, run_params->io.TreeType == lhalo_hdf5 ? "lhalo_hdf5" :
                        run_params->io.TreeType == lhalo_binary ? "lhalo_binary" :
                        run_params->io.TreeType == genesis_hdf5 ? "genesis_hdf5" :
                        run_params->io.TreeType == consistent_trees_ascii ? "consistent_trees_ascii" :
                        run_params->io.TreeType == consistent_trees_hdf5 ? "consistent_trees_hdf5" :
                        run_params->io.TreeType == gadget4_hdf5 ? "gadget4_hdf5" : "unknown", MAX_STRING_LEN-1);
    my_treetype[MAX_STRING_LEN-1] = '\0';
    
    strncpy(my_forest_dist_scheme, run_params->runtime.ForestDistributionScheme == uniform_in_forests ? "uniform_in_forests" :
                                  run_params->runtime.ForestDistributionScheme == linear_in_nhalos ? "linear_in_nhalos" :
                                  run_params->runtime.ForestDistributionScheme == quadratic_in_nhalos ? "quadratic_in_nhalos" :
                                  run_params->runtime.ForestDistributionScheme == exponent_in_nhalos ? "exponent_in_nhalos" :
                                  run_params->runtime.ForestDistributionScheme == generic_power_in_nhalos ? "generic_power_in_nhalos" : "unknown", MAX_STRING_LEN-1);
    my_forest_dist_scheme[MAX_STRING_LEN-1] = '\0';

    if(errorFlag) {
        ABORT(1);
    }
#ifdef VERBOSE
    fprintf(stdout, "\n");
#endif

    const size_t outlen = strlen(run_params->io.OutputDir);
    if(outlen > 0) {
        if(run_params->io.OutputDir[outlen - 1] != '/')
            strcat(run_params->io.OutputDir, "/");
    }

    if( ! (run_params->simulation.LastSnapshotNr+1 > 0 && run_params->simulation.LastSnapshotNr+1 < ABSOLUTEMAXSNAPS) ) {
        LOG_ERROR("LastSnapshotNr = %d should be in [0, %d)", run_params->simulation.LastSnapshotNr, ABSOLUTEMAXSNAPS);
        ABORT(1);
    }
    run_params->simulation.SimMaxSnaps = run_params->simulation.LastSnapshotNr + 1;

    if(!(run_params->simulation.NumSnapOutputs == -1 || (run_params->simulation.NumSnapOutputs > 0 && run_params->simulation.NumSnapOutputs <= ABSOLUTEMAXSNAPS))) {
        LOG_ERROR("NumOutputs must be -1 or between 1 and %i", ABSOLUTEMAXSNAPS);
        ABORT(1);
    }

    // read in the output snapshot list
    if(run_params->simulation.NumSnapOutputs == -1) {
        run_params->simulation.NumSnapOutputs = run_params->simulation.SimMaxSnaps;
        for (int i=run_params->simulation.NumSnapOutputs-1; i>=0; i--) {
            run_params->simulation.ListOutputSnaps[i] = i;
        }
#ifdef VERBOSE
        if(ThisTask == 0) {
            fprintf(stdout, "all %d snapshots selected for output\n", run_params->simulation.NumSnapOutputs);
        }
#endif
    } else {
#ifdef VERBOSE
        if(ThisTask == 0) {
            fprintf(stdout, "%d snapshots selected for output:", run_params->simulation.NumSnapOutputs);
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
                for(int i=0; i<run_params->simulation.NumSnapOutputs; i++) {
                    if(fscanf(fd, "%d", &(run_params->simulation.ListOutputSnaps[i])) == 1) {
#ifdef VERBOSE
                        if(ThisTask == 0) {
                            fprintf(stdout, " %d", run_params->simulation.ListOutputSnaps[i]);
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
            LOG_ERROR("Could not properly parse output snapshots");
            ABORT(2);
        }
#ifdef VERBOSE
        fprintf(stdout, "\n");
#endif
    }


    if(run_params->io.FirstFile < 0 || run_params->io.LastFile < 0 || run_params->io.LastFile < run_params->io.FirstFile) {
        fprintf(stderr,"Error: FirstFile = %d and LastFile = %d must both be >=0 *AND* LastFile "
                        "should be larger than   FirstFile.\nProbably a typo in the parameter-file. "
                        "Please change to appropriate values...exiting\n",
                        run_params->io.FirstFile, run_params->io.LastFile);
        ABORT(EXIT_FAILURE);
    }

    /* sort the output snapshot numbers in descending order (in case the user didn't do that already) MS: 24th Oct, 2023 */
    qsort(run_params->simulation.ListOutputSnaps, run_params->simulation.NumSnapOutputs, 
          sizeof(run_params->simulation.ListOutputSnaps[0]), compare_ints_descending);

    /* Check for duplicate snapshot outputs */
    int num_dup_snaps = 0;
    for(int ii=1;ii<run_params->simulation.NumSnapOutputs;ii++) {
        const int dsnap = run_params->simulation.ListOutputSnaps[ii-1] - run_params->simulation.ListOutputSnaps[ii];
        if(dsnap == 0) {
            LOG_ERROR("Found duplicate snapshots in the list of desired output snapshots");
            fprintf(stderr,"Duplicate value = %d in position = %d (out of %d total output snapshots requested)\n",
                            run_params->simulation.ListOutputSnaps[ii], ii, run_params->simulation.NumSnapOutputs);
            num_dup_snaps++;
        }
    }
    if(num_dup_snaps != 0) {
        LOG_ERROR("Found %d duplicate snapshots - please remove them from the parameter file and then re-run sage", num_dup_snaps);
        ABORT(EXIT_FAILURE);
    }

    /* because in the default case of 'lhalo-binary', nothing
       gets written to "treeextension", we need to
       null terminate tree-extension first  */
    run_params->io.TreeExtension[0] = '\0';

    // Check tree type is valid.
    if (strncmp(my_treetype, "lhalo_hdf5", 511)   == 0 ||
        strncmp(my_treetype, "genesis_hdf5", 511) == 0 ||
        strncmp(my_treetype, "gadget4_hdf5", 511) == 0
        ) {
#ifndef HDF5
        LOG_ERROR("You have specified to use a HDF5 file but have not compiled with the HDF5 option enabled");
        LOG_ERROR("Please check your file type and compiler options");
        ABORT(EXIT_FAILURE);
#endif
        // strncmp returns 0 if the two strings are equal.
        // only relevant options are HDF5 or binary files. Consistent-trees is *always* ascii (with different filename extensions)
        snprintf(run_params->io.TreeExtension, 511, ".hdf5");
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
    CHECK_VALID_ENUM_IN_PARAM_FILE(io.TreeType, nvalid_tree_types, tree_names, tree_enums, my_treetype);

    /* HDF5 is the only supported output format */
#ifndef HDF5
    LOG_ERROR("SAGE requires HDF5 support. Please compile with the HDF5 option enabled");
    ABORT(EXIT_FAILURE);
#endif


    /* Check that the way forests are distributed over (MPI) tasks is valid */
    const char scheme_names[][MAXTAGLEN] = {"uniform_in_forests", "linear_in_nhalos", "quadratic_in_nhalos", "exponent_in_nhalos", "generic_power_in_nhalos"};
    const enum Valid_Forest_Distribution_Schemes scheme_enums[] = {uniform_in_forests, linear_in_nhalos,
                                                                   quadratic_in_nhalos, exponent_in_nhalos, generic_power_in_nhalos};
    const int nvalid_scheme_types  = sizeof(scheme_names)/(MAXTAGLEN*sizeof(char));
    XRETURN(nvalid_scheme_types == num_forest_weight_types, EXIT_FAILURE, "nvalid_scheme_types = %d should have been %d\n",
            nvalid_scheme_types, num_forest_weight_types);

    CHECK_VALID_ENUM_IN_PARAM_FILE(runtime.ForestDistributionScheme, nvalid_scheme_types, scheme_names, scheme_enums, my_forest_dist_scheme);
#undef CHECK_VALID_ENUM_IN_PARAM_FILE


    /* Check that exponent supplied is non-negative (for cases where the exponent will be used) */
    if((run_params->runtime.ForestDistributionScheme == exponent_in_nhalos || 
        run_params->runtime.ForestDistributionScheme == generic_power_in_nhalos)
       && run_params->runtime.Exponent_Forest_Dist_Scheme < 0) {
        fprintf(stderr,"Error: You have requested a power-law exponent but the exponent = %e must be greater than 0\n",
                run_params->runtime.Exponent_Forest_Dist_Scheme);
        fprintf(stderr,"Please change the value for the parameter 'ExponentForestDistributionScheme' in the parameter file (%s)\n", fname);
        ABORT(EXIT_FAILURE);
    }

    return EXIT_SUCCESS;
}