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
    consistent_trees_hdf5 = 4,
    gadget4_hdf5 = 5,
    num_tree_types
};

/* Struct for making hdf5 file reading a bit easier */
struct HDF5_METADATA_NAMES
{
    char name_NTrees[MAX_STRING_LEN];
    char name_totNHalos[MAX_STRING_LEN];
    char name_TreeNHalos[MAX_STRING_LEN];
    char name_ParticleMass[MAX_STRING_LEN];
    char name_NumSimulationTreeFiles[MAX_STRING_LEN];
};


enum Valid_OutputFormats
{
    /* The number of output formats supported by sage */
    sage_binary = 0, /* will be deprecated after version 1 release*/
    sage_hdf5 = 1,
    lhalo_binary_output = 2, /* special functionality to convert *any* supported input mergertree into a lhalo-binary format */
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

/* 
 * This structure contains context data for galaxy evolution
 * It encapsulates the state needed during the evolution of galaxies
 * to reduce global state and prepare for modularity.
 */
struct evolution_context {
    /* Halo properties */
    int halo_nr;                /* Current halo number */
    int halo_snapnum;           /* Snapshot number of the halo */
    double redshift;            /* Current redshift (Z) */
    double halo_age;            /* Age of the halo */
    
    /* Galaxy references */
    struct GALAXY *galaxies;    /* Pointer to galaxies array */
    int ngal;                   /* Number of galaxies in the halo */
    int centralgal;             /* Index of central galaxy */
    
    /* Time integration */
    double deltaT;              /* Time step for evolution */
    
    /* Parameters */
    const struct params *params; /* Reference to runtime parameters */
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
    
    /* Extension mechanism - placed at the end for binary compatibility */
    void **extension_data;        /* Array of pointers to module-specific data */
    int num_extensions;           /* Number of registered extensions */
    uint64_t extension_flags;     /* Bitmap to track which extensions are in use */
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

/* Forward declarations */
struct galaxy_extension_registry;
struct io_interface;
struct property_serialization_context;

/**
 * @brief I/O interface handler data
 * 
 * Used for the unified I/O interface approach in galaxy output.
 * Manages the handler and format-specific data.
 */
struct io_handler_data {
    struct io_interface *handler;                         /**< I/O handler for the selected format */
    void *format_data;                                    /**< Format-specific data managed by the handler */
    int using_io_interface;                               /**< Flag to indicate if using the new I/O interface */
    struct property_serialization_context *property_ctx;  /**< Property serialization context */
};


struct lhalotree_info {
    int64_t nforests;/* number of forests to process */

    /* lhalotree format only has int32_t for nhalos per forest */
    int64_t *nhalos_per_forest;/* number of halos to read, nforests elements */

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

struct ctrees_h5_info {
    //different from totnforests; only stores forests to be processed by ThisTask when in MPI mode
    //in serial mode, ``forests_info->ctr.nforests == forests_info->totnforests``)
    union {
        int64_t nforests;
        int64_t nforests_this_task;
    };

    /* file level quantities */
    hid_t meta_fd; /* file descriptor for the metadata file */
    hid_t *h5_file_groups; /* contains all the file descriptors for the individual files -- shape (lastfile + 1, ) */
    hid_t *h5_forests_group; /* contains the file descriptors for the 'Forests' group in the SOA case */
    char snap_field_name[16]; /*  some of the Uchuu files have 'Snap_num', others have 'Snap_idx' as the snapshot field name
                               This variable contains the correct field name, as determined from the input file during forests
                              reading init */
    int8_t snap_field_is_double;/* some of the Uchuu files accidentally wrote out the snapshot field as double instead of int64_t ->
                                this flag is set during the forests reading init to correctly read the snapshot field */
    int8_t *contig_halo_props;/* Contains whether or not the halos are in contiguous order -- shape (lastfile + 1) */
    int32_t totnfiles;/* total number of files that the simulation is spread across*/
    int32_t start_filenum;/* the first file processed on this task*/
    int32_t end_filenum; /* the last file processed on this task (inclusive) */
};

struct gadget4_info {
    int64_t nforests;/* number of forests to process on this task, scalar */

    // int64_t start_forestnum_first_file;/* the forest number (file-local) that is the first forest assigned to this task, scalar*/
    int64_t *nhalos_per_forest; /* number of halos per forest, nforests elements*/

    int32_t numfiles;/* number of unique files being processed by this task,  must be >=1 and <= lastfile - firstfile + 1 */
    hid_t *open_h5_fds;/* contains open HDF5 file descriptors,  contains numfiles elements */

    // Unlike all the other mergertree formats, in the Gadget4 mergertree format, a single forest
    // can be spread over multiple files (potentially >> 1). Therefore, we need to know the range of files
    // that the forest is spread across. The loop over files should go from
    // ``[ start_fd_index_for_forest[iforest], end_fd_index_for_forests[iforest] ]`` (inclusive).
    // Within the loop, the reading for the first file needs to start at ``offset_in_first_file_for_forests[iforest]``
    // The number of halos that should be read in a file is ``min(halos_left_in_file, num_halos_left_to_read_in_forest )``
    //      ii) end file for forest -> read min()

    int32_t *start_h5_fd_index; /* contains the  index into open_h5_fds (starting) HDF5 file descriptor for each forest
                                    filenr-based indexing into open_h5_fds -> i.e., assumes that open_h5_fds can
                                    be indexed by (at least) [start_filenum, end_filenum] , nforests elements */

    int16_t *num_files_per_forest; /* contains the number of files that the forest is split across (used to index the HDF5 file descriptor for each forest), nforests elements */
    int32_t **nhalos_per_file_per_forest; /* irregular 2-D matrix, containing the number of halos within *each* file that this forest is spread over,
                                            dimension is at least [1, nforests], and set to [num_files_for_forests[iforest], nforests]
                                            loading code for `iforest` would look like:

                                                const int32_t numfiles = num_files_for_forests[iforest];
                                                int32_t h5_fd_index = start_h5_fd_index[iforest];
                                                int64_t start_offset = offset_in_first_file_for_forests[iforest];
                                                for(int32_t i=0;i<numfiles;i++) {
                                                    const int64_t numhalos_thisfile = nhalos_per_file_per_forest[i][iforest];
                                                    assert(numhalos_thisfile > 0);
                                                    hid_t hfd = open_h5_fds[h5_fd_index];
                                                    assert(hfd > 0);
                                                    READ_PARTIAL_HALOS_HDF5(hfd, start_offset, numhalos_thisfile);
                                                    h5_fd_index++;
                                                    start_offset = 0;
                                                }
                                            */
    int64_t *offset_in_nhalos_first_file_for_forests; /* offset counted in nhalos contained in all preceeding forests,
                                                        where to start reading the forest in the first files, nforests elements */
    // int64_t *offset_in_forests_first_file_for_forests; /* offset counted as the number of preceeding forests in that first file, nforests elements */
};
#endif

struct forest_info {
    union {
        struct lhalotree_info lht;
        struct ctrees_info ctr;
        struct ahf_info ahf;
#ifdef HDF5
        struct genesis_info gen;
        struct ctrees_h5_info ctr_h5;
        struct gadget4_info gadget4;
#endif
    };

    /* Run-level quantities */
    int64_t totnforests;  // Total number of forests across **all** input tree files.
    int64_t totnhalos; //Total number of halos across **all** input tree files (if it can be calculated ahead of time, otherwise set to 0 e.g., in case of Consistent-Trees ascii)
    double frac_volume_processed; // Fraction of the simulation volume processed by **this** task.
    // We assume that each of the input tree files span the same volume. Hence by summing the
    // number of trees processed by each task from each file, we can determine the
    // fraction of the simulation volume that this task processes.  We weight this summation by the
    // number of trees in each file because some files may have more/less trees whilst still spanning the
    // same volume (e.g., a void would contain few trees whilst a dense knot would contain many).
    int32_t firstfile;//The first file processed in this run (i.e., over all tasks)
    int32_t lastfile;//The last file processed in this run (i.e., over all tasks)

    /* Task level quantities -> unique for each task */
    int64_t nforests_this_task; // Total number of forests processed by **this** task.
    int64_t nhalos_this_task;// Total number of halos to be processed by **this** task (if it can be calculated ahead of time, otherwise set to 0 e.g., in case of Consistent-Trees ascii)

    /* Forest-level quantities (per task) */
    int32_t *FileNr; // The file number that each forest needs to be read from. For formats where an individual tree may be
                     // split across multiple files (e.g., Gadget4), this field contains the starting file number (i.e., where the
                    // first halos within the tree are to be found)
    int64_t *original_treenr; // The (file-local) tree number from the original tree files.
                              // Necessary because Task N's "Tree 0" could start at the middle of a file.
};

/**
 * @brief Save information structure
 * 
 * Holds handles and counters for galaxy output along with I/O interface data.
 */
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

    /* I/O interface support */
    struct io_handler_data io_handler;  /**< Handler for the I/O interface */
};


/*
 * Cosmology parameters
 * 
 * Contains fundamental physical constants and cosmological parameters
 * used throughout the simulation.
 */
struct cosmology_params
{
    /* Base cosmological parameters */
    double Omega;            /* Matter density parameter */
    double OmegaLambda;      /* Dark energy density parameter */
    double Hubble_h;         /* Hubble parameter (H0/100) */
    double PartMass;         /* Particle mass [10^10 Msun/h] */
    double BoxSize;          /* Simulation box size [Mpc/h] */
    
    /* Derived parameters */
    double G;                /* Gravitational constant in code units */
    double Hubble;           /* Hubble parameter in internal units */
    double RhoCrit;          /* Critical density */
};

/*
 * Physics parameters
 * 
 * Contains parameters controlling the various physical processes
 * modeled in the simulation.
 */
struct physics_params
{
    /* Control flags */
    int32_t SFprescription;     /* Star formation prescription to use */
    int32_t AGNrecipeOn;        /* Active Galactic Nuclei feedback flag */
    int32_t SupernovaRecipeOn;  /* Supernova feedback flag */
    int32_t ReionizationOn;     /* Reionization flag */
    int32_t DiskInstabilityOn;  /* Disk instability flag */
    
    /* General physics parameters */
    double BaryonFrac;          /* Cosmic baryon fraction */
    double RecycleFraction;     /* Fraction of stellar mass recycled */
    double Yield;               /* Chemical yield from stars */
    double FracZleaveDisk;      /* Fraction of metals ejected from disk */
    
    /* Mergers */
    double ThreshMajorMerger;   /* Mass ratio threshold for major mergers */
    double ThresholdSatDisruption; /* Threshold for satellite disruption */
    
    /* Gas physics */
    double ReIncorporationFactor; /* Gas reincorporation efficiency */
    
    /* Star formation parameters */
    double SfrEfficiency;       /* Star formation efficiency */
    
    /* Feedback parameters */
    double FeedbackReheatingEpsilon;    /* Efficiency of SN reheating */
    double FeedbackEjectionEfficiency;  /* Efficiency of gas ejection */
    
    /* Black hole parameters */
    double BlackHoleGrowthRate;     /* BH growth rate parameter */
    double RadioModeEfficiency;     /* Radio mode feedback efficiency */
    double QuasarModeEfficiency;    /* Quasar mode feedback efficiency */
    
    /* Reionization parameters */
    double Reionization_z0;     /* Reionization redshift parameter */
    double Reionization_zr;     /* Reionization redshift parameter */
    double a0;                  /* Scale factor for reionization (derived) */
    double ar;                  /* Scale factor for reionization (derived) */
    
    /* Supernova parameters */
    double EnergySN;            /* Energy per supernova [ergs] */
    double EtaSN;               /* Supernova rate */
    double EnergySNcode;        /* Energy per supernova in code units */
    double EtaSNcode;           /* Supernova rate in code units */
};

/*
 * I/O parameters
 * 
 * Contains parameters related to input/output operations.
 */
struct io_params
{
    /* File paths */
    char OutputDir[MAX_STRING_LEN];          /* Output directory path */
    char FileNameGalaxies[MAX_STRING_LEN];   /* Output filename base */
    char TreeName[MAX_STRING_LEN];           /* Merger tree filename base */
    char TreeExtension[MAX_STRING_LEN];      /* Merger tree file extension */
    char SimulationDir[MAX_STRING_LEN];      /* Simulation directory */
    char FileWithSnapList[MAX_STRING_LEN];   /* File containing snapshot list */
    
    /* Tree file parameters */
    int32_t FirstFile;                       /* First tree file to process */
    int32_t LastFile;                        /* Last tree file to process */
    int32_t NumSimulationTreeFiles;          /* Number of tree files */
    
    /* File formats */
    enum Valid_TreeTypes TreeType;           /* Type of input merger trees */
    enum Valid_OutputFormats OutputFormat;   /* Format for galaxy output */
};

/*
 * Units parameters
 * 
 * Contains the basic units of the simulation and derived unit conversions.
 */
struct units_params
{
    /* Base units */
    double UnitLength_in_cm;            /* Unit length in cm */
    double UnitMass_in_g;               /* Unit mass in g */
    double UnitVelocity_in_cm_per_s;    /* Unit velocity in cm/s */
    
    /* Derived units */
    double UnitTime_in_s;               /* Unit time in seconds */
    double UnitTime_in_Megayears;       /* Unit time in Myr */
    double UnitPressure_in_cgs;         /* Unit pressure in cgs */
    double UnitDensity_in_cgs;          /* Unit density in cgs */
    double UnitCoolingRate_in_cgs;      /* Unit cooling rate in cgs */
    double UnitEnergy_in_cgs;           /* Unit energy in cgs */
};

/*
 * Simulation parameters
 * 
 * Contains parameters controlling the simulation time steps and snapshots.
 */
struct simulation_params
{
    /* Snapshot parameters */
    int32_t nsnapshots;         /* Number of snapshots */
    int32_t LastSnapshotNr;     /* Number of the last snapshot */
    int32_t SimMaxSnaps;        /* Maximum number of snapshots */
    int32_t NumSnapOutputs;     /* Number of output snapshots */
    int32_t Snaplistlen;        /* Length of snapshot list */
    
    /* Snapshot arrays */
    int32_t ListOutputSnaps[ABSOLUTEMAXSNAPS]; /* List of output snapshots */
    
    /* Redshift values */
    union {
        double ZZ[ABSOLUTEMAXSNAPS];         /* Redshift at each snapshot */
        double redshift[ABSOLUTEMAXSNAPS];   /* Alias for better readability */
    };
    
    /* Scale factors */
    union {
        double AA[ABSOLUTEMAXSNAPS];         /* Scale factor at each snapshot */
        double scale_factors[ABSOLUTEMAXSNAPS]; /* Alias for better readability */
    };
    
    /* Ages of the Universe */
    double *Age;                /* Age of the universe at each snapshot */
};

/*
 * Runtime parameters
 * 
 * Contains parameters related to program execution and MPI.
 */
struct runtime_params
{
    /* MPI parameters */
    int32_t ThisTask;           /* Current MPI task ID */
    int32_t NTasks;             /* Total number of MPI tasks */
    
    /* Execution control */
    int32_t interrupted;        /* Flag for interrupted execution */
    
    /* Load balancing */
    enum Valid_Forest_Distribution_Schemes ForestDistributionScheme; /* Forest distribution method */
    double Exponent_Forest_Dist_Scheme;   /* Exponent for forest distribution */
    
    /* Tree indexing */
    int64_t FileNr_Mulfac;      /* Multiplier for file number in forest indexing */
    int64_t ForestNr_Mulfac;    /* Multiplier for forest number in indexing */
    
    /* I/O buffering */
    int BufferSizeInitialMB;    /* Initial I/O buffer size in MB */
    int BufferSizeMinMB;        /* Minimum I/O buffer size in MB */
    int BufferSizeMaxMB;        /* Maximum I/O buffer size in MB */
    int EnableMemoryMapping;    /* Enable memory mapping for input files */
    
    /* Module system configuration */
    char ModuleDir[MAX_STRING_LEN];  /* Directory for module search */
    int NumModulePaths;              /* Number of additional module paths */
    char ModulePaths[10][MAX_STRING_LEN]; /* Additional module search paths */
    int EnableModuleDiscovery;       /* Enable automatic module discovery */
};

/*
 * Main parameters structure
 * 
 * This structured groups all parameters into logical categories.
 */
struct params
{
    struct cosmology_params cosmology;    /* Cosmological parameters */
    struct physics_params physics;        /* Physics model parameters */
    struct io_params io;                  /* I/O parameters */
    struct units_params units;            /* Unit conversion parameters */
    struct simulation_params simulation;  /* Simulation control parameters */
    struct runtime_params runtime;        /* Program execution parameters */
};


#endif  /* #ifndef ALLVARS_H */
