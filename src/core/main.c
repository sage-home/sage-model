#include <stdio.h>
#include <stdlib.h>

#ifdef MPI
#include <mpi.h>
#endif

#include "sage.h"
#include "core_logging.h"
#include "core_init.h"
#include "core_config_system.h"


int main(int argc, char **argv)
{
    int ThisTask = 0;
    int NTasks = 1;

#ifdef MPI
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &ThisTask);
    MPI_Comm_size(MPI_COMM_WORLD, &NTasks);
#endif

    if(argc < 2 || argc > 3) {
        fprintf(stderr, "\n  usage: %s <parameterfile> [configfile]\n\n", argv[0]);
#ifdef MPI
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        MPI_Finalize();
#endif
        return EXIT_FAILURE;
    }
    
    // Optional configuration file
    char *config_file = (argc == 3) ? argv[2] : NULL;

    
    /* initialize sage (read parameter file, setup units, read cooling tables etc) */
    void *run_params;
    int status = run_sage(ThisTask, NTasks, argv[1], &run_params);
    
    /* If we have a configuration file, load it after initialization */
    if (config_file != NULL) {
        /* The core_init.c initialize_config_system function will have been called with NULL,
           now we call it directly with the actual config file */
        LOG_INFO("Loading configuration file: %s", config_file);
        printf("Loading configuration file: %s", config_file);
        initialize_config_system(config_file);
        
        /* Apply configuration to params */
        if (global_config != NULL) {
            LOG_INFO("Applying configuration to parameters and modules");
            config_configure_params((struct params *)run_params);
            config_configure_modules((struct params *)run_params);
            config_configure_pipeline();
        } else {
            LOG_ERROR("Failed to load configuration file: %s", config_file);
        }
    } else {
        LOG_INFO("No configuration file specified, using defaults");
    }
    if(status != EXIT_SUCCESS) {
        /* Use fprintf directly here since we might be in an error state before logging is fully initialized */
        fprintf(stderr, "SAGE execution failed with status code %d\n", status);
        goto err;
    }

#ifdef MPI
    // Wait until all tasks are done before we do final tasks/checks.
    MPI_Barrier(MPI_COMM_WORLD);
#endif

    // Perform some final checks.
    status = finalize_sage(run_params);
    if(status != EXIT_SUCCESS) {
        fprintf(stderr, "SAGE finalization failed with status code %d\n", status);
        goto err;
    }

#ifdef MPI
    MPI_Finalize();
#endif
    return EXIT_SUCCESS;

err:
#ifdef MPI
    MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
    MPI_Finalize();
#endif
    fprintf(stderr, "If the fix to this isn't obvious, please feel free to open an issue on our GitHub page.\n"
                    "https://github.com/sage-home/sage-model/issues/new\n");
    return status;

}
