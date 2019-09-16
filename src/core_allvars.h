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

#if 0
#ifdef HDF5
extern char          *core_output_file;
extern size_t         HDF5_dst_size;
extern size_t        *HDF5_dst_offsets;
extern size_t        *HDF5_dst_sizes;
extern const char   **HDF5_field_names;
extern hid_t         *HDF5_field_types;
extern int            HDF5_n_props;
#endif
#endif

#define DOUBLE 1
#define STRING 2
#define INT 3

enum Valid_TreeTypes
{
  lhalo_binary = 0,
  illustris_lhalo_hdf5 = 1,
  genesis_standard_hdf5 = 2,
  consistent_trees_ascii = 3,
  ahf_trees_ascii = 5,
  num_tree_types
};

enum Valid_OutputFormats
{
  sage_binary = 0,
  sage_hdf5 = 1,
  num_output_format_types
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
};

struct ctrees_info {
    //different from totnforests; only stores forests to be processed by ThisTask when in MPI mode
    //in serial mode, ``forests_info->ctr.nforests == forests_info->totnforests``)
    int64_t nforests;
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

struct genesis_info {
    int64_t nforests;
    void *some_yet_to_be_implemented_ptr;
};

struct forest_info {
    union {
        struct lhalotree_info lht;
        struct ctrees_info ctr;
        struct ahf_info ahf;
        struct genesis_info gen;
    };
    int64_t totnforests;  // Total number of forests across **all** input tree files.
    int64_t nforests_this_task; // Total number of forests processed by **this** task.
    float frac_volume_processed; // Fraction of the simulation volume processed by **this** task.
    // We assume that each of the input tree files span the same volume. Hence by summing the
    // number of trees processed by each task from each file, we can determine the
    // fraction of the simulation volume that this task processes.  We weight this summation by the
    // number of trees in each file because some files may have more/less trees whilst still spanning the
    // same volume (e.g., a void would contain few trees whilst a dense knot would contain many).
    int32_t *FileNr; // The file number that each forest was read from.
    int32_t *original_treenr; // The tree number from the original tree files.
                              // Necessary because Task N's "Tree 0" could start at the middle of a file.
};

struct save_info {
    union {
        int *save_fd; // Contains the open file to write to for each output.
#ifdef HDF5
        hid_t file_id;  // HDF5 only writes to a single file per processor.
#endif
    };

    int32_t *tot_ngals; // Number of galaxies **per snapshot**.
    int32_t **forest_ngals; // Number of galaxies **per snapshot** **per tree**; fores_ngals[snap][forest].

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
    int32_t NumSimulationTreeFiles; // This will be (e.g.,) 8 for Mini-Millennium and 512 for Millnnium.
    double EnergySNcode;
    double EnergySN;
    double EtaSNcode;
    double EtaSN;

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

    int LastSnapShotNr;
    int MAXSNAPS;
    int NOUT;
    int Snaplistlen;
    enum Valid_TreeTypes TreeType;
    enum Valid_OutputFormats OutputFormat;

    int ListOutputSnaps[ABSOLUTEMAXSNAPS];
    double ZZ[ABSOLUTEMAXSNAPS];
    double AA[ABSOLUTEMAXSNAPS];
    double *Age;

    int interrupted;/* to re-print the progress-bar */

    int32_t ThisTask;
    int32_t NTasks;
};


#endif  /* #ifndef ALLVARS_H */
