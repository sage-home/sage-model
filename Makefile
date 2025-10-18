USE-MPI := yes # set this if you want to run in embarrassingly parallel (automatically set if the compiler (i.e., the CC variable) is set to `mpicc`)
USE-HDF5 := yes # set this if you want to read in hdf5 trees (requires hdf5 libraries)

#MEM-CHECK = yes # Set this if you want to check sanitize pointers/memory addresses. Slowdown of ~2x is expected.
				 # Note: This only works with gcc

USE-BUFFERED-WRITE := yes # Set this to create binary output in chunks (typically has better performance)

MAKE-SHARED-LIB := yes # Define this to any value if you want to create a shared library (otherwise a static library is created)
MAKE-VERBOSE := yes # define this for info messages, otherwise all info messages are disabled (*error* messages are *always* printed)

ROOT_DIR:=$(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))
# In case any of the previous ones do not work and
# ROOT_DIR is not set, then use "." as ROOT_DIR and
# hopefully the cooling tables will still work
ROOT_DIR := $(if $(ROOT_DIR),$(ROOT_DIR),.)


CCFLAGS += -DGNU_SOURCE -std=gnu99 -fPIC
LIBFLAGS :=

OPTS := -DROOT_DIR='"${ROOT_DIR}"'
SRC_PREFIX := src

LIBNAME := sage
LIBSRC :=  sage.c core_read_parameter_file.c core_init.c core_io_tree.c \
           core_cool_func.c core_build_model.c core_save.c core_mymalloc.c core_utils.c progressbar.c \
           core_tree_utils.c model_infall.c model_cooling_heating.c model_starformation_and_feedback.c \
           model_disk_instability.c model_reincorporation.c model_mergers.c model_misc.c \
           io/read_tree_lhalo_binary.c io/read_tree_consistentrees_ascii.c io/ctrees_utils.c \
	       io/save_gals_binary.c io/forest_utils.c io/buffered_io.c

LIBINCL := $(LIBSRC:.c=.h)
LIBINCL += io/parse_ctrees.h

SRC := main.c $(LIBSRC)
SRC  := $(addprefix $(SRC_PREFIX)/, $(SRC))
OBJS := $(SRC:.c=.o)
INCL := core_allvars.h macros.h core_simulation.h $(LIBINCL)
INCL := $(addprefix $(SRC_PREFIX)/, $(INCL))

LIBSRC  := $(addprefix $(SRC_PREFIX)/, $(LIBSRC))
LIBINCL := $(addprefix $(SRC_PREFIX)/, $(LIBINCL))
LIBOBJS := $(LIBSRC:.c=.o)

ifdef MAKE-SHARED-LIB
  SAGELIB := lib$(LIBNAME).so
else
  SAGELIB := lib$(LIBNAME).a
endif

ifdef MAKE-VERBOSE
  OPTS += -DVERBOSE
endif
EXEC := $(LIBNAME)


UNAME := $(shell uname)
ifeq ($(CC), mpicc)
	USE-MPI = yes
endif

ifdef USE-MPI
    OPTS += -DMPI  #  This creates an MPI version that can be used to process files in parallel
    CC := mpicc  # sets the C-compiler
else
    # use clang by default on OSX and gcc on linux
    ifeq ($(UNAME), Darwin)
      CC := clang
    else
      CC := gcc
    endif
endif

# No need to do the path + library checks if
# only attempting to clean the build
DO_CHECKS := 1
CLEAN_CMDS := celan celna clean clena
ifneq ($(filter $(CLEAN_CMDS),$(MAKECMDGOALS)),)
  DO_CHECKS := 0
endif

# Files required for HDF5 -> needs to be defined outside of the
# if condition (for DO_CHECKS); otherwise `make clean` will not
# clean the H5_OBJS
H5_SRC := io/read_tree_lhalo_hdf5.c io/save_gals_hdf5.c io/read_tree_genesis_hdf5.c io/hdf5_read_utils.c \
          io/read_tree_consistentrees_hdf5.c io/read_tree_gadget4_hdf5.c

H5_INCL := $(H5_SRC:.c=.h)
H5_OBJS := $(H5_SRC:.c=.o)

H5_SRC := $(addprefix $(SRC_PREFIX)/, $(H5_SRC))
H5_INCL := $(addprefix $(SRC_PREFIX)/, $(H5_INCL))
H5_OBJS := $(addprefix $(SRC_PREFIX)/, $(H5_OBJS))


## Only set everything if the command is not "make clean" (or related to "make clean")
ifeq ($(DO_CHECKS), 1)
  ## Check if CC is clang under the hood
  CC_VERSION := $(shell $(CC) --version 2>/dev/null)
  ifndef CC_VERSION
    $(info Error: Could find compiler = ${CC})
    $(info Please either set "CC" in "Makefile" or via the command-line "make CC=yourcompiler")
    $(info And please check that the specified compiler is in your "$$PATH" variables)
    $(error )
  endif
  ifeq (clang,$(findstring clang,$(CC_VERSION)))
    CC_IS_CLANG := 1
  else
    CC_IS_CLANG := 0
  endif
  ifeq ($(UNAME), Darwin)
    ifeq ($(CC_IS_CLANG), 0)
      ## gcc on OSX has trouble with
      ## AVX+ instructions. This flag uses
      ## the clang assembler
      CCFLAGS += -Wa,-q
    endif
  endif

  ON_CI := false
  ifeq ($(CI), true)
    ON_CI := true
  endif

  ifeq ($(TRAVIS), true)
    ON_CI := true
  endif

  ifeq ($(ON_CI), true)
  ## If running on CI, fail if any warnings are generated when Making.
    CCFLAGS += -Werror
  endif
  ## end of checking is CC

  # This automatic detection of GSL needs to be before HDF5 checking.
  # This allows HDF5 to be installed in a different directory than Miniconda.
  GSL_FOUND := $(shell gsl-config --version 2>/dev/null)
  ifdef GSL_FOUND
    OPTS += -DGSL_FOUND
    # GSL is probably configured correctly, pick up the locations automatically
    GSL_INCL := $(shell gsl-config --cflags)
    GSL_LIBDIR := $(shell gsl-config --prefix)/lib
    GSL_LIBS   := $(shell gsl-config --libs) -Xlinker -rpath -Xlinker $(GSL_LIBDIR)
  else
    $(warning GSL not found in $$PATH environment variable. Tests will be disabled)
  endif
  CCFLAGS += $(GSL_INCL)
  LIBFLAGS += $(GSL_LIBS)

  ifdef USE-BUFFERED-WRITE
    CCFLAGS += -DUSE_BUFFERED_WRITE
  endif

  ifdef USE-HDF5
    ifndef HDF5_DIR
      ifeq ($(ON_CI), true)
        $(info Looks like we are building on a continuous integration service)
        $(info Assuming that the `hdf5` package and `gsl-config` are both installed by `conda` into the same directory)
        CONDA_BASE := $(shell gsl-config --prefix 2>/dev/null)
        HDF5_DIR := $(CONDA_BASE)
      else
        ## Check if h5tools are installed and use the base directory
        ## Assumes that the return value from 'which' will be
        ## something like /path/to/bin/h5ls; the 'sed' command
        ## replaces the '/bin/h5ls' with '' (i.e., removes '/bin/h5ls')
        ## and returns '/path/to' (without the trailing '/')
        HDF5_DIR := $(strip $(shell which h5ls 2>/dev/null | sed 's/\/bin\/h5ls//'))
        ifndef HDF5_DIR
          $(warning $$HDF5_DIR environment variable is not defined but HDF5 is requested)
          $(warning Could not locate hdf5 tools either)
          $(warning Please install HDF5 (or perhaps load the HDF5 module 'module load hdf5-serial') or disable the 'USE-HDF5' option in the 'Makefile')
          ## Define your HDF5 install directory here
          ## or outside before the USE-HDF5 if condition
          ## to avoid the warning message in the following line
          HDF5_DIR:= ${HOME}/anaconda3
          $(warning Proceeding with a default directory of `[${HDF5_DIR}]` - compilation might fail)
        endif
      endif
    endif

    LIBSRC += $(H5_SRC)
    SRC += $(H5_SRC)

    LIBOBJS += $(H5_OBJS)
    OBJS += $(H5_OBJS)

    LIBINCL += $(H5_INCL)
    INCL += $(H5_INCL)

    HDF5_INCL := -I$(HDF5_DIR)/include
    HDF5_LIB := -L$(HDF5_DIR)/lib -lhdf5 -Xlinker -rpath -Xlinker $(HDF5_DIR)/lib

    OPTS += -DHDF5
    LIBFLAGS += $(HDF5_LIB)
    CCFLAGS += $(HDF5_INCL)
  endif

  GIT_FOUND := $(shell git --version 2>/dev/null)
  ifdef GIT_FOUND
    OPTS += -DGITREF_STR='"$(shell git show-ref --head | head -n 1 | cut -d " " -f 1)"'
  else
    OPTS += -DGITREF_STR='""'
  endif

  ifdef USE-MPI
    MPI_LINK_FLAGS:=$(firstword $(shell $(CC) --showme:link 2>/dev/null))
    MPI_LIB_DIR:=$(firstword $(subst -L, , $(MPI_LINK_FLAGS)))
    LIBFLAGS += -Xlinker -rpath -Xlinker $(MPI_LIB_DIR)
  endif
  # The tests should test with the same compile flags
  # that users are expected to use. Disabled the previous
  # different optimization flags for travis vs regular users
  # This decision was driven by the fact the adding the `-march=native` flag
  # produces test failures on ozstar (https://supercomputing.swin.edu.au/ozstar/)
  # Good news is that even at -O3 the tests pass
  OPTIMIZE := -O2
  ifeq ($(filter tests,$(MAKECMDGOALS)),)
    OPTIMIZE += -march=native
  endif

  # Some flags to allow memory checking (similar to Valgrind).
  # Only enabled when specifically asked.
  ifdef MEM-CHECK
    CCFLAGS +=-fsanitize=undefined -fsanitize=bounds -fsanitize=address -fsanitize-undefined-trap-on-error -fstack-protector-all
    LIBFLAGS += -fsanitize=undefined -fsanitize=bounds -fsanitize=address -fsanitize-undefined-trap-on-error -fstack-protector-all
    # clang will need this flag
    # LIBFLAGS += -shared-libasan
  endif

  # Check if $(AR) and $(CC) belong to the same tool-chain
  AR_BASE_PATH := $(shell which $(AR))
  CC_BASE_PATH := $(shell which $(CC))
  AR_FIRST_DIR := $(firstword $(subst /, ,$(AR_BASE_PATH)))
  CC_FIRST_DIR := $(firstword $(subst /, ,$(CC_BASE_PATH)))

  AR_NAME := $(lastword $(subst /, ,$(AR_BASE_PATH)))
  CC_NAME := $(lastword $(subst /, ,$(CC_BASE_PATH)))
  CC_IN_AR_DIR := $(subst $(AR_NAME),$(CC_NAME),$(AR_BASE_PATH))
  AR_IN_CC_DIR := $(subst $(CC_NAME),$(AR_NAME),$(CC_BASE_PATH))

  # Check that the first directory for both AR and CC are the same
  ifneq ($(AR_FIRST_DIR), $(CC_FIRST_DIR))
    $(warning Looks like the archiver $$AR := '$(AR)' and compiler $$CC := '$(CC)' are from different toolchains)
    $(warning The archiver resolves to '$(AR_BASE_PATH)' while the compiler resolves to '$(CC_BASE_PATH)')
    $(warning If there is an error during linking, then a fix might be to make sure that both the compiler and archiver are from the same distribution)
    $(warning ---- either as '$(CC_IN_AR_DIR)' or '$(AR_IN_CC_DIR)')
  endif

  CCFLAGS += -g -Wextra -Wshadow -Wall -Wno-unused-local-typedefs # and more warning flags
  LIBFLAGS +=   -lm

else
  # something like `make clean` is in effect -> need to also remove the HDF5 objects
  # if HDF5 is requested
  ifdef USE-HDF5
    OBJS += $(H5_OBJS)
  endif

endif # End of DO_CHECKS if condition -> i.e., we do need to care about paths and such

.PHONY: clean celan celna clena tests all

all:  $(SAGELIB) $(EXEC)

$(EXEC): $(OBJS)
	$(CC) $^ $(LIBFLAGS) -o $@

lib libs: $(SAGELIB)

lib$(LIBNAME).a: $(LIBOBJS)
	@echo "Creating static lib"
	$(AR) rcs $@ $(LIBOBJS)

lib$(LIBNAME).so: $(LIBOBJS)
	@echo "Creating shared lib"
	$(CC) -shared $(LIBOBJS) -o $@ $(LIBFLAGS)

pyext: lib$(LIBNAME).so
	@echo "Creating python extension in Makefile"
	python -c "from sage import build_sage_pyext; build_sage_pyext();"

%.o: %.c $(INCL) Makefile
	$(CC) $(OPTS) $(OPTIMIZE) $(CCFLAGS) -c $< -o $@


celan celna clena: clean
clean:
	rm -f $(OBJS) $(EXEC) $(SAGELIB) _$(LIBNAME)_cffi*.so _$(LIBNAME)_cffi.[co]

tests: $(EXEC)
ifdef GSL_FOUND
	./tests/test_sage.sh
else
	$(error GSL is required to run the tests)
endif
