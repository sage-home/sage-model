#! /usr/bin/env python


def build_sage():
    from cffi import FFI
    ffibuilder = FFI()

    # cdef() expects a single string declaring the C types, functions and
    # globals needed to use the shared object. It must be in valid C syntax.
    ffibuilder.cdef("""

    /* API for sage */
    int init_sage(const int ThisTask, const int NTasks,
    const char *param_file, void **run_params);

    int run_sage(void *run_params);
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
                          library_dirs = ['./'],
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
        pass

    p = ffi.new("void **")
    fname = ffi.new("char []", paramfile.encode())

    lib.init_sage(rank, ntasks, fname, p)
    lib.run_sage(p[0])
    lib.finalize_sage(p[0])

    return

if __name__ == "__main__":
    run_sage("tests/test_data/mini-millennium.par")
