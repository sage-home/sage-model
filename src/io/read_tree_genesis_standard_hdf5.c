#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "read_tree_genesis_standard_hdf5.h"
#include "../core_mymalloc.h"
#include "../core_utils.h"

#include <hdf5.h>
#include <hdf5_hl.h> /* Accesses the "LITE" interface -- H5LT functions*/

/* Local enum for individual properties */
enum GalaxyPropertyEnums
{
 head_enum = 0,
 tail_enum = 1,
 hosthaloid_enum,
 m200c_enum,
 m200b_enum,
 mtophat_enum,
 r200c_enum,
 vmax_enum,
 xc_enum,
 yc_enum,
 zc_enum,
 vxc_enum,
 vyc_enum,
 vzc_enum,
 len_enum,
 mostboundid_enum,
 lx_enum,
 ly_enum,
 lz_enum,
 veldisp_enum,
 num_galaxy_props /* should be the last one */
};


void get_forests_filename_genesis_hdf5(char *filename, const size_t len, const struct params *run_params)
{
    snprintf(filename, len - 1, "%s/%s.%s", run_params->SimulationDir, run_params->TreeName, run_params->TreeExtension);
}


/* Externally visible Functions */
int setup_forests_io_genesis_hdf5(struct forest_info *forests_info, const int ThisTask, const int NTasks, struct params *run_params)
{
    herr_t status;
    /* struct METADATA_NAMES metadata_names; */
    char filename[4*MAX_STRING_LEN];
    get_forests_filename_genesis_hdf5(filename, 4*MAX_STRING_LEN, run_params);
    forests_info->gen.h5_fd = H5Fopen(filename, H5F_ACC_RDONLY, H5P_DEFAULT);

    if (forests_info->gen.h5_fd < 0) {
        fprintf(stderr,"Error: On ThisTask = %d can't open file `%s'\n", ThisTask, filename);
        return FILE_NOT_FOUND;
    }

    struct genesis_info *gen = &(forests_info->gen);

#define READ_GENESIS_ATTRIBUTE( TYPE, hid, dspace, attrname, dst) {     \
        if(sizeof(#TYPE) != sizeof(*dst)) {                             \
            fprintf(stderr,"Error: the type = %s with size = %zu does not " \
                    "equal the size of the destination memory = %zu\n", #TYPE, sizeof(#TYPE), sizeof(*dst)); \
        }                                                               \
        status = H5LTget_attribute_ ## TYPE (hid, #dspace, #attrname, dst); \
        if(status < 0) {                                                \
            fprintf(stderr,"Error while attempting %s attribute from %s/%s\n", #TYPE, #dspace, #attrname); \
            return -1;                                                  \
        }                                                               \
    }

    READ_GENESIS_ATTRIBUTE( int, gen->h5_fd, "/Header", "NSnaps", &(run_params->nsnapshots));
    READ_GENESIS_ATTRIBUTE( double, gen->h5_fd, "/Header/Particle_mass", "DarkMatter", &(run_params->PartMass));
    READ_GENESIS_ATTRIBUTE( double, gen->h5_fd, "/Header/Simulation", "Omega_m", &(run_params->Omega));
    READ_GENESIS_ATTRIBUTE( double, gen->h5_fd, "/Header/Simulation", "Omega_Lambda", &(run_params->OmegaLambda));
    READ_GENESIS_ATTRIBUTE( double, gen->h5_fd, "/Header/Simulation", "h_val", &(run_params->Hubble_h));
    READ_GENESIS_ATTRIBUTE( double, gen->h5_fd, "/Header/Simulation", "Period", &(run_params->BoxSize));

    double lunit, munit, vunit;

    /* Read in units from the Genesis forests file */
    READ_GENESIS_ATTRIBUTE( double, gen->h5_fd, "/Header/Units", "Length_unit_to_kpc", &lunit);
    READ_GENESIS_ATTRIBUTE( double, gen->h5_fd, "/Header/Units", "Velocity_unit_to_kms", &vunit);
    READ_GENESIS_ATTRIBUTE( double, gen->h5_fd, "/Header/Units", "Mass_unit_to_solarmass", &munit);
#undef READ_GENESIS_ATTRIBUTE

    /* convert the units to the appropriate cgs values */
    lunit *= CM_PER_MPC * 1e-3; /* convert from kpc to cm */
    vunit *= 1e5; /* convert to cm/s */
    munit *= SOLAR_MASS;/* convert from 1e10 Msun to gm */

    /* Check that the units specified in the parameter file are very close to these values -> if not, ABORT
       (We could simply call init_sage again here but that will lead to un-necessary intermingling of components that
       should be independent)
     */

    const double maxdiff = 1e-8, maxreldiff = 1e-5; /*numpy.allclose defaults (as of v1.16) */
#define CHECK_AND_ABORT_UNITS_VS_PARAM_FILE( name, variable, param, absdiff, absreldiff) { \
        if(AlmostEqualRelativeAndAbs_double(variable, param, absdiff, absreldiff) != EXIT_SUCCESS) { \
            fprintf(stderr,"Error: Variable %s has value = %g and is different from what is specified in the parameter file = %g\n", \
                    name, variable, param);                             \
            return -1;                                                  \
        }                                                               \
    }

    CHECK_AND_ABORT_UNITS_VS_PARAM_FILE("Length Unit", lunit, run_params->UnitLength_in_cm, maxdiff, maxreldiff);
    CHECK_AND_ABORT_UNITS_VS_PARAM_FILE("Velocity Unit", vunit, run_params->UnitVelocity_in_cm_per_s, maxdiff, maxreldiff);
    CHECK_AND_ABORT_UNITS_VS_PARAM_FILE("Mass Unit", munit, run_params->UnitMass_in_g, maxdiff, maxreldiff);

#undef CHECK_AND_ABORT_UNITS_VS_PARAM_FILE


    /* Now we know all the datasets -> we can open the corresponding dataset groups (ie, Snap_XXX groups)*/
    gen->maxsnaps = run_params->nsnapshots + 1;

    gen->open_h5_dset_snapgroups = malloc(sizeof(*gen->open_h5_dset_snapgroups) * gen->maxsnaps);
    XRETURN(gen->open_h5_dset_snapgroups != NULL, MALLOC_FAILURE,
            "Error: Could not allocate memory (each element of size = %zu bytes) "
            "for storing the hdf5 dataset groups for %d snapshots\n", sizeof(*gen->open_h5_dset_snapgroups), gen->maxsnaps);


    /* Now we can open the individual snapshot datasets */
    for(int i=0;i<gen->maxsnaps;i++) {
        gen->open_h5_dset_snapgroups[i] = -1;
        char snap_group_name[MAX_STRING_LEN];
        snprintf(snap_group_name, MAX_STRING_LEN-1, "Snap_%03d", i);
        gen->open_h5_dset_snapgroups[i] = H5Gopen(gen->h5_fd, snap_group_name, H5P_DEFAULT);
        XRETURN(gen->open_h5_dset_snapgroups[i] > 0, HDF5_ERROR,
                "Error: Could not open group = `%s` corresponding to snapshot = %d\n",
                snap_group_name, i);
    }

    /* At this point we do know the number of nsnapshots but do not know the number of unique forests */

    /*Count the number of unique forests */
    int64_t num_unique_forests = 0;
    for(int i=0;i<gen->maxsnaps;i++) {
        //H5LTread_dataset_long();//open 'ForestID' key within each snap
        //keep a hash of all unique forestids encountered so far

    }


    /* Now malloc */




    /* And distribute forests across tasks */





    return EXIT_SUCCESS;
}

/*
  Fields in the particle data type, stored at each snapshot
  ['Efrac', 'ForestID', 'ForestLevel',
  'Head', 'HeadRank', 'HeadSnap',
  'ID',
  'Lx', 'Ly', 'Lz',
  'Mass_200crit', 'Mass_200mean', 'Mass_FOF', 'Mass_tot',
  'Num_descen', 'Num_progen',
  'RVmax_Lx', 'RVmax_Ly', 'RVmax_Lz',
  'RVmax_sigV', 'R_200crit', 'R_200mean',
  'R_HalfMass', 'R_size', 'Rmax',
  'RootHead', 'RootHeadSnap', 'RootTail', 'RootTailSnap',
  'Structuretype',
  'Tail', 'TailSnap',
  'VXc', 'VYc', 'VZc', 'Vmax',
  'Xc', 'Yc', 'Zc',
  'cNFW',
  'hostHaloID',
  'lambda_B',
  'npart',
  'numSubStruct',
  'sigV']


----------------------------
  From the ASTRO 3D wiki, here is info about the fields.

  This format as several key fields per snapshot:

  Head: A halo ID pointing the immediate descendant of a halo. With temporally unique ids, this id encodes both the snapshot that the descendant is at and the index in the properties array
  HeadSnap: The snapshot of the immediate descendant
  RootHead: Final descendant
  RootHeadSnap: Final descendant snapshot
  Tail: A halo ID pointing to the immediate progenitor
  TailSnap, RootTail, RootTailSnap: similar in operation to HeadSnap, RootHead, RootHeadSnap but for progenitors
  ID: The halo ID
  Num_progen: number of progenitors

  There are also additional fields that are present for Meraxes,

  ForestID: A unique id that groups all descendants of a field halo and any subhalos it may have contained (which can link halos together if one was initially a subhalo of the other).This is computationally intensive. Allows for quick parsing of all halos to identify those that interact across cosmic time.

  To walk the tree, one needs only to move forward/backward in time one just needs to get Head or Tail and access the data given by that ID.
  The temporally unique ID is given by:

  ID = snapshot*1e12 + halo index

----------------------------

 */


int64_t load_forest_genesis_hdf5(int64_t forestnr, struct halo_data **halos, struct forest_info *forests_info, struct params *run_params)
{
    // The max. size of the data to be read in would be "nhalos" * NDIM (== 3 for pos/vel) * sizeof(double)
    char *buffer = NULL;
    struct genesis_info *gen = &(forests_info->gen);

    if (gen->h5_fd < 0) {
        fprintf(stderr, "The HDF5 file should still be opened when reading the halos in the forest.\n");
        fprintf(stderr, "For forest %"PRId64" we encountered error\n", forestnr);
        H5Eprint(gen->h5_fd, stderr);
        ABORT(NULL_POINTER_FOUND);
    }
    const int64_t nhalos = gen->nhalos_per_forest[forestnr];
    int32_t *forest_local_offsets = calloc(gen->maxsnaps, sizeof(*forest_local_offsets));
    if (forest_local_offsets == NULL) {
        fprintf(stderr, "Could not allocate memory for the storing the forest local offsets that separate each snapshot\n");
        ABORT(MALLOC_FAILURE);
    }

    int64_t offset=0;
    const int start_snap = gen->min_snap_num;
    const int end_snap = gen->min_snap_num + gen->maxsnaps;
    hsize_t *forest_nhalos = gen->nhalos_per_forest_per_snap[forestnr];
    for(int isnap=end_snap;isnap>=start_snap;isnap--) {
        if(offset > INT_MAX) {
            fprintf(stderr,"Error: In function %s> Can not correctly represent %"PRId64" as an offset in the 32-bit variable within "
                    "the LHaloTree struct. \n", __FUNCTION__, offset);
            ABORT(INTEGER_32BIT_TOO_SMALL);
        }

        forest_local_offsets[isnap] = offset;
        offset += forest_nhalos[isnap];
    }


    *halos = mymalloc(sizeof(struct halo_data) * nhalos);//the malloc failure check is done within mymalloc
    struct halo_data *local_halos = *halos;

    buffer = calloc(nhalos * NDIM * sizeof(double), sizeof(*buffer));
    if (buffer == NULL) {
        fprintf(stderr, "Could not allocate memory for the HDF5 multiple dimension buffer.\n");
        ABORT(MALLOC_FAILURE);
    }

#define READ_PARTIAL_1D_ARRAY(fd, dataset_name, dataset_id, filespace, offset, count, h5_dtype, buffer) \
    {                                                                   \
        herr_t macro_status = H5Sselect_hyperslab(filespace, H5S_SELECT_SET, offset, NULL, count, NULL); \
        if(macro_status < 0) {                                          \
            fprintf(stderr,"Error: Failed to select hyperslab for #dataset_name.\n" \
                    "The dimensions of the dataset was %d\nThe file ID was %d\n." \
                    "The dataspace ID was %d.", (int32_t) *count, (int32_t) fd, \
                    (int32_t) dataset_id);                              \
            return -1;                                                  \
        }                                                               \
        hid_t macro_memspace = H5Screate_simple(1, count, NULL);        \
        macro_status = H5Dread(dataset_id, h5_dtype, macro_memspace, filespace, H5P_DEFAULT, buffer); \
        if(macro_status < 0) {                                          \
            fprintf(stderr, "Error: Failed to read array for #dataset_name.\n" \
                    "The dimensions of the dataset was %d\nThe file ID was %d\n." \
                    "The dataspace ID was %d.", (int32_t) *count, (int32_t) fd, \
                    (int32_t) dataset_id);                              \
            return -1;                                                  \
        }                                                               \
        H5Sclose(macro_memspace);                                       \
    }

#define ASSIGN_BUFFER_TO_SAGE(nhalos, buffer_dtype, sage_name, sage_dtype) { \
        for(hsize_t i=0;i<nhalos[0];i++) {                                 \
            local_halos[i].sage_name = (sage_dtype) ((buffer_dtype *)buffer)[i]; \
        }                                                               \
    }

#define ASSIGN_BUFFER_TO_NDIM_SAGE(nhalos, ndim, buffer_dtype, sage_name, sage_dtype) { \
        for(hsize_t i=0;i<nhalos[0];i++) {                              \
            for(int idim=0;idim<ndim;idim++) {                          \
                buffer_dtype *new_buf = ((buffer_dtype*)buffer) + idim*nhalos[0]; \
                local_halos[i].sage_name[idim] = (sage_dtype) new_buf[i]; \
            }                                                           \
        }                                                               \
    }

#define ASSIGN_BUFFER_WITH_MERGERTREE_TO_SAGE(nhalos, buffer_dtype, sage_name, snapnum, is_mergertree_index) { \
        for(hsize_t i=0;i<nhalos[0];i++) {                              \
            const int64_t macro_haloid = ((int64_t *) buffer)[i];       \
            const int64_t macro_snapshot = CONVERT_HALOID_TO_SNAPSHOT(macro_haloid); \
            const int64_t macro_haloindex = CONVERT_HALOID_TO_INDEX(macro_haloid); \
            /*if the halo is pointing to itself and the index is a mergertree index then */ \
            /* then follow the sage convention of setting as '-1'. Note that 'FirstHaloinFOFGroup' */ \
            /* would point to itself correctly (is_mergertree_index should be 'False' for */ \
            /* 'FirstHaloinFOFGroup') */                                \
            if(is_mergertree_index && macro_snapshot == snapnum && (hsize_t) macro_haloindex == i) { \
                local_halos[i].sage_name = -1;                          \
            }                                                           \
            const int64_t macro_forest_local_index = forest_local_offsets[macro_snapshot] + macro_haloindex; \
            if(macro_forest_local_index > INT_MAX) {                    \
                fprintf(stderr,"Error: In function %s> Can not correctly represent %"PRId64" as an offset in the 32-bit variable within " \
                        "the LHaloTree struct. \n", __FUNCTION__, offset); \
                ABORT(INTEGER_32BIT_TOO_SMALL);                         \
            }                                                           \
            local_halos[i].sage_name = (int32_t) macro_forest_local_index; \
        }                                                               \
    }

    hsize_t *forest_offsets = gen->offset_for_forest_per_snap[forestnr];
    hid_t fd = gen->h5_fd;
    for(int isnap=end_snap;isnap>=start_snap;isnap--) {
        hsize_t offset_this_snap[1], nhalos_this_snap[1];
        offset_this_snap[0] = forest_offsets[isnap];
        nhalos_this_snap[0] = forest_nhalos[isnap];
        hid_t *open_h5_dset_props = gen->open_h5_dset_props[isnap];
        hid_t *open_h5_props_filespace = gen->open_h5_props_filespace[isnap];
        if(nhalos_this_snap[0] == 0) continue;


        /* Merger Tree Pointers */
        //Descendant, FirstProgenitor, NextProgenitor, FirstHaloInFOFgroup, NextHaloInFOFgroup
        READ_PARTIAL_1D_ARRAY(fd, "Head", open_h5_dset_props[head_enum], open_h5_props_filespace[head_enum],
                              offset_this_snap, nhalos_this_snap, H5T_NATIVE_LONG, buffer);

        /* Can not directly assign since 'Head' contains Descendant haloid which is too large to be contained
           within 32 bits. I will need a separate assignment to break up haloid into a local index + snapshot,
           and then use the forest-local offset for each snapshot */
        ASSIGN_BUFFER_WITH_MERGERTREE_TO_SAGE(nhalos_this_snap, int64_t, Descendant, isnap, 1);//the last parameter is '1' for mergertree indices and 0 otherwise

        //Same with 'Tail' -> 'FirstProgenitor'
        READ_PARTIAL_1D_ARRAY(fd, "Tail", open_h5_dset_props[tail_enum], open_h5_props_filespace[tail_enum],
                              offset_this_snap, nhalos_this_snap, H5T_NATIVE_LONG, buffer);
        ASSIGN_BUFFER_WITH_MERGERTREE_TO_SAGE(nhalos_this_snap, int64_t, FirstProgenitor, isnap, 1);//the last parameter is '1' for mergertree indices and 0 otherwise

        //And same with 'hostHaloID' -> FirstHaloinFOFGroup.
        READ_PARTIAL_1D_ARRAY(fd, "hostHaloID", open_h5_dset_props[hosthaloid_enum], open_h5_props_filespace[hosthaloid_enum],
                              offset_this_snap, nhalos_this_snap, H5T_NATIVE_LONG, buffer);
        ASSIGN_BUFFER_WITH_MERGERTREE_TO_SAGE(nhalos_this_snap, int64_t, FirstHaloInFOFgroup, isnap, 0);//the last parameter is '1' for mergertree indices and 0 otherwise

        /* MS 3rd June, 2019: The LHaloTree convention (which sage uses) is that Mvir contains M200c. While this is DEEPLY confusing,
           I am using C 'unions' to reduce the confusion slightly. What will happen here is that 'Mass_200crit' will get
           assigned to the 'M200c' field within the halo struct; but that 'M200c' will also be accessible via 'Mvir'.
        */
        READ_PARTIAL_1D_ARRAY(fd, "Mass_200crit", open_h5_dset_props[m200c_enum], open_h5_props_filespace[m200c_enum],
                              offset_this_snap, nhalos_this_snap, H5T_NATIVE_DOUBLE, buffer);
        ASSIGN_BUFFER_TO_SAGE(nhalos_this_snap, double, M200c, float);//M200c is an alias for Mvir
        READ_PARTIAL_1D_ARRAY(fd, "Vmax", open_h5_dset_props[vmax_enum], open_h5_props_filespace[vmax_enum],
                              offset_this_snap, nhalos_this_snap, H5T_NATIVE_DOUBLE, buffer);
        ASSIGN_BUFFER_TO_SAGE(nhalos_this_snap, double, Vmax, float);

        /* Read in the positions for the halo centre*/
        READ_PARTIAL_1D_ARRAY(fd, "Xc", open_h5_dset_props[xc_enum], open_h5_props_filespace[xc_enum],
                              offset_this_snap, nhalos_this_snap, H5T_NATIVE_DOUBLE, buffer);
        READ_PARTIAL_1D_ARRAY(fd, "Yc", open_h5_dset_props[yc_enum], open_h5_props_filespace[yc_enum],
                              offset_this_snap, nhalos_this_snap, H5T_NATIVE_DOUBLE, buffer + nhalos_this_snap[0]*sizeof(double));
        READ_PARTIAL_1D_ARRAY(fd, "Zc", open_h5_dset_props[zc_enum], open_h5_props_filespace[zc_enum],
                              offset_this_snap, nhalos_this_snap, H5T_NATIVE_DOUBLE, buffer + 2*nhalos_this_snap[0]*sizeof(double));
        /* Assign to the Pos array within sage*/
        ASSIGN_BUFFER_TO_NDIM_SAGE(nhalos_this_snap, NDIM, double, Pos, float);

        /* Read in the halo velocities */
        READ_PARTIAL_1D_ARRAY(fd, "VXc", open_h5_dset_props[vxc_enum], open_h5_props_filespace[vxc_enum],
                              offset_this_snap, nhalos_this_snap, H5T_NATIVE_DOUBLE, buffer);
        READ_PARTIAL_1D_ARRAY(fd, "VYc", open_h5_dset_props[vyc_enum], open_h5_props_filespace[vyc_enum],
                              offset_this_snap, nhalos_this_snap, H5T_NATIVE_DOUBLE, buffer + nhalos_this_snap[0]*sizeof(double));
        READ_PARTIAL_1D_ARRAY(fd, "VZc", open_h5_dset_props[vzc_enum], open_h5_props_filespace[vzc_enum],
                              offset_this_snap, nhalos_this_snap, H5T_NATIVE_DOUBLE, buffer + 2*nhalos_this_snap[0]*sizeof(double));
        /* Assign to the appropriate vel array within sage*/
        ASSIGN_BUFFER_TO_NDIM_SAGE(nhalos_this_snap, NDIM, double, Vel, float);

        READ_PARTIAL_1D_ARRAY(fd, "npart", open_h5_dset_props[len_enum], open_h5_props_filespace[len_enum],
                              offset_this_snap, nhalos_this_snap, H5T_NATIVE_ULONG, buffer);
        ASSIGN_BUFFER_TO_SAGE(nhalos_this_snap, int64_t, Len, int32_t);

        READ_PARTIAL_1D_ARRAY(fd, "ID", open_h5_dset_props[mostboundid_enum], open_h5_props_filespace[mostboundid_enum],
                              offset_this_snap, nhalos_this_snap, H5T_NATIVE_LLONG, buffer);
        ASSIGN_BUFFER_TO_SAGE(nhalos_this_snap, int64_t, MostBoundID, long long);

        /* Read in the angular momentum */
        READ_PARTIAL_1D_ARRAY(fd, "Lx", open_h5_dset_props[lx_enum], open_h5_props_filespace[lx_enum],
                              offset_this_snap, nhalos_this_snap, H5T_NATIVE_DOUBLE, buffer);
        READ_PARTIAL_1D_ARRAY(fd, "Ly", open_h5_dset_props[ly_enum], open_h5_props_filespace[ly_enum],
                              offset_this_snap, nhalos_this_snap, H5T_NATIVE_DOUBLE, buffer + nhalos_this_snap[0]*sizeof(double));
        READ_PARTIAL_1D_ARRAY(fd, "Lz", open_h5_dset_props[lz_enum], open_h5_props_filespace[lz_enum],
                              offset_this_snap, nhalos_this_snap, H5T_NATIVE_DOUBLE, buffer + 2*nhalos_this_snap[0]*sizeof(double));
        /* Assign to the appropriate vel array within sage*/
        ASSIGN_BUFFER_TO_NDIM_SAGE(nhalos_this_snap, NDIM, double, Spin, float);

        READ_PARTIAL_1D_ARRAY(fd, "Mass_200mean", open_h5_dset_props[m200b_enum], open_h5_props_filespace[m200b_enum],
                              offset_this_snap, nhalos_this_snap, H5T_NATIVE_DOUBLE, buffer);
        ASSIGN_BUFFER_TO_SAGE(nhalos_this_snap, double, M_Mean200, float);

        READ_PARTIAL_1D_ARRAY(fd, "sigV", open_h5_dset_props[veldisp_enum], open_h5_props_filespace[veldisp_enum],
                              offset_this_snap, nhalos_this_snap, H5T_NATIVE_DOUBLE, buffer);
        ASSIGN_BUFFER_TO_SAGE(nhalos_this_snap, double, VelDisp, float);

        const double scale_factor = run_params->scale_factors[isnap];
        const double hubble_h = run_params->Hubble_h;
        for(hsize_t i=0;i<nhalos_this_snap[0];i++) {
            /* Fill up the remaining properties that are not within the GENESIS dataset */
            local_halos[i].SnapNum = isnap;
            local_halos[i].FileNr = 0;
            local_halos[i].SubhaloIndex = -1;
            local_halos[i].SubHalfMass = -1.0;

            /* Change the conventions across the entire forest to match the SAGE conventions */
            /* convert the masses into 1d10 Msun/h units */
            local_halos[i].M200c *= hubble_h * 1e-10;// M200c is an alias for Mvir
            for(int j=0;j<NDIM;j++) {
                local_halos[i].Pos[j] *= hubble_h / scale_factor;
                local_halos[i].Vel[j] /= scale_factor;
                local_halos[i].Spin[j] *= hubble_h * hubble_h * 1e-10;
            }
        }

        //Done reading all halos belonging to this forest at this snapshot
        local_halos += nhalos_this_snap[0];
    }
    //Done reading all halos belonging to this forest (across all snapshots)
    local_halos -= nhalos;//rewind the local_halos to the first halo in this forest

    //Fix the NextProg, NexthaloinFofgroup mergertree indices and make them forest-local



    //free the allocated memory that only has scope
    //limited to this function
    free(buffer);free(forest_local_offsets);

    return nhalos;
}

#undef READ_PARTIAL_1D_ARRAY
#undef ASSIGN_BUFFER_TO_SAGE
#undef ASSIGN_BUFFER_TO_NDIM_SAGE

void cleanup_forests_io_genesis_hdf5(struct forest_info *forests_info)
{
    struct genesis_info *gen = &(forests_info->gen);

    /* loop over all snapshots and close all datasets at each snapshots */
    for(int64_t i=0;i<gen->maxsnaps;i++) {
        if(gen->open_h5_dset_snapgroups[i] > 0) {
            H5Dclose(gen->open_h5_dset_snapgroups[i]);
        }
    }
    H5Fclose(gen->h5_fd);
    free(gen->open_h5_dset_snapgroups);/* allocated memory for storing the dataset groups */

    /* free up all the memory associated at the forest level*/
    for(int64_t i=0;i<gen->nforests;i++) {
        free(gen->offset_for_forest_per_snap[i]);
        free(gen->nhalos_per_forest_per_snap[i]);
    }

    free(gen->nhalos_per_forest);
    free(gen->offset_for_forest_per_snap);
    free(gen->nhalos_per_forest_per_snap);
}
