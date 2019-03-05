#include <stdio.h>
#include <stdlib.h>

#ifdef MPI
#include <mpi.h>
#endif

#include "sage.h"


int main(int argc, char **argv)
{
    int ThisTask = 0;
    int NTasks = 1;

#ifdef MPI
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &ThisTask);
    MPI_Comm_size(MPI_COMM_WORLD, &NTasks);
#endif

    if(argc != 2) {
        printf("\n  usage: sage <parameterfile>\n\n");
#ifdef MPI        
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        MPI_Finalize();
#endif
        return EXIT_FAILURE;
    }

    /* initialize sage (read parameter file, setup units, read cooling tables etc) */
    struct params run_params;
    int status = EXIT_FAILURE;
    status = init_sage(ThisTask, argv[1], &run_params);
    if(status != EXIT_SUCCESS) {
        goto err;
    }

    /* run sage over all files */
    status = run_sage(ThisTask, NTasks, &run_params);
    if(status != EXIT_SUCCESS) {
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
    return status;
       
}


