#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#ifdef MPI
#include <mpi.h>
#endif

#include "sage.h"
#include "core_logging.h"
#include "core_init.h"
#include "core_config_system.h"

static void print_usage(const char *program_name) {
    fprintf(stderr, "\nUsage: %s [OPTIONS] <parameterfile> [configfile]\n", program_name);
    fprintf(stderr, "\nOptions:\n");
    fprintf(stderr, "  --log-level LEVEL    Set logging level: quiet, normal, verbose (default: normal)\n");
    fprintf(stderr, "  -h, --help          Show this help message\n");
    fprintf(stderr, "\nArguments:\n");
    fprintf(stderr, "  parameterfile       SAGE parameter file (required)\n");
    fprintf(stderr, "  configfile          Optional JSON configuration file\n");
    fprintf(stderr, "\nLog Levels:\n");
    fprintf(stderr, "  quiet               Show ERROR messages only\n");
    fprintf(stderr, "  normal              Show INFO, WARNING, ERROR messages (default)\n");
    fprintf(stderr, "  verbose             Show DEBUG, INFO, WARNING, ERROR messages\n\n");
}

int main(int argc, char **argv)
{
    int ThisTask = 0;
    int NTasks = 1;
    runtime_log_mode_t log_mode = RUNTIME_LOG_NORMAL; /* Default log level */
    char *param_file = NULL;
    char *config_file = NULL;

#ifdef MPI
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &ThisTask);
    MPI_Comm_size(MPI_COMM_WORLD, &NTasks);
#endif

    /* Parse command line arguments */
    static struct option long_options[] = {
        {"log-level", required_argument, 0, 'l'},
        {"help",      no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };
    
    int c;
    int option_index = 0;
    
    while ((c = getopt_long(argc, argv, "l:h", long_options, &option_index)) != -1) {
        switch (c) {
            case 'l':
                log_mode = logging_parse_level_string(optarg);
                if (log_mode == RUNTIME_LOG_NORMAL && strcasecmp(optarg, "normal") != 0) {
                    fprintf(stderr, "Warning: Invalid log level '%s', using 'normal'\n", optarg);
                }
                break;
            case 'h':
                print_usage(argv[0]);
#ifdef MPI
                MPI_Finalize();
#endif
                return EXIT_SUCCESS;
            case '?':
                /* getopt_long already printed an error message */
                print_usage(argv[0]);
#ifdef MPI
                MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
                MPI_Finalize();
#endif
                return EXIT_FAILURE;
            default:
                fprintf(stderr, "Internal error: unhandled option '%c'\n", c);
#ifdef MPI
                MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
                MPI_Finalize();
#endif
                return EXIT_FAILURE;
        }
    }
    
    /* Check for required parameter file */
    if (optind >= argc) {
        fprintf(stderr, "Error: Missing required parameter file\n");
        print_usage(argv[0]);
#ifdef MPI
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        MPI_Finalize();
#endif
        return EXIT_FAILURE;
    }
    
    param_file = argv[optind];
    
    /* Optional configuration file */
    if (optind + 1 < argc) {
        config_file = argv[optind + 1];
    }
    
    /* Check for too many arguments */
    if (optind + 2 < argc) {
        fprintf(stderr, "Error: Too many arguments\n");
        print_usage(argv[0]);
#ifdef MPI
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        MPI_Finalize();
#endif
        return EXIT_FAILURE;
    }
    
    /* Initialize logging with the specified level */
    logging_set_runtime_mode(log_mode);

    /* initialize sage (read parameter file, setup units, read cooling tables etc) */
    void *run_params;
    int status = run_sage(ThisTask, NTasks, param_file, &run_params);
    
    /* Initialize configuration system with the provided config file (or NULL for defaults) */
    if (config_file != NULL) {
        LOG_INFO("Loading configuration file: %s", config_file);
        printf("Loading configuration file: %s\n", config_file);
    }
    
    initialize_config_system(config_file);
    
    /* Apply configuration to params and modules if config was loaded successfully */
    if (global_config != NULL) {
        if (config_file != NULL) {
            LOG_INFO("Applying configuration to parameters and modules");
            config_configure_params((struct params *)run_params);
            config_configure_modules((struct params *)run_params);
            config_configure_pipeline();
        }
    } else if (config_file != NULL) {
        LOG_ERROR("Failed to load configuration file: %s", config_file);
    }
    if(status != EXIT_SUCCESS) {
        /* Use fprintf directly here since we might be in an error state before logging is fully initialized */
        fprintf(stderr, "SAGE execution failed with status code %d\n", status);
        goto error_exit;
    }

#ifdef MPI
    // Wait until all tasks are done before we do final tasks/checks.
    MPI_Barrier(MPI_COMM_WORLD);
#endif

    // Perform some final checks.
    status = finalize_sage(run_params);
    if(status != EXIT_SUCCESS) {
        fprintf(stderr, "SAGE finalization failed with status code %d\n", status);
        goto error_exit;
    }

#ifdef MPI
    MPI_Finalize();
#endif
    return EXIT_SUCCESS;

error_exit:
#ifdef MPI
    MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
    MPI_Finalize();
#endif
    fprintf(stderr, "If the fix to this isn't obvious, please feel free to open an issue on our GitHub page.\n"
                    "https://github.com/sage-home/sage-model/issues/new\n");
    return status;

}
