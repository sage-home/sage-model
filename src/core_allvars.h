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


enum Valid_TreeTypes
{
    /* The number of input tree types supported
       This consists of two parts, the first part
       dictates the tree kind (i.e., what the bytes mean), while
       the second part dictates the actual format on disk (i.e.,
       how to read/cast the bytes from disk) */
    lhalo_binary = 0,
    lhalo_hdf5 = 1,
    genesis_hdf5 = 2,
    consistent_trees_ascii = 3,
    num_tree_types
};

enum Valid_OutputFormats
{
    /* The number of output formats supported by sage */
    sage_binary = 0, /* will be deprecated after version 1 release*/
    sage_hdf5 = 1,
    num_output_format_types
};

enum Valid_Forest_Distribution_Schemes
{
    /* Determines the compute cost for each forest as a function
     of the number of halos in the forest*/
    uniform_in_forests = 0, /* returns 1 (i.e., all forests have the same cost regardless of forest size)*/
    linear_in_nhalos = 1, /* returns nhalos (i.e., bigger forests have a bigger compute cost) */
    quadratic_in_nhalos = 2, /* return nhalos^2 as the compute cost*/
    exponent_in_nhalos = 3,/* returns nhalos^exponent */
    generic_power_in_nhalos = 4, /* returns pow(nhalos, exponent) */
    num_forest_weight_types
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


/* This structure contains the properties used within the code */
struct GALAXY
{
    int32_t   SnapNum;
    int32_t  Type;

    int32_t   GalaxyNr;
    int32_t   CentralGal;
    int32_t   HaloNr;
    long long MostBoundID;
    uint64_t GalaxyIndex; // This is a unique value based on the tree local galaxy number,
    // file local tree number and the file number itself.
                       // See ``generate_galaxy_index()`` in ``core_save.c``.
    uint64_t CentralGalaxyIndex; // Same as above, except the ``GalaxyIndex`` value for the CentralGalaxy
    // of this galaxy's FoF group.

    int32_t   mergeType;  /* 0=none; 1=minor merger; 2=major merger; 3=disk instability; 4=disrupt to ICS */
    int32_t   mergeIntoID;
    int32_t   mergeIntoSnapNum;
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
    int32_t DoneFlag;
    int32_t HaloFlag;
    int32_t NGalaxies;
    int FirstGalaxy;
#ifdef PROCESS_LHVT_STYLE
    int orig_index;
#endif
    int output_snap_n;
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

    int32_t min_snapnum; /* smallest snapshot to process (inclusive, >= 0), across all forests*/
    int32_t maxsnaps;/* maxsnaps == max_snap_num + 1, largest snapshot to process across all forests */
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
    double frac_volume_processed; // Fraction of the simulation volume processed by **this** task.
    // We assume that each of the input tree files span the same volume. Hence by summing the
    // number of trees processed by each task from each file, we can determine the
    // fraction of the simulation volume that this task processes.  We weight this summation by the
    // number of trees in each file because some files may have more/less trees whilst still spanning the
    // same volume (e.g., a void would contain few trees whilst a dense knot would contain many).
    int32_t *FileNr; // The file number that each forest was read from.
    int64_t *original_treenr; // The (file-local) tree number from the original tree files.
                              // Necessary because Task N's "Tree 0" could start at the middle of a file.
};

struct save_info {
    union {
        int *save_fd; // Contains the open file to write to for each output.
#ifdef HDF5
        hid_t file_id;  // HDF5 only writes to a single file per processor.
#endif
    };

    int64_t *tot_ngals; // Number of galaxies **per snapshot**.
    int32_t **forest_ngals; // Number of galaxies **per snapshot** **per tree**; forest_ngals[snap][forest].

#ifdef HDF5
    char **name_output_fields;
    hsize_t *field_dtypes;

    hid_t *group_ids;

    int32_t num_output_fields;

    int32_t buffer_size;
    int32_t *num_gals_in_buffer;
    struct HDF5_GALAXY_OUTPUT *buffer_output_gals;
#endif

};


struct params
{
    int32_t    FirstFile;    /* first and last file for processing; only relevant for lhalotree style files (binary or hdf5) */
    int32_t    LastFile;

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
    int32_t    SFprescription;
    int32_t    AGNrecipeOn;
    int32_t    SupernovaRecipeOn;
    int32_t    ReionizationOn;
    int32_t   DiskInstabilityOn;

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

    int32_t nsnapshots;
    int32_t LastSnapshotNr;
    int32_t SimMaxSnaps;
    int32_t NumSnapOutputs;
    int32_t Snaplistlen;
    enum Valid_TreeTypes TreeType;
    enum Valid_OutputFormats OutputFormat;

    /* The combination of  ForestDistributionScheme = generic_power_in_nhalos and
       exponent_for_forest_dist_scheme = 0.7 seems to produce good work-load
       balance across MPI on the 512 Genesis test dataset - MS 16/01/2020 */
    enum Valid_Forest_Distribution_Schemes ForestDistributionScheme;
    double Exponent_Forest_Dist_Scheme;

    int64_t FileNr_Mulfac;
    int64_t ForestNr_Mulfac;

    int32_t ListOutputSnaps[ABSOLUTEMAXSNAPS];
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

    int32_t interrupted;/* to re-print the progress-bar */

    int32_t ThisTask;
    int32_t NTasks;
};


#endif  /* #ifndef ALLVARS_H */
