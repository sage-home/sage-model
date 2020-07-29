#! /usr/bin/env python


def build_sage():
    import os
    from cffi import FFI

    # Find the absolute path to the directory name
    dir_path = os.path.dirname(os.path.realpath(__file__))

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
                          #include "src/sage.h"   // the C header of the library
                          """,
                          libraries=['sage'],   # library name, for the linker
                          library_dirs = [dir_path],
                          extra_link_args=["-Xlinker -rpath "\
                                           "-Xlinker " + dir_path],
    )

    ffibuilder.compile(verbose=True)
    return


def run_sage(paramfile):
    try:
        from _sage_cffi import ffi, lib
    except ImportError:
        build_sage()
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
    p = ffi.new("void **")
    fname = ffi.new("char []", paramfile.encode())

    lib.run_sage(rank, ntasks, fname, p)
    lib.finalize_sage(p[0])

    return True


if __name__ == "__main__":
    import os
    parfile = "tests/test_data/mini-millennium.par"
    if not os.path.isfile(parfile):
        msg = "Error: Could not locate parameter file = {}. "\
              "You should be able to solve that by running "\
              "`/bin/bash first_run.sh` followed by `make tests`"\
              .format(parfile)
        raise AssertionError(msg)

    run_sage(parfile)
