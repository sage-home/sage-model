#! /usr/bin/env python


def build_sage_pyext(use_from_mcmc=False, verbose=False):
    import os
    import subprocess
    from cffi import FFI

    # Find the absolute path to the directory name
    dir_path = os.path.dirname(os.path.realpath(__file__))
    if use_from_mcmc:
        # Normally this would be `CFLAGS` but I have used
        ## `CCFLAGS` within the SAGE Makefile - MS: 17th Oct, 2023
        os.environ["CC"] = "mpicc"
        os.environ["CCFLAGS"] = "-DMPI"
        os.environ["CCFLAGS"] = "-DUSE_SAGE_IN_MCMC_MODE"
        os.environ["OPTS"] = "-NDVERBOSE"


    # You can add/remove the capture_output=True argument to suppress/display
    # the output of the subprocess (i.e., from the compilation) - MS 17th Oct, 2023
    try:
        print("running make lib in sage directory")
        subprocess.run(["make", "lib"], shell=True, cwd=dir_path, check=True)
    except subprocess.CalledProcessError as e:
        msg = "Error: Could not build the SAGE shared library"
        print(f"{msg}")
        print(f"Contents of STDOUT: {e.stdout}")
        print(f"Contents of STDERR: {e.stderr}")
        print(f"Return code: {e.returncode}")
        raise RuntimeError(msg)

    ffibuilder = FFI()
    # cdef() expects a single string declaring the C types, functions and
    # globals needed to use the shared object. It must be in valid C syntax.
    ffibuilder.cdef("""

    /* API for sage */
    int run_sage(const int ThisTask, const int NTasks,
    const char *param_file, void **run_params);
    int finalize_sage(void *run_params);

    """)

    # set_source() gives the name of the python extension module to
    # produce, and some C source code as a string.  This C code needs
    # to make the declarated functions, types and globals available,
    # so it is often just the "#include".
    ffibuilder.set_source("_sage_cffi",
                          """
                          #include "src/sage.h"   // the C header for the API
                          """,
                          libraries=['sage'],   # library name, for the linker
                          library_dirs=[dir_path],
                          include_dirs=[dir_path],
                          extra_link_args=["-Xlinker",
                                           "-rpath",
                                           "-Xlinker",
                                           dir_path],
    )

    ffibuilder.compile(verbose=verbose)
    return


def run_sage(paramfile, use_from_mcmc=False):
    try:
        from _sage_cffi import ffi, lib
    except ImportError:
        build_sage_pyext(use_from_mcmc=use_from_mcmc)
        from _sage_cffi import ffi, lib

    rank = 0
    ntasks = 1
    try:
        from mpi4py import MPI
        comm = MPI.COMM_WORLD
        rank = comm.Get_rank()
        ntasks = comm.Get_size()
    except ImportError:
        print("Did not detect MPI")
        pass

    print(f"[Rank={rank}]: Running on {ntasks} tasks")
    # this will contain the (malloc'ed) pointer to 'struct params' in C
    # The reason for this seemingly weird convention (i.e.,
    # why not just keep the params entirely within C) is keep
    # persistent state between run_sage and finalize_sage.
    # finalize_sage needs to know the output file format, which is
    # stored within params. finalize_sage is a separate function
    # so that there is no required MPI call within the SAGE API.
    # This way the necessary MPI_Barrier call is placed within
    # main.c before calling finalize_sage (but after run_sage) - MS: 17th Oct, 2023
    params_struct = ffi.new("void **")
    fname = ffi.new("char []", paramfile.encode())

    lib.run_sage(rank, ntasks, fname, params_struct)
    lib.finalize_sage(params_struct[0])

    return


if __name__ == "__main__":
    import os
    parfile = "tests/test_data/mini-millennium.par"
    if not os.path.isfile(parfile):
        msg = "Error: Could not locate parameter file = {}. "\
              "You should be able to solve that by running "\
              "`/bin/bash first_run.sh` followed by `make tests`"\
              .format(parfile)
        raise AssertionError(msg)

    run_sage(parfile, use_from_mcmc=False)
