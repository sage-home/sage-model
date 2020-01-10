#ifndef ALLVARS_H
#define ALLVARS_H

/* define off_t as a 64-bit long integer */
#define _FILE_OFFSET_BITS 64

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>

#ifdef HDF5
#include <hdf5.h>
#endif

#include "macros.h"
#include "core_simulation.h"

/* This structure contains the properties used within the code */
struct GALAXY
{
  int   SnapNum;
  int   Type;

  int   GalaxyNr;
  int   CentralGal;
  int   HaloNr;
  long long MostBoundID;
  int64_t GalaxyIndex; // This is a unique value based on the tree local galaxy number,
                       // file local tree number and the file number itself.
                       // See ``generate_galaxy_index()`` in ``core_save.c``.
  int64_t CentralGalaxyIndex; // Same as above, except the ``GalaxyIndex`` value for the CentralGalaxy
                              // of this galaxy's FoF group.

  int   mergeType;  /* 0=none; 1=minor merger; 2=major merger; 3=disk instability; 4=disrupt to ICS */
  int   mergeIntoID;
  int   mergeIntoSnapNum;
  float dT;

  /* (sub)halo properties */
  float Pos[3];
  float Vel[3];
  int   Len;
  float Mvir;
  float deltaMvir;
  float CentralMvir;
  float Rvir;
  float Vvir;
  float Vmax;

  /* baryonic reservoirs */
  float ColdGas;
  float StellarMass;
  float BulgeMass;
  float HotGas;
  float EjectedMass;
  float BlackHoleMass;
  float ICS;

  /* metals */
  float MetalsColdGas;
  float MetalsStellarMass;
  float MetalsBulgeMass;
  float MetalsHotGas;
  float MetalsEjectedMass;
  float MetalsICS;

  /* to calculate magnitudes */
  float SfrDisk[STEPS];
  float SfrBulge[STEPS];
  float SfrDiskColdGas[STEPS];
  float SfrDiskColdGasMetals[STEPS];
  float SfrBulgeColdGas[STEPS];
  float SfrBulgeColdGasMetals[STEPS];

  /* misc */
  float DiskScaleRadius;
  float MergTime;
  double Cooling;
  double Heating;
  float r_heat;
  float QuasarModeBHaccretionMass;
  float TimeOfLastMajorMerger;
  float TimeOfLastMinorMerger;
  float OutflowRate;
  float TotalSatelliteBaryons;

  /* infall properties */
  float infallMvir;
  float infallVvir;
  float infallVmax;
};



/* auxiliary halo data */
struct halo_aux_data
{
    int DoneFlag;
    int HaloFlag;
    int NGalaxies;
    int FirstGalaxy;
#ifdef PROCESS_LHVT_STYLE
    int orig_index;
#endif
    int output_snap_n;
};

enum Valid_TreeTypes
{
  lhalo_binary = 0,
  lhalo_hdf5 = 1,
  genesis_hdf5 = 2,
  consistent_trees_ascii = 3,
  ahf_trees_ascii = 4,
  num_tree_types
};

enum Valid_OutputFormats
{
  sage_binary = 0,
  sage_hdf5 = 1,
  num_format_types
};

/* do not use '0' as an enum since that '0' usually
   indicates 'success' on POSIX systems */
enum sage_error_types {
    /* start off with a large number */
    FILE_NOT_FOUND=1 << 12,
    SNAPSHOT_OUT_OF_RANGE,
    INVALID_OPTION_IN_PARAMS,
    OUT_OF_MEMBLOCKS,
    MALLOC_FAILURE,
    INVALID_PTR_REALLOC_REQ,
    INTEGER_32BIT_TOO_SMALL,
    NULL_POINTER_FOUND,
    FILE_READ_ERROR,
    FILE_WRITE_ERROR,
    INVALID_FILE_POINTER,
    INVALID_FILE_DESCRIPTOR,
    INVALID_VALUE_READ_FROM_FILE,
    PARSE_ERROR,
    INVALID_MEMORY_ACCESS_REQUESTED,
    HDF5_ERROR,
};

struct lhalotree_info {
    int64_t nforests;/* number of forests to process */

    /* lhalotree format only has int32_t for nhalos per forest */
    int32_t *nhalos_per_forest;/* number of halos to read, nforests elements */

    union {
        int *fd;/* the file descriptor for each forest (i.e., which file descriptor to read this forest from) nforests elements*/
#ifdef HDF5
        hid_t *h5_fd;/* contains the HDF5 file descriptor for each forest */
#endif
    };
    off_t *bytes_offset_for_forest;/* where to start reading the files, nforests elements */

    union {
        int *open_fds;/* contains numfiles elements of open file descriptors, numfiles elements */
#ifdef HDF5
        hid_t *open_h5_fds;/* contains numfiles elements of open HDF5 file descriptors */
#endif
    };
    int32_t numfiles;/* number of unique files being processed by this task,  must be >=1 and <= lastfile - firstfile + 1 */
    int32_t unused;/* unused, but present for alignment */
    int32_t *FileNr; // The file number that each forest was read from.
};

struct ctrees_info {
    //different from totnforests; only stores forests to be processed by ThisTask when in MPI mode
    //in serial mode, ``forests_info->ctr.nforests == forests_info->totnforests``)
    union {
        int64_t nforests;
        int64_t nforests_this_task;
    };
    int64_t ntrees;

    void *column_info;/* stored as a void * to avoid including parse_ctrees.h here*/

    /* forest level quantities */
    int64_t *ntrees_per_forest;/* contains nforests elements */
    int64_t *start_treenum_per_forest;/* contains nforests elements */

    /* tree level quantities */
    int *tree_fd;/* contains ntrees elements */
    off_t *tree_offsets;/* contains ntrees elements */


    /* file level quantities */
    int *open_fds;/* contains numfiles elements of open file descriptors */
    int32_t numfiles;/* total number of files the forests are spread over (BOX_DIVISIONS^3 per Consistent trees terminology) */
    int32_t unused;/* unused, but present for alignment */
};

/* place-holder for future AHF i/o capabilities */
struct ahf_info {
    int64_t nforests;
    void *some_yet_to_be_implemented_ptr;
};

#ifdef HDF5
struct genesis_info {
    union{
        int64_t nforests;/* number of forests to process on this task */
        int64_t nforests_this_task;/* shadowed for convenience */
    };

    int64_t start_forestnum;/* Global forestnumber to start processing from */
    int64_t maxforestsize; /* max. number of halos in any one single forest on any task */

    union {
        int32_t *FileNr;/* Integer identifying which file out of the '(lastfile + 1)' for each forest -- shape (nforests, )
                           The unusual capitalisation is to show that the semantics are the same as the
                           variable with 'struct halo_data' (in core_simulation.h) - MS 19/11/2019
                        */
        int32_t *filenum_for_forest;/* shadowed to show what the variable contains */
    };
    int64_t *forestnum_in_file;/* Integer identifying the file-local forest numbers to be read -- shape (nforests, ) */
    int64_t *offset_for_global_forestnum;/* What would be the offset to add to file-local 'forestnum' to get the global forest num
                                            that is needed to access the metadata ("*foreststats*.hdf5") file  -- shape (lastfile + 1, ) */

    int64_t *halo_offset_per_snap;/* Stores the current halo offsets to read from at each snapshot -- shape (maxsnaps, ).
                                     Initialised to all 0's for every new file and incremented as forests are read in. This details
                                     adds a loop-dependency - where later forests can not be correctly processed before all
                                     preceeding forests have been processed. It had to be implemented this way because otherwise
                                     the amount of RAM required to store the matrix offsets_per_forest_per_snap (with shape
                                     '[nforests, maxsnaps]' would have been a roadblock in the future. */
    hid_t meta_fd;/* file descriptor for the metadata file*/
    hid_t *h5_fds;/* contains all the file descriptors for the individual files -- shape (lastfile + 1, ) */

    int32_t min_snapnum; /* smallest snapshot to process (inclusive, >= 0)*/
    int32_t maxsnaps;/* maxsnaps == max_snap_num + 1 */
    int32_t totnfiles;/* total number of files requested to be processed (across all tasks)*/
    int32_t numfiles;/* total number of files to process on ThisTask (>=1)*/
    int32_t start_filenum;/* Which is the first file that this task is going to process  */
    int32_t curr_filenum; /* What file is currently being worked on --
                              required to reset the halo_offset_per_snap at the beginning of every new file */

};
#endif

struct forest_info {
    union {
        struct lhalotree_info lht;
        struct ctrees_info ctr;
        struct ahf_info ahf;
#ifdef HDF5
        struct genesis_info gen;
#endif
    };
    int64_t totnforests;  // Total number of forests across **all** input tree files.
    int64_t nforests_this_task; // Total number of forests processed by **this** task.
    float frac_volume_processed; // Fraction of the simulation volume processed by **this** task.
    // We assume that each of the input tree files span the same volume. Hence by summing the
    // number of trees processed by each task from each file, we can determine the
    // fraction of the simulation volume that this task processes.  We weight this summation by the
    // number of trees in each file because some files may have more/less trees whilst still spanning the
    // same volume (e.g., a void would contain few trees whilst a dense knot would contain many).
};

struct save_info {
    union {
        int *save_fd; // Contains the open file to write to for each output.
#ifdef HDF5
        hid_t file_id;  // HDF5 only writes to a single file per processor.
#endif
    };

    int32_t *tot_ngals;
    int32_t **forest_ngals;

#ifdef HDF5
    char **name_output_fields;
    hsize_t *field_dtypes;

    hid_t *group_ids;

    int32_t num_output_fields;
    hid_t **dataset_ids;

    int32_t buffer_size;
    int32_t *num_gals_in_buffer;
    struct HDF5_GALAXY_OUTPUT *buffer_output_gals;
#endif

};


#define DOUBLE 1
#define STRING 2
#define INT 3

struct params
{
    int    FirstFile;    /* first and last file for processing; only relevant for lhalotree style files (binary or hdf5) */
    int    LastFile;

    char   OutputDir[MAX_STRING_LEN];
    char   FileNameGalaxies[MAX_STRING_LEN];
    char   TreeName[MAX_STRING_LEN];
    char   TreeExtension[MAX_STRING_LEN]; // If the trees are in HDF5, they will have a .hdf5 extension. Otherwise they have no extension.
    char   SimulationDir[MAX_STRING_LEN];
    char   FileWithSnapList[MAX_STRING_LEN];

    double Omega;
    double OmegaLambda;
    double PartMass;
    double Hubble_h;
    double BoxSize;
    double EnergySNcode;
    double EnergySN;
    double EtaSNcode;
    double EtaSN;

    /* moving for alignment */
    int32_t NumSimulationTreeFiles;

    /* recipe flags */
    int    SFprescription;
    int    AGNrecipeOn;
    int    SupernovaRecipeOn;
    int    ReionizationOn;
    int    DiskInstabilityOn;

    double RecycleFraction;
    double Yield;
    double FracZleaveDisk;
    double ReIncorporationFactor;
    double ThreshMajorMerger;
    double BaryonFrac;
    double SfrEfficiency;
    double FeedbackReheatingEpsilon;
    double FeedbackEjectionEfficiency;
    double RadioModeEfficiency;
    double QuasarModeEfficiency;
    double BlackHoleGrowthRate;
    double Reionization_z0;
    double Reionization_zr;
    double ThresholdSatDisruption;

    double UnitLength_in_cm;
    double UnitVelocity_in_cm_per_s;
    double UnitMass_in_g;
    double UnitTime_in_s;
    double RhoCrit;
    double UnitPressure_in_cgs;
    double UnitDensity_in_cgs;
    double UnitCoolingRate_in_cgs;
    double UnitEnergy_in_cgs;
    double UnitTime_in_Megayears;
    double G;
    double Hubble;
    double a0;
    double ar;

    int nsnapshots;
    int LastSnapShotNr;
    int MAXSNAPS;
    int NOUT;
    int Snaplistlen;
    enum Valid_TreeTypes TreeType;
    enum Valid_OutputFormats OutputFormat;
    int64_t FileNr_Mulfac;
    int64_t ForestNr_Mulfac;

    int ListOutputSnaps[ABSOLUTEMAXSNAPS];

    //Essentially creating an alias so that the indecipherable 'ZZ'
    //can be interpreted to contain 'redshift' values
    union {
        double ZZ[ABSOLUTEMAXSNAPS];
        double redshift[ABSOLUTEMAXSNAPS];
    };

    //Similarly: 'AA' contains the scale_factors corresponding to
    //each snapshot
    union {
        double AA[ABSOLUTEMAXSNAPS];
        double scale_factors[ABSOLUTEMAXSNAPS];
    };
    double *Age;

    int interrupted;/* to re-print the progress-bar */

    int32_t ThisTask;
    int32_t NTasks;
};


#endif  /* #ifndef ALLVARS_H */
